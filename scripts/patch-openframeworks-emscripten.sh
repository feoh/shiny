#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/openFrameworks" >&2
  exit 2
fi

of_root=$1
platform_mk="$of_root/libs/openFrameworksCompiled/project/emscripten/config.emscripten.default.mk"
compile_core_mk="$of_root/libs/openFrameworksCompiled/project/makefileCommon/compile.core.mk"
sound_cpp="$of_root/addons/ofxEmscripten/src/ofxEmscriptenSoundStream.cpp"
tess_lib="$of_root/libs/tess2/lib/emscripten/WASM/libtess2.a"

for path in "$platform_mk" "$compile_core_mk" "$sound_cpp" "$tess_lib"; do
  if [[ ! -e "$path" ]]; then
    echo "required openFrameworks file is missing: $path" >&2
    exit 1
  fi
done

python3 - "$platform_mk" "$compile_core_mk" "$sound_cpp" "$tess_lib" <<'PY'
from pathlib import Path
import sys

platform_mk = Path(sys.argv[1])
compile_core_mk = Path(sys.argv[2])
sound_cpp = Path(sys.argv[3])
tess_lib = Path(sys.argv[4])

text = platform_mk.read_text()
text = text.replace("PLATFORM_PTHREAD = -pthread -matomics -mbulk-memory", "PLATFORM_PTHREAD =")
text = text.replace("CFLAG_PLATFORM_PTHREAD = -pthread -matomics -mbulk-memory", "CFLAG_PLATFORM_PTHREAD =")
comment = "# GitHub Pages cannot set cross-origin isolation headers, so keep the web build single-threaded."
if comment not in text:
    text = text.replace("PLATFORM_LDFLAGS +=  $(PLATFORM_PTHREAD)", f"{comment}\nPLATFORM_LDFLAGS +=  $(PLATFORM_PTHREAD)")
platform_mk.write_text(text)

sound_obj = "$(OF_CORE_OBJ_OUTPUT_PATH)addons/ofxEmscripten/src/ofxEmscriptenSoundStream.o"
tess = str(tess_lib)
text = compile_core_mk.read_text()
if sound_obj not in text:
    marker = "else ifeq ($(BYTECODECORE),1)\n$(TARGET) : $(OF_CORE_OBJ_FILES) $(OF_CORE_OBJ_OUTPUT_PATH).compiler_flags\n"
    replacement = (
        "else ifeq ($(BYTECODECORE),1)\n"
        f"{sound_obj}: {sound_cpp}\n"
        "\t@echo \"Compiling $<\"\n"
        "\t@mkdir -p $(@D)\n"
        "\t$(CXX) $(OPTIMIZATION_CFLAGS) $(CFLAGS) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -MT$@ -o $@ -c $<\n"
        f"$(TARGET) : $(OF_CORE_OBJ_FILES) {sound_obj} $(OF_CORE_OBJ_OUTPUT_PATH).compiler_flags\n"
    )
    text = text.replace(marker, replacement)
text = text.replace("$(CC) $(OF_CORE_OBJ_FILES) -o $@", f"$(CC) $(OF_CORE_OBJ_FILES) {sound_obj} {tess} -o $@")
compile_core_mk.write_text(text)
PY
