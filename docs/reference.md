# The module

```lua
local llaya = require "llaya"
```

`require` returns the module table. It does not create a global variable.

## Overview

Module functions:

| Function | Returns |
|---|---|
| [`llaya.new(model_dir [, options])`](#llayanew) | an agent |
| [`llaya.version()`](#llayaversion-and-llayaapi_version) | the library's version text |
| [`llaya.api_version()`](#llayaversion-and-llayaapi_version) | the library's API version, a number |
| [`llaya.encode(value)`](#llayaencode-and-llayadecode) | JSON text |
| [`llaya.decode(json)`](#llayaencode-and-llayadecode) | a Lua value |
| [`llaya.null`](#llayanull) | the value that stands for JSON `null` |
| `llaya._VERSION` | the module's version, `"LLaya 0.1.0"` |

Agent methods. Each exists twice: the plain name returns JSON text, the name with `_table`
returns a Lua table.

| JSON text | Lua table | Purpose |
|---|---|---|
| [`agent:ask_yes_no(state, instructions [, id])`](#agentask_yes_no) | `agent:ask_yes_no_table(...)` | One yes/no question. |
| [`agent:ask_choice(state, instructions, options [, id])`](#agentask_choice) | `agent:ask_choice_table(...)` | One multiple-choice question. |
| [`agent:ask_score(state, instructions, levels [, id])`](#agentask_score) | `agent:ask_score_table(...)` | One question on a scale. |
| [`agent:predict(request)`](#agentpredict) | `agent:predict_table(request)` | Any request: several questions, several texts. |
| [`agent:info()`](#agentinfo) | `agent:info_table()` | Backend, device, model and limits. |
| [`agent:prepare(request)`](#agentprepare) | `agent:prepare_table(request)` | The model's input for a request, for debugging. |
| [`agent:close()`](#agentclose) | | Unload the model now. |

## Errors

There are two kinds, as in Lua's own libraries.

**Something went wrong at run time**: the function returns `nil` and a message. This covers
a model that cannot be loaded, a request the library rejects, text that is too long, a table
that cannot be written as JSON, and a closed agent. `assert(...)` turns it into a Lua error
when that is what you want:

```lua
local agent, message = llaya.new("/wrong/folder")
if not agent then print(message) end
--> cannot load Laya model: Cannot open /wrong/folder/rl_agent_config.json
```

**The call itself is wrong**: an argument of the wrong type raises a Lua error, like
`string.rep(nil)` does.

```lua
agent:ask_choice("text", "Which?", "not a table")
--> bad argument #3 to 'ask_choice' (table expected, got string)
```

A failed request leaves the agent usable.

## llaya.new

```lua
agent = llaya.new(model_dir [, options])      --> agent | nil, message
```

Loads a model.

| Argument | Meaning |
|---|---|
| `model_dir` | The model folder: the one that contains `rl_agent_config.json`. A string, in UTF-8. |
| `options` | Optional. A table such as `{ backend = "vulkan", precision = "fp16" }`, or the same as JSON text. See [Options](options.md). |

Loading takes about a second from a fast disk and about 1.7 GB of memory for the English
model on the CPU. Load once and keep the agent; do not create one per question.

Several agents can exist at once, each with its own copy of the model.

On failure the message starts with `cannot load Laya model:` followed by the library's
reason.

## agent:ask_yes_no

```lua
json   = agent:ask_yes_no(state, instructions [, id])
answer = agent:ask_yes_no_table(state, instructions [, id])
```

| Argument | Meaning |
|---|---|
| `state` | The text to decide about. A string, or a table that is sent as a JSON object. |
| `instructions` | The question. A string. |
| `id` | The name of the answer. A string; the default is `"q"`. |

The answer is at `results[1].answers[id]`, with the fields `noul` (the probability of yes)
and `confidence`.

To say what yes and no mean for a question, use `predict` with `criteria`
([Asking questions](asking-questions.md#describing-the-options)).

## agent:ask_choice

```lua
json   = agent:ask_choice(state, instructions, options [, id])
answer = agent:ask_choice_table(state, instructions, options [, id])
```

`options` is a table: a list of 2 to 255 strings. The answer has `choice` (the winning
option), `probabilities` (by option name) and `confidence`.

## agent:ask_score

```lua
json   = agent:ask_score(state, instructions, levels [, id])
answer = agent:ask_score_table(state, instructions, levels [, id])
```

`levels` is a table: a list of 2 to 255 strings, lowest level first. The answer has `score`
(the average level, counted from 0), `probabilities` and `legend` (both keyed by the level
number as a string) and `confidence`.

## agent:predict

```lua
json   = agent:predict(request)
answer = agent:predict_table(request)
```

`request` is a table or JSON text: one request, or a list of requests. A request has `state`
and `questions`; see [Asking questions](asking-questions.md#several-questions-several-texts-predict).

`results` has one entry per request, in the order they were given. A single request also
comes back as a list of one.

A table is converted to JSON by the rules in [Tables and JSON](json.md). A string is passed
to the library as it is.

## agent:info

```lua
json = agent:info()
info = agent:info_table()
```

Describes the loaded model:

| Field | Meaning |
|---|---|
| `backend` | What runs the model: `CPU`, or for example `Vulkan0`. |
| `device` | The processor or GPU, by name. |
| `model_dir` | The folder the model was loaded from. |
| `model_name` | The name in the model's configuration. |
| `variant` | `english`, `multilingual` or `typed-decisions`. |
| `max_len` | The most tokens one question plus its text may take (512). |
| `head_max_len` | The most tokens the question part may take (192). |

```lua
local info = assert(agent:info_table())
print(info.backend, info.device)
```

## agent:prepare

```lua
json  = agent:prepare(request)
input = agent:prepare_table(request)
```

Takes the same requests as `predict` but does not run the model. It returns the token
numbers the model would receive. It is a debugging aid, useful for seeing how many tokens a
text takes:

```lua
local input = assert(agent:prepare_table({
  state = "Please refund the duplicate charge.",
  questions = { q = { type = "noul", instructions = "Does the customer ask for a refund?" } },
}))
print(input.lengths[1])     -- tokens in the first question, text included
```

## agent:close

```lua
agent:close()
```

Unloads the model and frees its memory at once. Calling it twice is harmless. Afterwards
every other method returns `nil, "the agent is closed"`.

If `close` is never called, the model is unloaded when the garbage collector collects the
agent. Because a model holds about 1.7 GB that Lua's collector does not know about, call
`close` yourself when you are done with an agent in a long-running program.

In Lua 5.4 an agent can be declared as a to-be-closed variable:

```lua
do
  local agent <close> = assert(llaya.new("/models/laya"))
  -- ...
end   -- the model is unloaded here
```

`tostring(agent)` gives `llaya.agent: 0x…`, or `llaya.agent (closed)`.

## llaya.version and llaya.api_version

```lua
llaya.version()        --> "laya_c 1.0.14 (api 1; backends: cpu)"
llaya.api_version()    --> 1
llaya._VERSION         --> "LLaya 0.1.0"
```

`llaya.version()` is the version of the **library** that was loaded and the backends it
contains. A library that lists `vulkan` can use the GPU. `llaya._VERSION` is the version of
the module itself.

The module checks the API version when it is loaded and refuses a library whose version is
not 1.

## llaya.encode and llaya.decode

```lua
text  = llaya.encode(value)     --> JSON text | nil, message
value = llaya.decode(text)      --> Lua value | nil, message
```

The module's own JSON functions, the same ones the `_table` methods use. They are there so
a program needs no JSON library for LLaya; they are not meant to replace one. The rules are
in [Tables and JSON](json.md).

## llaya.null

A unique value that stands for JSON `null`. `llaya.decode` and the `_table` methods produce
it wherever the JSON has `null`, and `llaya.encode` writes it back as `null`. See
[Tables and JSON](json.md#null).

## Things to know

* **Calls block.** A method returns when the model has answered, and the Lua state waits
  meanwhile. A coroutine does not change that. For throughput, send several requests in one
  `predict` call.
* **Strings are UTF-8** and are passed through unchanged.
* **An agent belongs to the Lua state that created it.** A program with several Lua states
  can load the module in each; every state then creates its own agents.
* **The log.** The library writes warnings to the standard error stream. The module does not
  offer a way to redirect them.
* **Debugging.** With `LAYA_DEBUG=1` in the environment the library prints what it is doing
  at every stage of every call ([Troubleshooting](troubleshooting.md#a-call-never-returns)).
