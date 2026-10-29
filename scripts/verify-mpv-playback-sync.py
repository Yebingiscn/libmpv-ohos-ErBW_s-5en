#!/usr/bin/env python3
"""Host-test actual patched mpv track-sync code; no target build is required.

Usage: python3 scripts/verify-mpv-playback-sync.py [path/to/patched/mpv]
Set CC to a host C compiler (an executable path, or a quoted command with flags).
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import shutil
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^(?:static\s+)?void\s+" + re.escape(name)
                      + r"\([^;{]*\)\s*\{[\s\S]*?^\}", source, re.M)
    if not match:
        raise SystemExit("Missing patched function: " + name)
    return match.group(0) + "\n"


def require(condition, message):
    if not condition:
        raise SystemExit("Source regression: " + message)


def compiler_command():
    value = os.environ.get("CC", "cc")
    # An unquoted executable path with spaces is convenient on Windows.
    if Path(value).is_file():
        return [value]
    args = shlex.split(value, posix=os.name != "nt")
    return [part.strip('"') if os.name == "nt" else part for part in args]


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mpv", nargs="?", type=Path, default=root / "libmpv/mpv")
    args = parser.parse_args()
    wrapper = (args.mpv / "filters/f_decoder_wrapper.c").read_text(encoding="utf-8")
    header = (args.mpv / "filters/f_decoder_wrapper.h").read_text(encoding="utf-8")
    video = (args.mpv / "player/video.c").read_text(encoding="utf-8")

    setter = function(wrapper, "mp_decoder_wrapper_set_playback_sync")
    require("mp_decoder_wrapper_set_playback_sync" in header, "public setter declaration")
    reset = function(wrapper, "decf_reset")
    require(re.search(r"mp_mutex_lock\(&p->cache_lock\);[\s\S]*?"
                      r"p->playback_sync\s*=\s*false;[\s\S]*?"
                      r"mp_mutex_unlock\(&p->cache_lock\);", reset), "reset clears sync under lock")
    reset_video = function(video, "reset_video_state")
    require("mp_decoder_wrapper_set_playback_sync(t->dec, false)" in reset_video,
            "video reset clears sync before reinitialization")
    write = function(video, "write_video")
    require(write.index("update_playback_sync(mpctx)") < write.index("STATUS_READY"),
            "sync update precedes READY/paused early return")
    eof = write[write.index("if (r == VD_EOF)"):]
    require(eof.index("mp_decoder_wrapper_set_playback_sync(track->dec, false)")
            < eof.index("check_for_hwdec_fallback"), "EOF clears demand before early returns")
    ready = write[write.index("mpctx->video_status = STATUS_READY;"):]
    require(ready.index("update_playback_sync(mpctx)") < ready.index("vo_wait_frame"),
            "submission clears demand on entering READY")
    # A changing playback_pts already drives the existing display-side catchup.
    output = video[video.index("static int video_output_image"):]
    output = output[:output.index("\n}\n")]
    require("hrseek_pts = mpctx->playback_pts;" in output,
            "track synchronization keeps using the moving playback position")

    extracted = "\n".join((function(wrapper, "update_decode_hints"), setter,
        function(wrapper, "decf_process"), function(video, "update_playback_sync")))
    compiler = compiler_command()
    if not compiler or not (Path(compiler[0]).is_file() or shutil.which(compiler[0])):
        raise SystemExit("Host C compiler unavailable; set CC to a host compiler")
    env = os.environ.copy()
    if Path(compiler[0]).is_file():
        env["PATH"] = str(Path(compiler[0]).resolve().parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="mpv-playback-sync-") as temporary:
        check = Path(temporary)
        (check / "mpv-playback-sync.inc").write_text(extracted, encoding="utf-8")
        binary = check / ("sync-test.exe" if os.name == "nt" else "sync-test")
        subprocess.run(compiler + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(check), str(root / "tests/mpv-playback-sync.c"), "-o", str(binary)],
            check=True, env=env)
        subprocess.run([str(binary)], check=True, env=env)
    print("mpv playback sync: reset/EOF/READY and moving-target source checks passed")


if __name__ == "__main__":
    main()
