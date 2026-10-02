# LLaya

![LLaya Logo](./llaya-logo.png)

Lua binding for **Laya**, the open typed-decision AI model, running in-process through the
LibLayaX library (`laya.dll` / `liblaya.so` / `liblaya.dylib`, built from
[laya.cpp](https://github.com/lkarlslund/laya.cpp)). No server, no HTTP: your Lua program loads
the model and asks it yes/no, multiple-choice and score questions about a piece of text.

LLaya is an independent, unofficial project. It is not part of Laya or of laya.cpp.

```lua
local llaya = require "llaya"

local agent = assert(llaya.new("/models/laya", { backend = "cpu" }))

-- the answer as a Lua table ...
local answer = assert(agent:ask_yes_no_table("Please refund the duplicate charge.",
                                             "Does the customer ask for a refund?", "refund"))
print(answer.results[1].answers.refund.noul)        --> 0.8364

-- ... or as JSON text, for the JSON library you already use
local json = assert(agent:ask_yes_no("Please refund the duplicate charge.",
                                     "Does the customer ask for a refund?", "refund"))
```

| Path | What it is |
|---|---|
| `src/llaya.c` | The whole binding: one C file, no dependencies besides Lua's headers. |
| `test/test.lua` | The test suite. |
| `examples/ask.lua` | A small program that loads a model and asks three questions. |
| `Makefile` | Builds the module for the Lua on your machine. |
| `scripts/build-all.sh` | Cross-builds the module for five platforms from Linux. |
| `LICENSE` | MIT. |

The source builds against **Lua 5.1, 5.2, 5.3 and 5.4**. Ready-made binaries are published for
**Lua 5.1** only.

## What you need

1. **The `llaya` module** for your Lua version and platform: `llaya.dll` on Windows, `llaya.so`
   on Linux and macOS. Take it from the Lua 5.1 binaries, or build it (see below).

2. **The LibLayaX library**, C API version 1 (1.0.5 or later), in the same folder as the module:

   | Platform | Library |
   |---|---|
   | Windows x64 | `laya.dll` from `laya-windows-…-avx2`, `-compat-sse42` or `-vulkan` |
   | Windows ARM64 | `laya.dll` from `laya-windows-arm64-…` |
   | Linux x86-64 / ARM64 | `liblaya.so` from `laya-linux-…` / `laya-linux-arm64-…` |
   | macOS (Apple Silicon) | `liblaya.dylib` from `laya-macos-…-arm64` |

   The module is called `llaya`, with two Ls, because on Windows `laya.dll` is the library
   itself.

3. **The model weights** (about 800 MB for the english variant), from the Hugging Face
   repository [convaiinnovations/laya](https://huggingface.co/convaiinnovations/laya). With the
   Hugging Face command-line tool:

   ```
   pip install huggingface_hub
   huggingface-cli download convaiinnovations/laya --local-dir /models/laya \
       --include "model.safetensors" "rl_agent_config.json" "encoder/*" "tokenizer/*"
   ```

   The folder you pass to `llaya.new` is the one that contains `rl_agent_config.json`. The LibLayaX
   README ("Getting the model") lists the files needed, the other ways to download them, and how
   to get the multilingual and typed-decisions variants.

### Where the files go

Put the module where `require` looks (`package.cpath`; by default the current folder), and the
library next to it. The module finds the library by itself, in this order:

1. the file named by the environment variable `LLAYA_LIBRARY`, if set;
2. the folder the `llaya` module is in;
3. the system's normal search (the program's folder and `PATH` on Windows,
   `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` and the standard folders elsewhere).

So on Linux no `LD_LIBRARY_PATH` is needed. If the library cannot be found, `require "llaya"`
fails with a message that says so.

**Windows only:** a Lua module must use the same Lua DLL as the program that loads it. The
published Windows binaries ask for **`lua51.dll`**. Two common arrangements both work:

* the program itself uses `lua51.dll`;
* the program uses `lua5.1.dll` and `lua51.dll` is a proxy that passes every call on to it
  (the usual LuaBinaries layout). The module then shares the program's Lua through the proxy.

If there is only a `lua5.1.dll` and no proxy, rebuild the module for that name
(`LUA_DLL=lua5.1 LUA_SRC=... scripts/build-all.sh windows-x64`). If Lua is built into the
executable itself, the module has to be linked against that executable instead.

If `require "llaya"` reports `cannot load laya.dll (error 193)`, the `laya.dll` next to the
module is for the other processor type (x64 instead of ARM64, or the reverse).

## The module

```lua
llaya.new(model_dir [, options])   --> agent            | nil, message
llaya.version()                    --> "laya_c 1.0.14 (api 1; backends: cpu)"
llaya.api_version()                --> 1
llaya.encode(value)                --> JSON text        | nil, message
llaya.decode(json)                 --> Lua value        | nil, message
llaya.null                         -- what JSON null becomes
llaya._VERSION                     --> "LLaya 0.1.0"
```

Every agent method exists twice: the plain name returns **JSON text**, the name with `_table`
returns the same answer as a **Lua table**.

| JSON text | Lua table | What it does |
|---|---|---|
| `agent:ask_yes_no(state, instructions [, id])` | `agent:ask_yes_no_table(...)` | One yes/no question. |
| `agent:ask_choice(state, instructions, options [, id])` | `agent:ask_choice_table(...)` | One multiple-choice question; `options` is a list of strings. |
| `agent:ask_score(state, instructions, levels [, id])` | `agent:ask_score_table(...)` | One question on an ordered scale; `levels` from lowest to highest. |
| `agent:predict(request)` | `agent:predict_table(request)` | Anything the protocol supports: several questions, several texts in one call. |
| `agent:info()` | `agent:info_table()` | Backend, device, model and limits. |
| `agent:prepare(request)` | `agent:prepare_table(request)` | The tokenized model inputs, for debugging. |

`agent:close()` unloads the model at once; otherwise the garbage collector does it.

* `state` is the text to judge: a string, or a table that is sent as a JSON object.
* `id` names the question in the answer (default `"q"`).
* `request` and `options` (of `llaya.new`) are a JSON string **or** a Lua table:

```lua
local answer = assert(agent:predict_table({
  { state = "I want my money back",
    questions = { refund = { type = "noul", instructions = "Does the customer ask for a refund?" } } },
  { state = "Great service, thanks",
    questions = { mood = { type = "choice", instructions = "Mood?", criteria = { "happy", "angry" } } } },
}))
print(answer.results[2].answers.mood.choice)
```

The answer, as JSON text and as the table it becomes:

```json
{"results":[{"model":"laya-rl-agent",
             "answers":{"refund":{"type":"noul","confidence":0.8364,"noul":0.8364,
                                  "action":{"act_probability":1.0}}},
             "usage":{"input_tokens":40,"output_tokens":0}}],
 "elapsed_ms":244.9,"backend":"CPU","device":"..."}
```

```lua
answer.results[1].answers.refund.noul      -- yes/no: probability of "yes"
answer.results[1].answers.intent.choice    -- choice: the winning option; .probabilities has all
answer.results[1].answers.anger.score      -- score: the expected level; .legend names the levels
answer.elapsed_ms
```

**Options** for `llaya.new` (unknown keys are rejected):

| Key | Values | Default |
|---|---|---|
| `backend` | `"cpu"`, `"vulkan"`, `"cuda"` (must be compiled into the library you ship) | `"cpu"` |
| `variant` | `"english"`, `"multilingual"`, `"typed-decisions"`: picks a subfolder of a model store | folder as given |
| `precision` | `"fp32"`, `"fp16"`, `"bf16"` (the half precisions need a GPU) | `"fp32"` |
| `threads` | CPU threads, 0 = all | 0 |
| `device` | GPU index, or part of its name such as `"RTX"` | first discrete GPU |
| `flash` | boolean, fused attention (GPU) | on for `fp16`/`bf16`, otherwise off |
| `tensor_core`, `allow_truncation` | booleans | off |

## Things worth knowing

* **Errors.** A model that cannot be loaded or a request the library rejects gives
  `nil, message`, so `assert(...)` works and a failed request leaves the agent usable. Only a
  wrong argument type raises a Lua error, as with Lua's own functions.
* **Tables from JSON.** Arrays become tables indexed from 1. JSON `null` becomes `llaya.null`
  (not `nil`), so no key is lost. On Lua 5.3 and later, whole numbers are integers.
* **Tables to JSON.** A table whose keys are exactly 1..n is written as an array, any other
  table as an object (keys must be strings or numbers). An empty table is written as `{}`.
  `nil` and `llaya.null` are written as `null`.
* **Strings.** Everything is UTF-8. Lua strings are passed through unchanged.
* **Blocking.** A call returns when the model has answered; the Lua state waits meanwhile. For
  throughput, send several requests in one `predict`.
* **Memory.** About 1.7 GB per loaded model on CPU. Create one agent per model and keep it.
* **Debugging.** `LAYA_DEBUG=1` in the environment makes the library print what it is doing at
  each stage of each call.

## Building

For the Lua on your machine:

```
make LUA_INCDIR=/usr/include/lua5.1                                    Linux, macOS
make LUA_INCDIR=C:/lua/include LUA_LIBDIR=C:/lua LUA_LIB=lua51         Windows (MinGW)
```

`LUA_INCDIR` is the folder with `lua.h`. The LibLayaX library is not needed to build, only to
run. For all five published platforms at once, from Linux:

```
LUA_SRC=/path/to/lua-5.1.4/src LLVM_MINGW=/path/to/llvm-mingw scripts/build-all.sh
```

It writes `out/<platform>/llaya.dll` or `llaya.so`. The header of the script lists the tools it
needs (Zig, MinGW-w64, llvm-mingw, lld).

## Running the tests

From a folder that contains the module, the library and `test.lua`:

```
lua test.lua                        model-free checks (26)
lua test.lua /path/to/model         full run on the CPU (50)
lua test.lua /path/to/model vulkan  full run on another backend
lua ask.lua /path/to/model          the example
```

Exit code 0 means everything passed. The full run covers the JSON encoder and decoder, all three
question types as text and as tables, table and string requests, batches, Unicode, error
reporting, `prepare`, `close` and unloading by the garbage collector.

## Status

Built and tested with LibLayaX 1.0.14. Unless a row says otherwise, the model was a synthetic
test model (real architecture, random weights):

| Lua | Platform | Result |
|---|---|---|
| 5.1.4 | Linux x86-64 | full run, 50 checks, 0 failures; also clean under address and undefined-behaviour sanitizers, including a few thousand malformed JSON inputs |
| 5.1.4 | Windows x64 | **real Windows, real english model** (Windows 11 on ARM running the x64 module under x64 emulation): full run, 50 checks, 0 failures, `noul` 0.8364. Also under Wine with the synthetic model. Not yet on an x64 PC. |
| 5.1.4 | Linux ARM64 | **real hardware, real english model** (Ubuntu 24.04 ARM64, Parallels on Apple Silicon): full run, 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | Windows ARM64 | **real hardware, real english model** (Windows 11 on ARM, Parallels on Apple Silicon): full run, 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | macOS Apple Silicon | **real hardware, real english model** (Apple M3 Ultra): full run, 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | Windows ARM64 and x64, program on `lua5.1.dll` with a `lua51.dll` proxy | **real Windows, real english model**: full run, 50 checks, 0 failures on both (the x64 one under x64 emulation on the ARM machine). |
| 5.2.3, 5.3.6, 5.4.9 | Linux x86-64 | full run, 0 failures (built from this source; no binaries published) |

Not yet done: the Windows x64 module on an x64 PC; the Linux x86-64 module outside the build
machine and with the real model; LuaJIT
(it uses the Lua 5.1 API, so the source should build against it, but that was not tried).

## Credits

* **[Laya](https://github.com/NandhaKishorM/laya)** by NandhaKishorM is the original project:
  the model, the typed-decision primitives (`choice`, `score`, `noul`) and the Python
  reference implementation on PyTorch and Transformers. Apache-2.0. The weights are published
  on Hugging Face under `convaiinnovations`.
* **[laya.cpp](https://github.com/lkarlslund/laya.cpp)** by Lars Karlslund is the native C++
  port of Laya inference, built on ggml, with CPU, CUDA, Vulkan and Core ML backends. MIT.
  The native library LLaya loads is built from it.
* **LLaya** and the LibLayaX library underneath it were written by Claude (Anthropic), under
  the direction of **Felipe Daragon** of **DaragonTech**, who set the goals and guided the work.
* [Lua](https://www.lua.org) is by Roberto Ierusalimschy, Luiz Henrique de Figueiredo and
  Waldemar Celes at PUC-Rio. MIT.

## License

LLaya is released under the MIT License; see [LICENSE](LICENSE).

It contains no code from the projects it builds on. Those keep their own terms: the LibLayaX
library and laya.cpp are MIT, Laya is Apache-2.0, Lua is MIT, and the model weights are published on
Hugging Face under their own terms.
