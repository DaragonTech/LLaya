#!/usr/bin/env bash
# Cross-builds the llaya module for one Lua version on five targets, from Linux:
#   out/windows-x64/llaya.dll   out/windows-arm64/llaya.dll
#   out/linux-x64/llaya.so      out/linux-arm64/llaya.so      out/macos-arm64/llaya.so
#
#   LUA_SRC=/path/to/lua-5.1.4/src scripts/build-all.sh [target ...]
#
# LUA_SRC is the folder with lua.h, lauxlib.h, luaconf.h and Lua's .c files. Only the headers
# are used, except on Windows, where a module must link against the Lua DLL of the host
# program: the script builds that DLL from LUA_SRC to link against (and to test with).
#
# Tools:  zig          (pip install ziglang)             Linux and macOS targets
#         ld64.lld     (apt install lld)                 macOS final link
#         mingw-w64    (x86_64-w64-mingw32-gcc)          Windows x64
#         llvm-mingw   (LLVM_MINGW=/path/to/llvm-mingw)  Windows ARM64
# Settings: LUA_DLL  name of the host's Lua DLL without .dll (default lua51)
#           OUT      output folder (default ./out)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LUA_SRC=${LUA_SRC:?set LUA_SRC to the folder with the Lua headers and sources}
OUT=${OUT:-$ROOT/out}
LUA_DLL=${LUA_DLL:-lua51}
ZIG=${ZIG:-$(command -v zig || python3 -c "import ziglang,os;print(os.path.join(os.path.dirname(ziglang.__file__),'zig'))")}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
LD64=${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18 || echo ld64.lld)}
CFLAGS="-O2 -Wall -Wextra -fvisibility=hidden -I$LUA_SRC"
STRIP=-s  # no symbol table in the shipped modules
SOURCE=$ROOT/src/llaya.c
LUA_CORE=$(ls "$LUA_SRC"/*.c | grep -v -E '/(lua|luac|print|ltests|onelua)\.c$')
TARGETS=${*:-linux-x64 linux-arm64 macos-arm64 windows-x64 windows-arm64}

# Windows: the host's Lua DLL (for linking and testing) and a lua.exe that uses it.
windows_lua() {  # $1 = compiler, $2 = folder
  [ -f "$2/$LUA_DLL.dll" ] && return
  $1 -O2 -DLUA_BUILD_AS_DLL -shared -o "$2/$LUA_DLL.dll" $LUA_CORE -Wl,--out-implib,"$2/lib$LUA_DLL.dll.a"
  $1 -O2 -DLUA_BUILD_AS_DLL -o "$2/lua.exe" "$LUA_SRC/lua.c" -L"$2" -l"$LUA_DLL"
}

for target in $TARGETS; do
  mkdir -p "$OUT/$target"
  case $target in
    linux-x64)    "$ZIG" cc -target x86_64-linux-gnu.2.17  $CFLAGS $STRIP -fPIC -shared -o "$OUT/$target/llaya.so" "$SOURCE" -ldl ;;
    linux-arm64)  "$ZIG" cc -target aarch64-linux-gnu.2.27 $CFLAGS $STRIP -fPIC -shared -o "$OUT/$target/llaya.so" "$SOURCE" -ldl ;;
    # A bundle (what Lua modules are on macOS), with Lua's functions left for the host program
    # to supply. Zig compiles; ld64.lld links (zig's own linker crashes on -bundle) and signs.
    macos-arm64)  "$ZIG" cc -target aarch64-macos.11.0 $CFLAGS -c -o "$OUT/$target/llaya.o" "$SOURCE"
                  "$LD64" -arch arm64 -platform_version macos 11.0 11.0 -bundle -undefined dynamic_lookup \
                    -exported_symbol _luaopen_llaya -x -adhoc_codesign -o "$OUT/$target/llaya.so" "$OUT/$target/llaya.o" \
                    -L"$("$ZIG" env | sed -n 's/.*\.lib_dir = "\(.*\)".*/\1/p')/libc/darwin" -lSystem
                  rm "$OUT/$target/llaya.o" ;;
    windows-x64)  windows_lua "$MINGW" "$OUT/$target"
                  $MINGW $CFLAGS $STRIP -DLUA_BUILD_AS_DLL -shared -static-libgcc -o "$OUT/$target/llaya.dll" "$SOURCE" -L"$OUT/$target" -l"$LUA_DLL" ;;
    windows-arm64) CC="${LLVM_MINGW:?set LLVM_MINGW to the llvm-mingw folder}/bin/aarch64-w64-mingw32-clang"
                  windows_lua "$CC" "$OUT/$target"
                  $CC $CFLAGS $STRIP -DLUA_BUILD_AS_DLL -shared -o "$OUT/$target/llaya.dll" "$SOURCE" -L"$OUT/$target" -l"$LUA_DLL" ;;
    *) echo "unknown target $target" >&2; exit 1 ;;
  esac
  echo "== $target: $(ls "$OUT/$target" | tr '\n' ' ')"
done
