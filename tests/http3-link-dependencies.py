"""Exercise the real FFmpeg dependency flattener; no target build is claimed."""
import pathlib
import re
import subprocess
import sys
import tempfile

source = pathlib.Path(sys.argv[1])
configure = (source / "configure").read_text(encoding="utf-8")
functions = []
for name in ("filter", "append", "reverse", "unique", "resolve",
             "flatten_extralibs", "flatten_extralibs_wrapper"):
    match = re.search(r"^" + name + r"\s*\(\)\s*\{\n.*?^\}", configure, re.M | re.S)
    if not match:
        raise AssertionError(f"Missing FFmpeg helper: {name}")
    functions.append(match.group())
declaration = re.search(r'^avformat_extralibs="([^"]*)"$', configure, re.M)
if not declaration or "ohos_http3_extralibs" not in declaration[1].split():
    raise AssertionError("avformat must export ohos_http3_extralibs")

for enabled in (False, True):
    script = 'set -e\n' + "\n".join(functions) + '\nldflags_filter=echo\n'
    script += 'pthreads_extralibs=-pthread\nlibdl_extralibs=-ldl\n'
    script += 'mbedtls_extralibs="-lmbedtls -lmbedx509 -lmbedcrypto"\n'
    if enabled:
        # The closure stored by require_pkg_config + check_deps when enabled.
        script += ('ohos_http3_extralibs="-L/static/lib -lnghttp3 '
                   'pthreads_extralibs libdl_extralibs mbedtls_extralibs"\n')
    script += declaration.group() + '\n'
    script += 'libs=$(flatten_extralibs_wrapper avformat_extralibs)\nprintf "%s\\n" "$libs"\n'
    result = subprocess.run(["bash", "-c", script], check=True,
                            capture_output=True, text=True).stdout.split()
    if enabled:
        assert result == ["-L/static/lib", "-lnghttp3", "-pthread", "-ldl",
                          "-lmbedtls", "-lmbedx509", "-lmbedcrypto"], result
    else:
        assert not result, result
    # Run FFmpeg's actual pkg-config generator, without copying its logic.
    with tempfile.TemporaryDirectory(prefix="http3-pc-") as directory:
        stage = pathlib.Path(directory)
        (stage / "ffbuild").mkdir()
        (stage / "libavformat").mkdir()
        (stage / "libavformat/libavformat.version").write_text(
            "libavformat_VERSION=62.0.0\n", encoding="utf-8")
        (stage / "ffbuild/config.sh").write_text(
            'shared=no\nprefix=/static\nlibdir=/static/lib\nincdir=/static/include\n'
            'source_path=/source\nbuild_suffix=\navformat_deps=\nrpath=\n'
            'extralibs_avformat="' + " ".join(result) + '"\n', encoding="utf-8")
        subprocess.run(["bash", str((source / "ffbuild/pkgconfig_generate.sh").resolve()),
                        "avformat", "FFmpeg container library"], cwd=stage, check=True)
        pc = (stage / "libavformat/libavformat.pc").read_text(encoding="utf-8")
        libs = re.search(r"^Libs: (.*)$", pc, re.M)[1].split()
        assert ("-lnghttp3" in libs) == enabled, pc
print("FFmpeg static HTTP/3 dependency closure and pkg-config: enabled/disabled passed")
