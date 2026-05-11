#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openFrameworks" >&2
  exit 2
fi

of_root=$1
platform_mk="$of_root/libs/openFrameworksCompiled/project/emscripten/config.emscripten.default.mk"
compile_core_mk="$of_root/libs/openFrameworksCompiled/project/makefileCommon/compile.core.mk"

for path in "$platform_mk" "$compile_core_mk"; do
  if [[ ! -e "$path" ]]; then
    echo "required openFrameworks file is missing: $path" >&2
    exit 1
  fi
done

python3 - "$platform_mk" "$compile_core_mk" <<'PY'
from pathlib import Path
import sys

platform_mk = Path(sys.argv[1])
compile_core_mk = Path(sys.argv[2])

text = platform_mk.read_text()
text = text.replace("PLATFORM_PTHREAD = -pthread -matomics -mbulk-memory", "PLATFORM_PTHREAD =")
text = text.replace("CFLAG_PLATFORM_PTHREAD = -pthread -matomics -mbulk-memory", "CFLAG_PLATFORM_PTHREAD =")
text = text.replace("PLATFORM_LDFLAGS += -s USE_WEBGPU=1", "# Removed for current Emscripten; this sketch targets WebGL, not WebGPU.")
text = text.replace("-sLOAD_SOURCE_MAP=1 ", "")
comment = "# GitHub Pages cannot set cross-origin isolation headers, so keep the web build single-threaded."
if comment not in text:
    text = text.replace("PLATFORM_LDFLAGS +=  $(PLATFORM_PTHREAD)", f"{comment}\nPLATFORM_LDFLAGS +=  $(PLATFORM_PTHREAD)")
platform_mk.write_text(text)

text = compile_core_mk.read_text()
text = text.replace("$(CC) $(OF_CORE_OBJ_FILES) -o $@", "$(CC) -r $(OF_CORE_OBJ_FILES) -o $@")
compile_core_mk.write_text(text)
PY
