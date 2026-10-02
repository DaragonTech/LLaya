# Troubleshooting

## require "llaya" fails

`require` raises an error, and the first line of the message says which of these it is.

| Message | Cause | Fix |
|---|---|---|
| `module 'llaya' not found:` followed by a list of files | Lua did not find the module. | Put `llaya.dll` or `llaya.so` in the current folder, or add its folder to `package.cpath` ([Installing](installing.md#where-the-module-goes)). The list in the message shows where Lua looked. |
| `llaya: cannot load liblaya.so: … cannot open shared object file …` (or `liblaya.dylib`) | The module was found, the LibLayaX library was not. | Put the library in the same folder as the module, or set `LLAYA_LIBRARY` to its full path. |
| `llaya: cannot load laya.dll (error 126): …` | Windows: the library was not found. | The same. |
| `llaya: cannot load laya.dll (error 193): …` | Windows: `laya.dll` is for another processor type than the module (x64 against ARM64). | Use the library that matches: `laya-windows-arm64-…` for the ARM64 module, an x64 package for the x64 module. |
| `llaya: cannot load the library named by LLAYA_LIBRARY …` | `LLAYA_LIBRARY` is set and points to a file that cannot be loaded. | Correct the path, or unset the variable. |
| `llaya: … has no function laya_version: not a LibLayaX library?` | A file with the library's name that is something else. | Replace it with the real library. |
| `llaya: … has C API version N, this module needs 1` | A library with a different interface. | Use LibLayaX 1.0.5 to 1.0.14, or a module built for that library. |
| `… llaya.dll: %1 is not a valid Win32 application` | Windows: the **module** is for another processor type than the Lua interpreter. | Use the module folder that matches your `lua.exe`: `windows-x64` or `windows-arm64`. |
| `… llaya.dll: The specified module could not be found` although the file is there | Windows: a DLL the module needs is missing. It is almost always the Lua DLL: the module asks for `lua51.dll`. | See [Which Lua DLL](installing.md#windows-which-lua-dll). |
| `… llaya.dll: The specified procedure could not be found`, or a crash on `require` | Windows: the module found a Lua DLL of another Lua version. | Use a module built for your Lua version ([Building](building.md)). |
| `… undefined symbol: lua_…` | Linux: the program that runs Lua does not export the Lua functions, or it is another Lua version. | Use the module with a matching Lua; an application must export its Lua functions for C modules to work. |
| macOS: "cannot be opened because the developer cannot be verified" | The download quarantine. | `xattr -dr com.apple.quarantine .` in the folder with the module and the library. |
| ``version `GLIBC_2.38' not found`` | Linux x86-64: the system is older than the released LibLayaX library was built for. | Build LibLayaX from source on that system. |

To check what was loaded once `require` works:

```lua
local llaya = require "llaya"
print(llaya._VERSION)       -- the module
print(llaya.version())      -- the library, with its version and backends
```

## llaya.new returns nil

The message begins with `cannot load Laya model:` and continues with the library's reason.

| Reason | Cause | Fix |
|---|---|---|
| `Cannot open …/rl_agent_config.json` | The folder is not a model folder. | Give the folder that contains `rl_agent_config.json` ([Getting started](getting-started.md#3-the-model)). |
| `Cannot open model.safetensors`, `Cannot open tokenizer: …` | The download is incomplete. | Download the missing file, keeping the `encoder/` and `tokenizer/` folders. |
| `Unknown option: …` | A key in the options that does not exist. | See [Options](options.md). |
| `Unknown backend: …`, `Unknown precision: …` | A value that is not allowed. | See [Options](options.md). |
| `[json.exception.parse_error…]` | The options were given as a string that is not valid JSON. | Pass a table, or correct the JSON. |
| `This build has no Vulkan backend` | The GPU was asked of a CPU-only library. | Use the LibLayaX `vulkan` package. |
| `Vulkan is not installed on this computer …`, `No usable Vulkan GPU found` | No GPU that Vulkan can use. | Update the GPU driver, or use `backend = "cpu"`. |
| `FP16 currently requires Vulkan`, `BF16 mode requires a GPU; …` | A half precision on the CPU. | Leave `precision` out on the CPU. |
| `This laya build needs CPU instructions your processor (or virtual machine) does not provide: …` | The `avx2` library on a processor without AVX2, or under emulation. | Use the `compat-sse42` library. |

`expected a JSON string or a table` (without the prefix) means the options argument was
something else, for example a number.

## A question returns nil

| Message | Cause | Fix |
|---|---|---|
| `Question '…' exceeds state context limit (N tokens)` | The text is too long. | Shorten or split it, or load the model with `allow_truncation = true` ([Asking questions](asking-questions.md#how-long-a-text-can-be)). |
| `Unsupported question type: …` | `type` is not `"noul"`, `"choice"` or `"score"`. | |
| `Questions require 2 through 255 options` | A choice or score with fewer than two entries. | |
| `Choice criteria must be a list or object`, `Score criteria must be an array` | `criteria` is missing or has the wrong form. An empty table counts as an object, not a list. | Give a list with at least two entries. |
| `[json.exception.out_of_range.403] key '…' not found` | A request without `state`, `questions`, `type` or `instructions`. | |
| `[json.exception.parse_error…]` | A request given as a string that is not valid JSON. | Give a table, or correct the JSON. |
| `the agent is closed` | A method was called after `agent:close()`. | |
| `table keys must be strings or numbers to be written as JSON`, and similar | The request table cannot be converted. | See [Tables and JSON](json.md#from-lua-to-json). |
| `Insufficient memory for this batch` | Too many requests in one call. | Send fewer at a time. |

## An error is raised and no nil is returned

`bad argument #N to '…'` means the call itself is wrong: an argument is missing or of the
wrong type, for example a string where the list of options should be. This is a Lua error,
as with Lua's own functions. Correct the call; or wrap it in `pcall` if the arguments come
from outside.

`attempt to index a nil value` right after a question usually means the result was not
checked. Wrap the call in `assert(...)` to see the real message:

```lua
local answer = assert(agent:ask_yes_no_table(text, question))
```

## The answer is not where I look for it

* The answer of a question is under the name you gave it: `answers.refund` if the name was
  `"refund"`, `answers.q` if you gave none.
* `results` is a list: the first request is `results[1]`.
* In a `score` answer, `probabilities` and `legend` have string keys: `probabilities["1"]`,
  not `probabilities[1]`.
* The methods without `_table` return JSON **text**. Indexing it gives `nil`. Use the
  `_table` form, or decode the text.

## It is slow

* **Loading takes about a second; answering does not.** If every question takes a second
  or more, the program is probably calling `llaya.new` per question. Create one agent and
  keep it.
* **On the CPU a question takes tens of milliseconds** on a fast desktop processor and a few
  hundred on a small virtual machine. For more, use a GPU ([Options](options.md#running-on-the-gpu)).
* **On a GPU, send many questions per call** ([Asking questions](asking-questions.md#many-texts-in-one-call)).
* **Check what is in use**: `answer.backend` and `answer.device` show whether the CPU or the
  GPU did the work.

## macOS: many lines of mvk-info on the GPU

With `backend = "vulkan"` on a Mac, the first use of the GPU prints a warning from
`ggml_vulkan` and about 180 lines that begin with `[mvk-info]`: the list of Vulkan extensions
and a description of the GPU. They come from MoltenVK, they go to the standard error stream,
and they are harmless.

To switch them off, set the environment variable `MVK_CONFIG_LOG_LEVEL=1` before starting the
program; MoltenVK then reports errors only. Tested on an Apple M3 Ultra: one line remains,
the `ggml_vulkan` warning about `VK_KHR_portability_enumeration`, which is harmless too.

```
MVK_CONFIG_LOG_LEVEL=1 lua myscript.lua
```

## The first GPU question can be slow

The first question after loading a model on a GPU can include one-time setup. Measured with
the test suite, which starts with a single question:

| Machine | First question on the GPU | Same question on the CPU |
|---|---|---|
| NVIDIA RTX 5080 Laptop GPU (Windows 11) | 1355 ms | 101 ms |
| Apple M3 Ultra, first run | 417 ms | 105 ms |
| Apple M3 Ultra, a later run | 98 ms | |

So do not judge a GPU by its first answer. Load the model once, keep the agent, and send
questions in batches: that is where a GPU pays off
([Asking questions](asking-questions.md#many-texts-in-one-call)).

## Memory keeps growing

Each agent holds about 1.7 GB that Lua's garbage collector does not see, so it has no reason
to hurry. Call `agent:close()` when an agent is no longer needed.

## A call never returns

Set `LAYA_DEBUG=1` in the environment before starting the program. The library then prints a
line on the standard error stream at every stage of every call:

```
[laya] create: loading model: /models/laya
[laya] load: starting the cpu backend, threads=4
[laya] load: model.safetensors opened, 803 MB
[laya] create: model loaded
[laya] predict: running the model
[laya] predict: done
```

The last line shows where it stopped. The LibLayaX documentation
(`docs/troubleshooting.md` in that repository) has more on the library's own diagnostics,
including crash reports on Windows.

## Reporting a problem

A useful report contains the output of these two commands, the system and processor, and the
complete error message:

```
lua -e "local l = require 'llaya'; print(_VERSION, l._VERSION, l.version())"
lua test.lua /models/laya
```
