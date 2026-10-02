# Installing

LLaya is two files at run time: the module (`llaya.dll` or `llaya.so`) and the LibLayaX
library (`laya.dll`, `liblaya.so` or `liblaya.dylib`). The simplest arrangement is both in
the folder your script runs from. This page is for everything else.

## Where the module goes

`require "llaya"` looks for the module along `package.cpath`. By default that includes the
current folder, and on Windows the folder of the Lua executable. To keep the module
somewhere else, add that folder before the `require`:

```lua
package.cpath = "/opt/myapp/lib/?.so;" .. package.cpath          -- Linux, macOS
package.cpath = "C:\\myapp\\lib\\?.dll;" .. package.cpath        -- Windows
local llaya = require "llaya"
```

or set the environment variable `LUA_CPATH` in the same form.

The file must keep the name `llaya`: Lua finds the module's entry point by that name.

## Where the library goes

When the module is loaded, it loads the library. It looks in this order and takes the first
one it finds:

1. the file named by the environment variable `LLAYA_LIBRARY`, if that is set;
2. the folder the `llaya` module itself is in;
3. the system's normal search: the program's folder and `PATH` on Windows;
   `LD_LIBRARY_PATH` and the standard library folders on Linux; `DYLD_LIBRARY_PATH` and the
   standard folders on macOS.

Because of step 2, putting the library next to the module is all that is needed, on every
system. On Linux no `LD_LIBRARY_PATH` is required.

`LLAYA_LIBRARY` is a full path to the library file. It is useful for trying another build of
the library without moving files, for example the GPU one:

```
LLAYA_LIBRARY=/opt/laya/vulkan/liblaya.so lua myscript.lua
```

When it is set, only that file is tried.

The library is loaded once per process and stays loaded.

## Matching architectures

All three must be the same architecture: the Lua interpreter, the module and the library.

| Lua interpreter | Module folder | Library package |
|---|---|---|
| Windows x64 | `windows-x64` | `laya-windows-…` (`avx2`, `compat-sse42` or `vulkan`) |
| Windows x64, running on Windows on ARM | `windows-x64` | `laya-windows-…-compat-sse42` |
| Windows ARM64 | `windows-arm64` | `laya-windows-arm64-…` |
| Linux x86-64 | `linux-x64` | `laya-linux-…` |
| Linux ARM64 | `linux-arm64` | `laya-linux-arm64-…` |
| macOS, Apple Silicon | `macos-arm64` | `laya-macos-…-arm64` |

A mismatch between module and library shows as `cannot load laya.dll (error 193)` on
Windows. There is no 32-bit version of either.

## Windows: which Lua DLL

On Windows a Lua module must use the very same Lua DLL as the program that loads it. If it
used another copy, the program would have two Lua engines in it and crash.

The published Windows modules ask for a DLL named **`lua51.dll`**. Which of these describes
your program?

| Your program | What to do |
|---|---|
| Uses `lua51.dll` | Nothing. This is what the module expects. |
| Uses `lua5.1.dll`, and a small `lua51.dll` sits next to it that forwards every call to `lua5.1.dll` (the LuaBinaries arrangement) | Nothing. The module reaches the program's Lua through the forwarding DLL. |
| Uses `lua5.1.dll` only | Rebuild the module for that name: `LUA_DLL=lua5.1 LUA_SRC=… scripts/build-all.sh windows-x64` ([Building](building.md)). |
| Has Lua built into the `.exe`, with no Lua DLL | The module has to be linked against that executable instead. C modules only work if the executable exports the Lua functions. |

To see which one applies, look at the DLL files next to your `lua.exe` or your application.

The first two arrangements were both tested, on Windows x64 and on Windows ARM64.

## Linux and macOS: no Lua library needed

On Linux and macOS the module is not linked to a Lua library. It uses the Lua functions of
the program that loads it, so it works with any Lua 5.1 interpreter of the right
architecture. The standard `lua` program exports those functions.

## Lua inside another application

Many applications have Lua built in (editors, game engines, servers). LLaya can be used
from such a Lua when all of these hold:

* The application's Lua is **5.1** for the published binaries, or you build the module for
  its version.
* The application is **64-bit**, and you use the module and the library of its architecture.
* The application **allows C modules**: `require` of a `.dll` or `.so` is not disabled, and
  on Linux and macOS the application exports its Lua functions.
* On Windows, the application's Lua DLL is one of the cases in the table above.

LuaJIT implements the Lua 5.1 interface, so the source should build against it, but that
has not been tried.

Remember that a call blocks the Lua state until the model has answered: tens of milliseconds
or more on a CPU. In an application with a user interface or a frame loop, do not ask
questions on the thread that draws.

## What to ship with your program

| File | Needed |
|---|---|
| `llaya.dll` or `llaya.so` | always |
| `laya.dll`, `liblaya.so` or `liblaya.dylib` | always, next to the module |
| `libMoltenVK.dylib` | macOS with the GPU package, next to the library |
| the model folder | always; about 800 MB for the English model. It can be shipped, downloaded on first run, or located by the user |

Nothing else: no Python, no server, no internet connection at run time.

## System requirements

The module itself asks little: glibc 2.17 on Linux x86-64, glibc 2.27 on Linux ARM64,
macOS 11 on a Mac. The library sets the real requirements. In particular the
released Linux x86-64 library needs glibc 2.38 (Ubuntu 24.04, Fedora 39, Debian 13 or
newer). See the LibLayaX documentation (`docs/platforms.md` in that repository).
