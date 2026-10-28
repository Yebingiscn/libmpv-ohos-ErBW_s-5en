#!/usr/bin/env python3
"""Check effective build inputs, not just the requested LTO environment flag."""
import hashlib
import json
import pathlib
import re
import shlex
import sys


MESON_ARCHIVES = {
    "dav1d": ("libdav1d.a",), "libdvdcss": ("libdvdcss.a",),
    "libdvdread": ("libdvdread.a",), "libdvdnav": ("libdvdnav.a",),
    "libxml2": ("libxml2.a",), "fribidi": ("libfribidi.a",),
    "freetype": ("libfreetype.a",), "harfbuzz": ("libharfbuzz.a",),
    "fontconfig": ("libfontconfig.a",), "libass": ("libass.a",),
    "libbluray": ("libbluray.a",), "lcms": ("liblcms2.a",),
    "libplacebo": ("libplacebo.a",),
}
CMAKE_ARCHIVES = {
    "nghttp3": ("libnghttp3.a",), "libarchive": ("libarchive.a",),
    "shaderc": ("libshaderc_combined.a",),
}
MAKE_ARCHIVES = {
    "lua": ("liblua.a",),
    "mbedtls": ("libmbedcrypto.a", "libmbedx509.a", "libmbedtls.a"),
}
TRIPLES = {"arm64": "aarch64-linux-ohos", "x86_64": "x86_64-linux-ohos"}


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def variable(text, name):
    match = re.search(r"^" + re.escape(name) + r"\s*[:?+]?=\s*(.*)$", text, re.M)
    require(match, "Missing effective build variable: " + name)
    return match.group(1)


def archive_objects(archive):
    """Inspect installed regular archives, allowing native assembly members."""
    require(archive.is_file(), "Missing installed static archive: " + str(archive))
    result = {"bitcode_members": 0, "native_members": 0, "hashes": set()}
    with archive.open("rb") as stream:
        require(stream.read(8) == b"!<arch>\n", "Expected regular static archive: " + str(archive))
        archive_size = archive.stat().st_size
        while True:
            header = stream.read(60)
            if not header:
                return result
            require(len(header) == 60 and header[-2:] == b"`\n", "Invalid archive header: " + str(archive))
            size = int(header[48:58].decode("ascii").strip())
            require(size >= 0, "Invalid archive member size")
            start = stream.tell()
            require(start + size <= archive_size, "Truncated archive member: " + str(archive))
            # LLVM's ELF ar normally uses GNU names. Handle BSD extended names
            # too, so an archive's filename prefix cannot mask actual bitcode.
            name = header[:16].decode("ascii").strip()
            prefix = int(name[3:]) if name.startswith("#1/") else 0
            require(0 <= prefix <= size, "Invalid archive member filename")
            stream.seek(start + prefix)
            magic = stream.read(min(size - prefix, 4))
            if magic == b"BC\xc0\xde":
                digest = hashlib.sha256(magic)
                remaining = size - prefix - 4
                while remaining:
                    chunk = stream.read(min(remaining, 65536))
                    require(chunk, "Archive changed while verifying: " + str(archive))
                    digest.update(chunk)
                    remaining -= len(chunk)
                result["bitcode_members"] += 1
                result["hashes"].add(digest.hexdigest())
            elif magic == b"\x7fELF":
                result["native_members"] += 1
            stream.seek(start + size + size % 2)


def archive_has_object(archive, hashes):
    return bool(archive_objects(archive)["hashes"] & hashes)


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                return digest.hexdigest()
            digest.update(chunk)


def tokens(command):
    return shlex.split(command)


def require_thin(flags, context):
    args = tokens(flags) if isinstance(flags, str) else flags
    # Clang uses the last LTO mode. Meson also emits -flto-jobs=N at link
    # time; that is a resource bound, not an override of -flto=thin.
    modes = [arg for arg in args if arg in ("-flto", "-fno-lto") or
             arg.startswith(("-flto=", "-fno-lto="))]
    require(modes and modes[-1] == "-flto=thin", context + " lost ThinLTO or overrides it")


def verify_commands(build, name, triple):
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    checked = 0
    for item in commands:
        source = pathlib.Path(item.get("file", ""))
        if source.suffix not in (".c", ".cc", ".cpp", ".cxx", ".C"):
            continue  # Hand-written assembler is not LLVM IR.
        args = item.get("arguments") or tokens(item.get("command", ""))
        if "--target=" + triple not in args and not any(
                args[index:index + 2] == ["--target", triple] for index in range(len(args))):
            continue  # A Meson/CMake build-machine generator, not OHOS code.
        require_thin(args, name + " compile command for " + str(source))
        checked += 1
    require(checked, name + " has no verifiable OHOS C/C++ compile commands")
    return checked


def verify_meson(build, name, triple, final_link=False):
    options = {item["name"]: item["value"] for item in json.loads(
        (build / "meson-info/intro-buildoptions.json").read_text(encoding="utf-8"))}
    require(options.get("b_lto") is True and options.get("b_lto_mode") == "thin",
            name + " effective Meson options lost ThinLTO")
    if final_link:
        ninja = (build / "build.ninja").read_text(encoding="utf-8")
        require(re.search(r"^\s*LINK_ARGS\s*=.*(?:^|\s)-flto=thin(?:\s|$)", ninja, re.M),
                name + " final link command lost ThinLTO")
    return verify_commands(build, name, triple)


