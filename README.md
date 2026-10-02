![LLaya Logo](./llaya-logo.png)

# LLaya

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
| `docs/` | The manual: installing, asking questions, the full reference, troubleshooting. |
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

   The folder you pass to `llaya.new` is the one that contains `rl_agent_config.json`.
   [Getting started](docs/getting-started.md) lists the files and the other ways to download
   them.

### Where the files go

Put the module where `require` looks (by default the current folder) and the library next to
it. The module finds the library in its own folder, so nothing has to be configured, and on
Linux no `LD_LIBRARY_PATH` is needed. The environment variable `LLAYA_LIBRARY` can name
another library file.

**Windows only:** a Lua module must use the same Lua DLL as the program that loads it. The
published Windows binaries ask for **`lua51.dll`**. A program that uses `lua51.dll` works, and
so does one that uses `lua5.1.dll` with a `lua51.dll` that forwards to it (the usual
LuaBinaries layout). For anything else, see [Installing](docs/installing.md).

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
             "answers":{"refund":{"type":"noul","confidence":0.8364,
                                  "action":{"act_probability":1.0},"noul":0.8364}},
             "usage":{"input_tokens":40,"output_tokens":0}}],
 "elapsed_ms":244.9,"backend":"CPU","device":"..."}
```

```lua
answer.results[1].answers.refund.noul      -- yes/no: probability of "yes"
answer.results[1].answers.intent.choice    -- choice: the winning option; .probabilities has all
answer.results[1].answers.anger.score      -- score: the expected level; .legend names the levels
answer.elapsed_ms
```

Errors come back the Lua way: a model that cannot be loaded or a request the library rejects
gives `nil, message`, so `assert(...)` works, and a failed request leaves the agent usable.

To run on the GPU, pass options to `llaya.new`, for example
`{ backend = "vulkan", precision = "fp16" }`, with a LibLayaX library that includes the GPU
backend. See [Options](docs/options.md).

## Documentation

The [`docs/`](docs/README.md) folder is the manual.

| Page | Content |
|---|---|
| [Getting started](docs/getting-started.md) | The three things to download, where to put them, a first script. |
| [Asking questions](docs/asking-questions.md) | The three kinds of question, several at once, many texts in one call, reading the answers, how long a text can be. |
| [The module](docs/reference.md) | Every function and method: arguments, results, errors. |
| [Options](docs/options.md) | CPU or GPU, precision, threads, model variant. |
| [Tables and JSON](docs/json.md) | How tables become JSON and back, `llaya.null`, `llaya.encode`, `llaya.decode`. |
| [Installing](docs/installing.md) | How the module finds the library, the Lua DLL on Windows, Lua inside another application. |
| [Troubleshooting](docs/troubleshooting.md) | Error messages and what to do about them. |
| [Testing](docs/testing.md) | The test suite and what has been tested where. |
| [Building](docs/building.md) | Building the module for Lua 5.2 to 5.4 or for another platform. |

## Status

Version 0.1.0, built and tested with LibLayaX 1.0.14.

With **Lua 5.1.4 and the real english model**, the full test suite (50 checks) passes with 0
failures on Windows x64, Windows ARM64, Linux ARM64 and macOS with Apple Silicon. On Windows
x64 and on the Mac it passes on the CPU and on the GPU (NVIDIA RTX 5080 Laptop GPU, Apple M3
Ultra). On Windows both Lua DLL layouts were tried. Lua 5.2, 5.3 and 5.4 pass on Linux x86-64
with a synthetic test model.

Not yet done: the Linux x86-64 module with the real model; the GPU on Linux; the half
precisions (`fp16`, `bf16`) through LLaya; LuaJIT. The full table is in
[Testing](docs/testing.md#what-has-been-tested).

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
