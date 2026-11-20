"""Smoke-test the backport with FFmpeg FATE's synthetic 2ch-dsd_5ch-dst.iso.

Usage: python tests/sacd-image.py <patched-ffprobe> <fate-sample.iso>
Sample: https://fate-suite.ffmpeg.org/sacd/2ch-dsd_5ch-dst.iso
"""

import hashlib
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


probe, sample = map(lambda arg: Path(arg).resolve(), sys.argv[1:])
image = sample.read_bytes()
assert hashlib.sha256(image).hexdigest() == (
    "4f770c9dd93fcade83c2f159a765d6ac3ee4e0e4e820701a23b7a927cd5fb2a8"
), "expected the synthetic FFmpeg FATE sample"


def run(path, *options, check=True):
    # ffprobe's 1 MiB default misses the raw-sector TOC, which is just over 1 MiB.
    # MPV's own lavf probe allows up to 10 MiB.
    result = subprocess.run(
        [str(probe), "-v", "error", "-formatprobesize", "2M", "-of", "json", *options, str(path)],
        capture_output=True, text=True, timeout=20,
    )
    if check and result.returncode:
        raise RuntimeError(result.stderr)
    return result


def check_image(path):
    metadata = json.loads(run(path, "-show_entries",
        "format=format_name:stream=index,codec_name,channels:"
        "stream_disposition=default:chapter=start_time:chapter_tags=title").stdout)
    assert metadata["format"]["format_name"] == "sacd"
    streams = metadata["streams"]
    assert [(s["codec_name"], s["channels"], s["disposition"]["default"])
            for s in streams] == [("dsd_msbf", 2, 1), ("dst", 5, 0)]
    chapters = metadata["chapters"]
    assert [c["tags"]["title"] for c in chapters] == ["Tone 1 kHz", "Tone 500 Hz"]
    assert float(chapters[0]["start_time"]) == 0 < float(chapters[1]["start_time"])

    data = json.loads(run(path, "-read_intervals", "0.4%+0.05",
        "-show_entries", "packet=stream_index,pts_time,size:frame=channels,nb_samples").stdout)
    packets = data["packets_and_frames"]
    for index, channels in enumerate((2, 5)):
        first_packet = next(p for p in packets if p.get("stream_index") == index)
        assert float(first_packet["pts_time"]) == 0.4 and int(first_packet["size"]) > 0
        assert any(f.get("channels") == channels and f["nb_samples"] == 4704 for f in packets)


check_image(sample)
with TemporaryDirectory() as directory:
    raw = Path(directory) / "raw-sectors.iso"
    assert len(image) % 2048 == 0
    raw.write_bytes(b"".join(bytes(12) + image[i:i + 2048] + bytes(4)
                             for i in range(0, len(image), 2048)))
    check_image(raw)

    truncated = Path(directory) / "truncated.iso"
    truncated.write_bytes(image[:512 * 2048])
    assert run(truncated, "-show_format", check=False).returncode != 0

print("PASS: SACD 2048/2064-byte images, stereo default, chapters, DSD/DST decode, seek and truncated input")
