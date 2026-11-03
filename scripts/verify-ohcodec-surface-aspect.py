#!/usr/bin/env python3
"""Host-test the patched OHCodec Surface preparation and resize path.

Usage: python3 scripts/verify-ohcodec-surface-aspect.py [path/to/patched/mpv]
Set CC to a host C compiler, optionally followed by target or linker flags.
This extracts the production implementation; recording mocks replace only the
platform/device operations. It does not claim to verify physical VPE output.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import shutil
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise SystemExit("Source regression: " + message)


def function(source, name):
    match = re.search(r"^(?:static\s+)?(?:[\w*]+\s+)+\**"
                      + re.escape(name) + r"\([^;{]*\)\s*\{[\s\S]*?^\}",
                      source, re.M)
    require(match, "missing production function " + name)
    return match.group(0) + "\n"


def structure(source, name):
    match = re.search(r"^struct " + re.escape(name) + r"\s*\{[\s\S]*?^\};",
                      source, re.M)
    require(match, "missing production structure " + name)
    return match.group(0) + "\n"


def compiler_command():
    value = os.environ.get("CC", "cc")
    if Path(value).is_file():
        return [value]
    args = shlex.split(value, posix=os.name != "nt")
    return [arg.strip('"') if os.name == "nt" else arg for arg in args]


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mpv", nargs="?", type=Path, default=root / "libmpv/mpv")
    args = parser.parse_args()
    surface = (args.mpv / "video/out/vo_ohcodec.c").read_text(encoding="utf-8")
    decoder = (args.mpv / "video/decode/vd_lavc.c").read_text(encoding="utf-8")
    header = (args.mpv / "video/out/vo.h").read_text(encoding="utf-8")
    image = (args.mpv / "video/mp_image.c").read_text(encoding="utf-8")

    # VPE output DAR is fixed before opening OHCodec, not derived from a remembered
    # 4:3/stretched display Surface or changed after decoded buffers are live.
    prepare = function(surface, "prepare_decoder")
    resize = function(surface, "resize")
    require("ohos_vpe_resize" not in resize,
            "display resize must not mutate VPE buffer geometry")
    require("VOCTRL_PREPARE_DECODER" in header, "public decoder preparation request")
    control = function(surface, "control")
    require(re.search(r"case VOCTRL_PREPARE_DECODER\s*:[\s\S]*?prepare_decoder\(", control),
            "Surface VO dispatches decoder preparation")
    start = decoder.index("mp_set_avctx_codec_headers(avctx, c)")
    opened = decoder.index("avcodec_open2(avctx, lavc_codec, NULL)", start)
    require("VOCTRL_PREPARE_DECODER" in decoder[start:opened],
            "Surface preparation follows stream headers and precedes decoder open")
    decoder_prepare = re.search(
        r"^    if \(ctx->vo && avctx->hw_device_ctx[\s\S]*?^    \}",
        decoder[start:opened], re.M)
    require(decoder_prepare and "VOCTRL_PREPARE_DECODER" in decoder_prepare.group(0),
            "decoder preparation retains the hardware/device/VO guard")
    for path in (args.mpv / "video/out").rglob("*.c"):
        if path.name != "vo_ohcodec.c":
            require("VOCTRL_PREPARE_DECODER" not in path.read_text(encoding="utf-8"),
                    "GPU/Buffer VO must not implement Surface-only preparation")

    extracted = "\n".join((function(image, "mp_image_crop_valid"),
        function(image, "mp_image_params_get_dsize"),
        structure(header, "voctrl_prepare_decoder"),
        structure(surface, "ohcodec_output"),
        function(surface, "free_ohcodec_device"),
        function(surface, "create_ohcodec_device_ref"), prepare,
        function(surface, "get_video_surface_size"),
        function(surface, "update_video_scaling_mode"), resize,
        function(surface, "frame_uses_current_device"),
        function(surface, "flip_page"),
        "static int decoder_before_open(struct decoder_context *ctx, "
        "AVCodecContext *avctx, const AVCodec *lavc_codec, "
        "const struct mp_codec_params *c)\n{\n"
        + decoder_prepare.group(0) + "\n    return 1;\nerror:\n    return -1;\n}\n"))
    compiler = compiler_command()
    require(compiler and (Path(compiler[0]).is_file() or shutil.which(compiler[0])),
            "host C compiler unavailable; set CC")
    env = os.environ.copy()
    if Path(compiler[0]).is_file():
        env["PATH"] = str(Path(compiler[0]).resolve().parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="ohcodec-surface-aspect-") as temporary:
        check = Path(temporary)
        (check / "ohcodec-surface-aspect.inc").write_text(extracted, encoding="utf-8")
        binary = check / ("aspect-test.exe" if os.name == "nt" else "aspect-test")
        subprocess.run(compiler + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(check), str(root / "tests/ohcodec-surface-aspect.c"),
            "-o", str(binary)], check=True, env=env)
        subprocess.run([str(binary)], check=True, env=env)
    print("OHCodec Surface aspect: production geometry/lifecycle and decoder ordering passed")


if __name__ == "__main__":
    main()