def verify_cmake(build, name, triple):
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    options = dict(re.findall(r"^([^#/:][^:=]*):[^=]*=(.*)$", cache, re.M))
    require_thin(options.get("CMAKE_C_FLAGS", ""), name + " CMake C flags")
    for key, tool in (("CMAKE_AR", "llvm-ar"), ("CMAKE_RANLIB", "llvm-ranlib")):
        require(tool in options.get(key, ""), name + " must archive with SDK " + tool)
    # Inspect actual per-source commands as project flags can override cache.
    return verify_commands(build, name, triple)


def verify_make(source, name, triple):
    inputs = (source / ".build/.sweetvideo-make-lto-inputs").read_text(encoding="utf-8")
    require(triple in variable(inputs, "CC"), name + " Make compiler has wrong target")
    require_thin(variable(inputs, "CFLAGS"), name + " effective Make CFLAGS")
    require_thin(variable(inputs, "LDFLAGS"), name + " effective Make LDFLAGS")
    require("-fuse-ld=lld" in tokens(variable(inputs, "LDFLAGS")), name + " Make linker is not LLD")
    for key, tool in (("AR", "llvm-ar"), ("RANLIB", "llvm-ranlib")):
        require(tool in variable(inputs, key), name + " Make must use SDK " + tool)
    objects = list((source / ("src" if name == "lua" else "library")).glob("*.o"))
    require(objects, name + " has no compiled library objects")
    for obj in objects:
        with obj.open("rb") as stream:
            require(stream.read(4) == b"BC\xc0\xde", name + " contains non-bitcode C object: " + str(obj))
    return len(objects)


def verify_archives(dest, names):
    result = {}
    for name in names:
        archive = dest / "lib" / name
        summary = archive_objects(archive)
        require(summary["bitcode_members"], "Installed archive has no LLVM bitcode: " + name)
        del summary["hashes"]
        # Bind evidence to exactly the installed input consumed by final LLD.
        summary["sha256"] = file_sha256(archive)
        result[name] = summary
    return result


def inspect(root, dest, arch):
    require(arch in TRIPLES, "Unsupported ThinLTO verification architecture: " + arch)
    triple = TRIPLES[arch]
    ffmpeg = root / "libmpv/ffmpeg/.build"
    mpv = root / "libmpv/mpv/.build"
    config = (ffmpeg / "ffbuild/config.mak").read_text(encoding="utf-8")
    flags = variable(config, "CFLAGS")
    ldflags = variable(config, "LDFLAGS")
    bitcode = []
    bitcode_hashes = set()
    require_thin(flags, "FFmpeg effective CFLAGS")
    require_thin(ldflags, "FFmpeg effective LDFLAGS")
    require("-fuse-ld=lld" in ldflags.split(), "FFmpeg link is not using SDK LLD")
    for name, tool in (("AR", "llvm-ar"), ("RANLIB", "llvm-ranlib"), ("NM_CMD", "llvm-nm")):
        require(tool in variable(config, name), "FFmpeg must use SDK " + tool)
    components = {"mpv": {"build_system": "meson", "target_compile_commands":
        verify_meson(mpv, "mpv", triple, final_link=True)}}
    # Pinned n8.1.2 decoder core, not merely a parser/DSP sibling that may have
    # been rebuilt while an old native hevcdec.o remains in the tree/archive.
    core = ffmpeg / "libavcodec/hevc/hevcdec.o"
    require(core.is_file(), "Missing HEVC decoder core object")
    core_bytes = core.read_bytes()
    require(core_bytes[:4] == b"BC\xc0\xde", "HEVC decoder core is not bitcode")
    bitcode.append(str(core.relative_to(ffmpeg)))
    bitcode_hashes.add(hashlib.sha256(core_bytes).hexdigest())
    # Verify the installed archive too: it is what the final link consumes.
    archive = dest / "lib/libavcodec.a"
    require(archive_has_object(archive, bitcode_hashes),
            "Installed libavcodec.a does not contain verified HEVC bitcode; stale archive?")
    components["ffmpeg"] = {"build_system": "configure/make", "archives":
        verify_archives(dest, ("libavcodec.a", "libavformat.a", "libavutil.a", "libswscale.a", "libswresample.a", "libavfilter.a"))}
    for name, archives in MESON_ARCHIVES.items():
        build = root / "libmpv" / name / ".build"
        components[name] = {"build_system": "meson", "target_compile_commands":
            verify_meson(build, name, triple), "archives": verify_archives(dest, archives)}
    for name, archives in CMAKE_ARCHIVES.items():
        build = root / "libmpv" / name / ".build"
        components[name] = {"build_system": "cmake", "target_compile_commands":
            verify_cmake(build, name, triple), "archives": verify_archives(dest, archives)}
    for name, archives in MAKE_ARCHIVES.items():
        components[name] = {"build_system": "make", "bitcode_objects":
            verify_make(root / "libmpv" / name, name, triple), "archives": verify_archives(dest, archives)}
    library = dest / "libmpv.so"
    # This sidecar binds the verified inputs to one exact final ELF, not all
    # future libraries or an application's already-installed binary.
    report = {"schema": 2, "architecture": arch, "ffmpeg_mpv_lto": "thin",
              "native_c_cpp_lto": "thin", "components": components,
              "exceptions": {"dovi_tools": "Rust staticlib; separate toolchain, no cross-language LTO",
                             "assembly": "Hand-written machine code is not LLVM IR",
                             "system_libraries": "SDK/prebuilt libraries are not rebuilt"},
              "hevc_bitcode_objects": bitcode,
              "libmpv_sha256": file_sha256(library)}
    (dest / "build-optimization.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Effective native C/C++ ThinLTO verification passed (" + str(len(components)) + " components)")


if __name__ == "__main__":
    require(len(sys.argv) == 4, "Usage: verify-build-lto.py ROOT DEST ARCH")
    inspect(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3])
