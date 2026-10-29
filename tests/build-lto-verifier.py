#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import tempfile
import unittest
import sys

sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location("lto_verify", pathlib.Path(__file__).parents[1] / "scripts/verify-build-lto.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class VerifyLto(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="sweetvideo-lto-verify-")
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.ffmpeg = self.root / "libmpv/ffmpeg/.build"
        self.mpv = self.root / "libmpv/mpv/.build"
        self.dest = self.root / "libmpv/arm64-build"
        for path in (self.ffmpeg / "ffbuild", self.ffmpeg / "libavcodec/hevc",
                     self.mpv / "meson-info", self.dest / "lib"):
            path.mkdir(parents=True, exist_ok=True)
        (self.dest / "libmpv.so").write_bytes(b"fixture-elf-not-a-device-build")
        self.configure()
        self.configure_dependencies()

    def configure(self, thin=True):
        self.config = "CFLAGS=-O3 -fPIC" + (" -flto=thin" if thin else "") + "\n"
        self.config += "LDFLAGS=-fuse-ld=lld" + (" -flto=thin" if thin else "") + "\n"
        self.config += "AR=/sdk/native/llvm/bin/llvm-ar\nRANLIB=/sdk/native/llvm/bin/llvm-ranlib\nNM_CMD=/sdk/native/llvm/bin/llvm-nm\n"
        self.save_config()
        self.options = {"b_lto": thin, "b_lto_mode": "thin" if thin else "default"}
        self.save_options()
        (self.mpv / "build.ninja").write_text(" LINK_ARGS = -shared -flto=thin\n", encoding="utf-8")
        self.save_commands(self.mpv)
        self.bitcode = b"BC\xc0\xdefixture-hevc-bitcode"
        (self.ffmpeg / "libavcodec/hevc/hevcdec.o").write_bytes(self.bitcode)
        self.save_archive(self.bitcode)

    def save_config(self):
        (self.ffmpeg / "ffbuild/config.mak").write_text(self.config, encoding="utf-8")

    def save_options(self):
        (self.mpv / "meson-info/intro-buildoptions.json").write_text(json.dumps(
            [{"name": key, "value": value} for key, value in self.options.items()]), encoding="utf-8")

    def save_archive(self, body, name="libavcodec.a"):
        header = ("hevcdec.o/".ljust(16) + "0".ljust(12) + "0".ljust(6) + "0".ljust(6) +
                  "100644".ljust(8) + str(len(body)).ljust(10) + "`\n").encode("ascii")
        (self.dest / "lib" / name).write_bytes(b"!<arch>\n" + header + body + (b"\n" if len(body) % 2 else b""))

    def save_commands(self, build, commands=None, triple="aarch64-linux-ohos"):
        if commands is None:
            commands = [{"file": "client.c", "command": "clang --target=" + triple + " -O3 -flto=thin -c client.c"}]
        (build / "compile_commands.json").write_text(json.dumps(commands), encoding="utf-8")

    def cmake_cache_fixture(self, newline="\n"):
        # Match CMake's generated format, not just three adjacent records:
        # the old multiline regex consumed blank lines and // help into keys.
        return newline.join((
            "# This is the CMakeCache file.",
            "# KEY:TYPE=VALUE",
            "",
            "########################",
            "# EXTERNAL cache entries",
            "########################",
            "",
            "//Flags used by the C compiler during all build types.",
            "CMAKE_C_FLAGS:STRING=-fPIC -flto=thin",
            "",
            "//Path to a program.",
            "CMAKE_AR:FILEPATH=/sdk/llvm-ar",
            "",
            "//Generate index for LLVM archive.",
            "CMAKE_RANLIB:FILEPATH=/sdk/llvm-ranlib",
            ""))

    def configure_dependencies(self):
        for name, archives in verify.MESON_ARCHIVES.items():
            build = self.root / "libmpv" / name / ".build"
            (build / "meson-info").mkdir(parents=True)
            (build / "meson-info/intro-buildoptions.json").write_text(json.dumps([
                {"name": "b_lto", "value": True}, {"name": "b_lto_mode", "value": "thin"}]), encoding="utf-8")
            self.save_commands(build)
            for archive in archives:
                self.save_archive(self.bitcode, archive)
        for name, archives in verify.CMAKE_ARCHIVES.items():
            build = self.root / "libmpv" / name / ".build"
            build.mkdir(parents=True)
            (build / "CMakeCache.txt").write_text(
                self.cmake_cache_fixture(), encoding="utf-8")
            self.save_commands(build)
            for archive in archives:
                self.save_archive(self.bitcode, archive)
        for name, archives in verify.MAKE_ARCHIVES.items():
            source = self.root / "libmpv" / name
            (source / ".build").mkdir(parents=True)
            (source / ".build/.sweetvideo-make-lto-inputs").write_text(
                "CC=/sdk/clang --target=aarch64-linux-ohos\nCFLAGS=-O2 -flto=thin\nLDFLAGS=-flto=thin -fuse-ld=lld\nAR=/sdk/llvm-ar rcu\nRANLIB=/sdk/llvm-ranlib\n", encoding="utf-8")
            objects = source / ("src" if name == "lua" else "library")
            objects.mkdir()
            (objects / "client.o").write_bytes(self.bitcode)
            for archive in archives:
                self.save_archive(self.bitcode, archive)
        for archive in ("libavformat.a", "libavutil.a", "libswscale.a", "libswresample.a", "libavfilter.a"):
            self.save_archive(self.bitcode, archive)

    def run_verify(self):
        verify.inspect(self.root, self.dest, "arm64")

    def test_thin_report_binds_final_elf(self):
        self.run_verify()
        report = json.loads((self.dest / "build-optimization.json").read_text())
        self.assertEqual(report["schema"], 2)
        self.assertEqual(report["ffmpeg_mpv_lto"], "thin")
        self.assertEqual(report["native_c_cpp_lto"], "thin")
        self.assertIn("dovi_tools", report["exceptions"])
        self.assertEqual(len(report["components"]), 20)
        self.assertEqual(report["components"]["shaderc"]["archives"]["libshaderc_combined.a"]["bitcode_members"], 1)
        self.assertEqual(report["libmpv_sha256"], verify.hashlib.sha256(b"fixture-elf-not-a-device-build").hexdigest())

    def test_non_lto_is_not_silently_accepted(self):
        self.configure(False)
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_missing_ffmpeg_compile_flag(self):
        self.config = self.config.replace("CFLAGS=-O3 -fPIC -flto=thin", "CFLAGS=-O3 -fPIC")
        self.save_config()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_missing_ffmpeg_link_flag(self):
        self.config = self.config.replace("LDFLAGS=-fuse-ld=lld -flto=thin", "LDFLAGS=-fuse-ld=lld")
        self.save_config()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_non_sdk_ar(self):
        self.config = self.config.replace("llvm-ar", "ar")
        self.save_config()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_native_hevc_objects(self):
        (self.ffmpeg / "libavcodec/hevc/hevcdec.o").write_bytes(b"\x7fELFnot-bitcode")
        with self.assertRaises(SystemExit): self.run_verify()

    def test_native_core_cannot_be_masked_by_bitcode_dsp_or_parser(self):
        (self.ffmpeg / "libavcodec/hevc/hevcdec.o").write_bytes(b"\x7fELFstale-decoder-core")
        (self.ffmpeg / "libavcodec/hevc/dsp.o").write_bytes(self.bitcode)
        (self.ffmpeg / "libavcodec/hevc/parser.o").write_bytes(self.bitcode)
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_missing_decoder_core(self):
        (self.ffmpeg / "libavcodec/hevc/hevcdec.o").unlink()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_stale_installed_archive(self):
        self.save_archive(b"BC\xc0\xdeold-other-codec")
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_no_meson_lto(self):
        self.options["b_lto"] = False
        self.save_options()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_full_not_thin(self):
        self.options["b_lto_mode"] = "default"
        self.save_options()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_no_final_link_flag(self):
        (self.mpv / "build.ninja").write_text("LINK_ARGS = -shared\n", encoding="utf-8")
        with self.assertRaises(SystemExit): self.run_verify()

    def test_reject_no_mpv_compile_flag(self):
        self.save_commands(self.mpv, [{"file": "client.c", "command": "clang --target=aarch64-linux-ohos -O3 -c client.c"}])
        with self.assertRaises(SystemExit): self.run_verify()

    def test_every_dependency_must_retain_lto(self):
        for name in verify.MESON_ARCHIVES:
            with self.subTest(name=name):
                build = self.root / "libmpv" / name / ".build"
                self.save_commands(build, [{"file": "client.c", "command": "clang --target=aarch64-linux-ohos -O3 -c client.c"}])
                with self.assertRaises(SystemExit): self.run_verify()
                self.save_commands(build)

    def test_native_installed_archives_are_rejected(self):
        for archive in ("libdav1d.a", "libshaderc_combined.a", "liblua.a", "libmbedcrypto.a", "libavutil.a"):
            with self.subTest(archive=archive):
                self.save_archive(b"\x7fELFnative-only", archive)
                with self.assertRaises(SystemExit): self.run_verify()
                self.save_archive(self.bitcode, archive)

    def test_host_generators_and_handwritten_asm_do_not_need_lto(self):
        self.save_commands(self.mpv, [
            {"file": "client.c", "arguments": ["clang", "--target=aarch64-linux-ohos", "-flto=thin", "-c", "client.c"]},
            {"file": "generator.c", "command": "cc -c generator.c"},
            {"file": "neon.S", "command": "clang --target=aarch64-linux-ohos -c neon.S"}])
        self.run_verify()

    def test_one_good_source_does_not_hide_non_lto_target_source(self):
        self.save_commands(self.mpv, [
            {"file": "good.c", "command": "clang --target=aarch64-linux-ohos -flto=thin -c good.c"},
            {"file": "bad.cpp", "command": "clang++ --target=aarch64-linux-ohos -c bad.cpp"}])
        with self.assertRaises(SystemExit): self.run_verify()

    def test_lto_override_is_rejected(self):
        self.config = self.config.replace("CFLAGS=-O3 -fPIC -flto=thin", "CFLAGS=-O3 -fPIC -flto=thin -flto=full")
        self.save_config()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_clang_final_mode_and_meson_resource_flag(self):
        self.save_commands(self.mpv, [{"file": "client.c", "command":
            "clang --target=aarch64-linux-ohos -flto=full -flto=thin -flto-jobs=4 -c client.c"}])
        self.run_verify()

    def test_wrong_target_commands_are_rejected(self):
        self.save_commands(self.mpv, triple="x86_64-linux-ohos")
        with self.assertRaises(SystemExit): self.run_verify()

    def test_cmake_flags_and_archive_tools_are_verified(self):
        build = self.root / "libmpv/shaderc/.build"
        cache = build / "CMakeCache.txt"
        original = cache.read_text()
        for old, new in (("-flto=thin", ""), ("llvm-ar", "ar"), ("llvm-ranlib", "ranlib")):
            with self.subTest(old=old):
                cache.write_text(original.replace(old, new))
                with self.assertRaises(SystemExit): self.run_verify()
                cache.write_text(original)

    def test_real_cmake_comments_and_blank_lines_accept_lf_and_crlf(self):
        expected = {"CMAKE_C_FLAGS": "-fPIC -flto=thin",
                    "CMAKE_AR": "/sdk/llvm-ar", "CMAKE_RANLIB": "/sdk/llvm-ranlib"}
        for newline in ("\n", "\r\n"):
            with self.subTest(newline=repr(newline)):
                text = self.cmake_cache_fixture(newline)
                self.assertEqual(verify.parse_cmake_cache(text), expected)
                # Exercise the file reader and the full verification path too.
                for name in verify.CMAKE_ARCHIVES:
                    cache = self.root / "libmpv" / name / ".build/CMakeCache.txt"
                    cache.write_bytes(text.encode("utf-8"))
                self.run_verify()

    def test_cmake_cache_preserves_empty_values_and_value_equals(self):
        text = ("# Entries below contain values, not further records.\n\n"
                "CMAKE_C_FLAGS:STRING=-DPAIR=left=right -flto=thin\n"
                "EMPTY:STRING=\nURL:STRING=https://example.invalid/path?a=b=c\n")
        parsed = verify.parse_cmake_cache(text)
        self.assertEqual(parsed["CMAKE_C_FLAGS"], "-DPAIR=left=right -flto=thin")
        self.assertEqual(parsed["EMPTY"], "")
        self.assertEqual(parsed["URL"], "https://example.invalid/path?a=b=c")
        verify.require_thin(parsed["CMAKE_C_FLAGS"], "fixture")

    def test_cmake_comment_examples_cannot_supply_missing_required_keys(self):
        build = self.root / "libmpv/nghttp3/.build"
        cache = build / "CMakeCache.txt"
        entries = (("CMAKE_C_FLAGS:STRING=-fPIC -flto=thin", "CMAKE_C_FLAGS",
                    "nghttp3 CMake cache is missing CMAKE_C_FLAGS"),
                   ("CMAKE_AR:FILEPATH=/sdk/llvm-ar", "CMAKE_AR",
                    "nghttp3 must archive with SDK llvm-ar"),
                   ("CMAKE_RANLIB:FILEPATH=/sdk/llvm-ranlib", "CMAKE_RANLIB",
                    "nghttp3 must archive with SDK llvm-ranlib"))
        for entry, key, message in entries:
            with self.subTest(key=key):
                text = ("\n# " + entry + "\n\n  // " + entry + "\n\n" +
                        self.cmake_cache_fixture().replace(entry + "\n", ""))
                self.assertNotIn(key, verify.parse_cmake_cache(text))
                cache.write_text(text, encoding="utf-8")
                with self.assertRaises(SystemExit) as error:
                    verify.verify_cmake(build, "nghttp3", "aarch64-linux-ohos")
                self.assertEqual(str(error.exception), message)

    def test_cmake_empty_required_values_fail_for_the_actual_reason(self):
        build = self.root / "libmpv/nghttp3/.build"
        cache = build / "CMakeCache.txt"
        for value, key, message in (
                ("-fPIC -flto=thin", "CMAKE_C_FLAGS", "nghttp3 CMake C flags lost ThinLTO or overrides it"),
                ("/sdk/llvm-ar", "CMAKE_AR", "nghttp3 must archive with SDK llvm-ar"),
                ("/sdk/llvm-ranlib", "CMAKE_RANLIB", "nghttp3 must archive with SDK llvm-ranlib")):
            with self.subTest(key=key):
                text = self.cmake_cache_fixture().replace("=" + value + "\n", "=\n")
                self.assertEqual(verify.parse_cmake_cache(text)[key], "")
                cache.write_text(text, encoding="utf-8")
                with self.assertRaises(SystemExit) as error:
                    verify.verify_cmake(build, "nghttp3", "aarch64-linux-ohos")
                self.assertEqual(str(error.exception), message)

    def test_cmake_invalid_values_are_not_masked_by_valid_comment_examples(self):
        build = self.root / "libmpv/nghttp3/.build"
        cache = build / "CMakeCache.txt"
        for old, new, message in (
                ("-flto=thin", "-flto=full", "nghttp3 CMake C flags lost ThinLTO or overrides it"),
                ("-flto=thin", "-flto=thin -fno-lto", "nghttp3 CMake C flags lost ThinLTO or overrides it"),
                ("/sdk/llvm-ar", "/usr/bin/ar", "nghttp3 must archive with SDK llvm-ar"),
                ("/sdk/llvm-ranlib", "/usr/bin/ranlib", "nghttp3 must archive with SDK llvm-ranlib")):
            with self.subTest(new=new):
                text = self.cmake_cache_fixture().replace(old, new)
                text += ("\n//CMAKE_C_FLAGS:STRING=-flto=thin\n"
                         "# CMAKE_AR:FILEPATH=/sdk/llvm-ar\n"
                         "//CMAKE_RANLIB:FILEPATH=/sdk/llvm-ranlib\n")
                cache.write_text(text, encoding="utf-8")
                with self.assertRaises(SystemExit) as error:
                    verify.verify_cmake(build, "nghttp3", "aarch64-linux-ohos")
                self.assertEqual(str(error.exception), message)

    def test_cmake_valid_cache_cannot_hide_non_lto_compile_command(self):
        build = self.root / "libmpv/nghttp3/.build"
        self.save_commands(build, [{"file": "client.c", "command":
            "clang --target=aarch64-linux-ohos -O3 -fno-lto -c client.c"}])
        with self.assertRaises(SystemExit) as error:
            verify.verify_cmake(build, "nghttp3", "aarch64-linux-ohos")
        self.assertEqual(str(error.exception),
                         "nghttp3 compile command for client.c lost ThinLTO or overrides it")

    def test_make_effective_flags_are_verified(self):
        inputs = self.root / "libmpv/lua/.build/.sweetvideo-make-lto-inputs"
        original = inputs.read_text()
        for old, new in (("CFLAGS=-O2 -flto=thin", "CFLAGS=-O2"),
                         ("LDFLAGS=-flto=thin", "LDFLAGS="), ("llvm-ar", "ar")):
            with self.subTest(old=old):
                inputs.write_text(original.replace(old, new))
                with self.assertRaises(SystemExit): self.run_verify()
                inputs.write_text(original)

    def test_make_stale_native_object_is_rejected(self):
        obj = self.root / "libmpv/mbedtls/library/stale.o"
        obj.write_bytes(b"\x7fELFstale-native-object")
        with self.assertRaises(SystemExit): self.run_verify()

    def test_missing_dependency_archive_is_rejected(self):
        (self.dest / "lib/libplacebo.a").unlink()
        with self.assertRaises(SystemExit): self.run_verify()

    def test_x86_64_scope(self):
        for build in (self.mpv, *(self.root / "libmpv" / name / ".build" for name in
                                      (*verify.MESON_ARCHIVES, *verify.CMAKE_ARCHIVES))):
            self.save_commands(build, triple="x86_64-linux-ohos")
        for name in verify.MAKE_ARCHIVES:
            inputs = self.root / "libmpv" / name / ".build/.sweetvideo-make-lto-inputs"
            inputs.write_text(inputs.read_text().replace("aarch64-linux-ohos", "x86_64-linux-ohos"))
        verify.inspect(self.root, self.dest, "x86_64")

    def test_truncated_archive_is_rejected(self):
        archive = self.dest / "lib/libdav1d.a"
        archive.write_bytes(archive.read_bytes()[:-7])
        with self.assertRaises(SystemExit): self.run_verify()

if __name__ == "__main__":
    unittest.main()
