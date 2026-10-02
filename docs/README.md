# LLaya documentation

LLaya lets a Lua program ask the Laya AI model questions about a piece of text: yes or no,
one of several options, or a level on a scale. The model runs inside the program, through the
LibLayaX library. These pages are about using the module; the [project README](../README.md)
says what the project is.

## Start here

| Page | Read it when |
|---|---|
| [Getting started](getting-started.md) | You want a first answer: which three things to download, where to put them, a first script. |
| [Asking questions](asking-questions.md) | You are writing the questions: the three kinds, several at once, many texts in one call, reading the answers. |

## Reference

| Page | Content |
|---|---|
| [The module](reference.md) | Every function and method: arguments, results, errors. |
| [Options](options.md) | What can be passed to `llaya.new`: CPU or GPU, precision, threads, model variant. |
| [Tables and JSON](json.md) | How Lua tables become JSON and back, `llaya.null`, `llaya.encode` and `llaya.decode`. |
| [Installing](installing.md) | Where the files go, how the module finds the library, the Lua DLL on Windows, Lua inside another application. |

## When you need it

| Page | Content |
|---|---|
| [Troubleshooting](troubleshooting.md) | Error messages and what to do about them. |
| [Testing](testing.md) | The test suite, and what has been tested on which system. |
| [Building](building.md) | Building the module for your Lua version or for another platform. |

## The short version

```lua
local llaya = require "llaya"
local agent = assert(llaya.new("/models/laya"))

local answer = assert(agent:ask_yes_no_table("Please refund the duplicate charge.",
                                             "Does the customer ask for a refund?", "refund"))
print(answer.results[1].answers.refund.noul)        --> 0.8364
```

Load the model once, ask as many questions as you like. Every method comes in two forms: the
plain name returns the answer as JSON text, the name ending in `_table` returns it as a Lua
table.

**Names.** LLaya is the project and `llaya` is the module (`llaya.dll` on Windows, `llaya.so`
on Linux and macOS). It has two Ls because `laya.dll` is the LibLayaX library itself, which
the module loads.
