# Getting started

## The quick way: a test kit

To see LLaya work before setting anything up, use a test kit from the releases page. A kit
already contains the Lua interpreter, the module and the library for each platform; only the
model is missing ([step 3](#3-the-model) below says where to get it).

| Archive | Use it for |
|---|---|
| `LLaya-0.1.0-testkit-cpu.tar.xz` | any machine; runs on the processor |
| `LLaya-0.1.0-testkit-gpu.tar.xz` | Windows x64, Linux x64 or a Mac with Apple Silicon, with a GPU |

```
tar -xf LLaya-0.1.0-testkit-cpu.tar.xz
cd LLaya-0.1.0-testkit-cpu/windows-x64          the folder for your system
run-tests.bat D:\models\laya                    Windows
sh run-tests.sh /models/laya                    Linux, macOS
```

It should end with `50 checks, 0 failures`, followed by the speed on your machine. The same
folder is a working setup: you can put your own script in it and run it with the `lua` that
is there. [Testing](testing.md#the-test-kits) describes the kits in full.

The rest of this page builds the same setup by hand, which is what you do for your own
program.

## The parts

A working setup is three things in one folder, plus the model somewhere on disk:

```
myproject/
├── llaya.dll  or  llaya.so          the Lua module (this project)
├── laya.dll   or  liblaya.so  or  liblaya.dylib      the LibLayaX library
└── myscript.lua

/models/laya/                        the model (about 800 MB)
```

## 1. The module

Take the module for your system from `LLaya-0.1.0-lua5.1-binaries.zip` on the releases page.
It contains one folder per platform:

| Folder | File | For |
|---|---|---|
| `windows-x64` | `llaya.dll` | 64-bit Lua 5.1 on Windows |
| `windows-arm64` | `llaya.dll` | ARM64 Lua 5.1 on Windows on ARM |
| `linux-x64` | `llaya.so` | Linux on Intel and AMD processors |
| `linux-arm64` | `llaya.so` | Linux on ARM64 |
| `macos-arm64` | `llaya.so` | macOS on Apple Silicon |

These are built for **Lua 5.1**. For Lua 5.2, 5.3 or 5.4, build the module from source; it
is one C file ([Building](building.md)).

Choose by the architecture of your **Lua interpreter**, not of the machine. A 64-bit x64
`lua.exe` running on Windows on ARM needs the `windows-x64` module. There is no 32-bit
version.

## 2. The library

LLaya does not contain the AI engine. That is LibLayaX, a separate download from its own
releases page. Take the package for the same platform and copy the library file out of it:

| Platform | Package | File to copy |
|---|---|---|
| Windows x64 | `laya-windows-…-avx2`, or `-compat-sse42` for older processors and for x64 Lua on Windows on ARM, or `-vulkan` for a GPU | `laya.dll` |
| Windows ARM64 | `laya-windows-arm64-…` | `laya.dll` |
| Linux x86-64 | `laya-linux-…`, folder `avx2`, `compat-sse42` or `vulkan` | `liblaya.so` |
| Linux ARM64 | `laya-linux-arm64-…` | `liblaya.so` |
| macOS, Apple Silicon | `laya-macos-…-arm64`, or `laya-macos-vulkan-…-arm64` for the GPU | `liblaya.dylib` (and `libMoltenVK.dylib` from the GPU package) |

Any LibLayaX version from 1.0.5 on works. Put the library **in the same folder as the
module**. The module looks there first, so nothing has to be configured.

On macOS, remove the download quarantine from both files once
(`xattr -dr com.apple.quarantine .` in that folder).

## 3. The model

The model is published by the Laya project on Hugging Face, in the repository
[convaiinnovations/laya](https://huggingface.co/convaiinnovations/laya). The English model is
a download of about 800 MB.

```
pip install huggingface_hub
huggingface-cli download convaiinnovations/laya --local-dir /models/laya \
    --include "model.safetensors" "rl_agent_config.json" "encoder/*" "tokenizer/*"
```

Or with a browser: open the repository page, go to "Files", and download these five files,
keeping the folders:

```
model.safetensors
rl_agent_config.json
encoder/config.json
tokenizer/tokenizer.json
tokenizer/tokenizer_config.json
```

The folder that contains `rl_agent_config.json` is the **model folder**: the path you give to
`llaya.new`. The model comes with its own terms; see the model card on the repository page.

## 4. A first script

Save this as `first.lua` next to the module:

```lua
local llaya = require "llaya"

print(llaya._VERSION, llaya.version())

local agent = assert(llaya.new("/models/laya"))

local answer = assert(agent:ask_yes_no_table(
  "Please refund the duplicate charge.",        -- the text
  "Does the customer ask for a refund?",        -- the question
  "refund"))                                    -- a name for the answer

print(answer.results[1].answers.refund.noul)
agent:close()
```

Run it from that folder:

```
lua first.lua
```

```
LLaya 0.1.0     laya_c 1.0.14 (api 1; backends: cpu)
0.8364
```

`noul` is the probability that the answer is yes: 0.8364 for this text with the English
model.

## What the script did

1. **`require "llaya"`** loaded the module, and the module loaded the LibLayaX library from
   its own folder.
2. **`llaya.new`** loaded the model. This takes about a second from a fast disk and about
   1.7 GB of memory. A real program does it once and keeps the agent.
3. **`ask_yes_no_table`** sent one text and one question and returned the answer as a Lua
   table. `ask_yes_no`, without `_table`, returns the same answer as JSON text.
4. **`agent:close()`** unloaded the model. Without it the garbage collector does so later.

`assert` is used because the functions return `nil` and a message when something goes wrong,
the usual Lua convention.

## A second example

The source has a slightly longer example, `examples/ask.lua` (it is also in the binaries
zip), which asks one question of each kind:

```
lua ask.lua /models/laya
```

## Where to go next

* [Asking questions](asking-questions.md): choices, scores, several questions, many texts.
* [Options](options.md): running on the GPU.
* [Installing](installing.md): using the module from a folder of your choice, or from a Lua
  that lives inside another application.
* [Troubleshooting](troubleshooting.md): if `require "llaya"` fails.
