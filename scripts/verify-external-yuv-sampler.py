#!/usr/bin/env python3
import os
import re
from pathlib import Path


root = Path(__file__).resolve().parents[1]
source = Path(os.environ.get("LIBPLACEBO_SOURCE", root / "libmpv" / "libplacebo"))
dispatch = (source / "src" / "dispatch.c").read_text(encoding="utf-8")
shaders = (source / "src" / "shaders.c").read_text(encoding="utf-8")

# Since libplacebo 7.387.0, dispatch adds highp to all GLES sampler uniforms.
# The external YUV type must be bare there; the separate function parameter
# generator still needs the explicit qualifier in shaders.c.
if not (
    re.search(r'if \(gpu->glsl.gles\)\s*\{\s*ADD\(pre, "uniform highp %c%s', dispatch)
    and '[PL_SAMPLER_EXTERNAL_YUV][2] = "__samplerExternal2DY2YEXT"' in dispatch
    and '[PL_SAMPLER_EXTERNAL_YUV] = "highp __samplerExternal2DY2YEXT"' in shaders
):
    raise SystemExit("Unexpected GLSL precision for external YUV samplers")

print("External YUV GLSL sampler precision source check passed")
