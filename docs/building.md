# Building

The binaries on the releases page are for Lua 5.1. Build the module yourself for Lua 5.2,
5.3 or 5.4, for a Lua DLL with another name on Windows, or for a platform that is not
published.

The whole binding is one file, `src/llaya.c`. It needs a C compiler and Lua's header files
(`lua.h`, `lauxlib.h`, `luaconf.h`). It does **not** need the LibLayaX library to build: the
module loads the library when it runs.

## For the Lua on your machine

With the `Makefile`:

```
make LUA_INCDIR=/usr/include/lua5.1                                    Linux, macOS
make LUA_INCDIR=C:/lua/include LUA_LIBDIR=C:/lua LUA_LIB=lua51         Windows (MinGW)
```

| Variable | Meaning | Default |
|---|---|---|
| `LUA_INCDIR` | The folder with `lua.h`. | `/usr/include/lua5.1` |
| `LUA_LIBDIR` | Windows only: the folder with the Lua DLL or its import library. | `.` |
| `LUA_LIB` | Windows only: the name of the Lua DLL, without `.dll`. | `lua51` |
| `CC`, `CFLAGS` | Compiler and flags. | `cc`, `-O2 -Wall -Wextra` |

The result is `llaya.so` or `llaya.dll` in the current folder.

For another Lua version, point `LUA_INCDIR` at that version's headers:

```
make LUA_INCDIR=/usr/include/lua5.4
```

On Debian and Ubuntu the headers are in the packages `liblua5.1-0-dev`, `liblua5.3-dev`,
`liblua5.4-dev`.

## Without make

The commands the `Makefile` runs:

```
cc -O2 -fPIC -fvisibility=hidden -I/usr/include/lua5.1 -shared -o llaya.so src/llaya.c -ldl        Linux
cc -O2 -fvisibility=hidden -I/path/to/lua -bundle -undefined dynamic_lookup -o llaya.so src/llaya.c    macOS
gcc -O2 -DLUA_BUILD_AS_DLL -IC:/lua/include -shared -o llaya.dll src/llaya.c -LC:/lua -llua51      Windows
```

On Linux and macOS the module is not linked to a Lua library; it uses the Lua of the program
that loads it. On Windows it must be linked to the Lua DLL of that program, which is why the
name of the DLL matters there ([Installing](installing.md#windows-which-lua-dll)).

## All published platforms at once, from Linux

`scripts/build-all.sh` cross-builds the module for five targets on one Linux machine:

```
LUA_SRC=/path/to/lua-5.1.4/src LLVM_MINGW=/path/to/llvm-mingw scripts/build-all.sh
```

It writes `out/<target>/llaya.dll` or `llaya.so` for `linux-x64`, `linux-arm64`,
`macos-arm64`, `windows-x64` and `windows-arm64`. To build only some, name them:

```
LUA_SRC=/path/to/lua-5.1.4/src scripts/build-all.sh linux-x64 windows-x64
```

| Setting | Meaning | Default |
|---|---|---|
| `LUA_SRC` | The folder with Lua's headers and `.c` files. Required. | |
| `LUA_DLL` | Windows: the name of the host program's Lua DLL, without `.dll`. | `lua51` |
| `LLVM_MINGW` | The llvm-mingw folder; needed for `windows-arm64`. | |
| `OUT` | The output folder. | `./out` |

Tools it uses:

| Tool | For | How to get it |
|---|---|---|
| Zig | Linux and macOS targets | `pip install ziglang` |
| `ld64.lld` | the final link for macOS | `apt install lld` |
| MinGW-w64 | Windows x64 | `apt install mingw-w64` |
| [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) | Windows ARM64 | download and unpack |

For the Windows targets the script also builds the Lua DLL and a `lua.exe` from `LUA_SRC`,
to link the module against and to test it with.

### A module for `lua5.1.dll`

```
LUA_DLL=lua5.1 LUA_SRC=/path/to/lua-5.1.4/src scripts/build-all.sh windows-x64
```

The module then asks for `lua5.1.dll` and needs no forwarding `lua51.dll`.

## After building

Put the LibLayaX library next to the new module and run the tests ([Testing](testing.md)):

```
lua test/test.lua                   26 checks, no model needed
lua test/test.lua /models/laya      50 checks
```

## Lua versions

One source serves Lua 5.1, 5.2, 5.3 and 5.4; the few differences are handled inside the
file. The to-be-closed support (`local agent <close>`) is active in Lua 5.4. LuaJIT uses the
Lua 5.1 interface, so the source should build against it, but that has not been tried.
