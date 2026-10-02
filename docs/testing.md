# Testing

## Running the tests

The test suite is one Lua file, `test/test.lua` (`test.lua` in the binaries zip). Run it
from a folder that contains the module and the LibLayaX library:

```
lua test.lua                        26 checks, no model needed
lua test.lua /models/laya           50 checks, on the CPU
lua test.lua /models/laya vulkan    the same on another backend
```

The environment variables `LAYA_TEST_MODEL` and `LAYA_TEST_BACKEND` do the same as the two
arguments.

It prints the versions it found and ends with the result:

```
module:  LLaya 0.1.0 on Lua 5.1
library: laya_c 1.0.14 (api 1; backends: cpu)
…
50 checks, 0 failures
```

The exit code is 0 when everything passed.

What it covers:

* **Without a model**: the module loads the library, the version functions, the JSON encoder
  and decoder (arrays, objects, escapes, numbers, `null`, invalid input), and the errors
  for a model folder that does not exist and for wrong options.
* **With a model**: all three kinds of question, as JSON text and as tables; requests given
  as tables and as strings; a batch; Unicode text; error reporting for bad requests, and
  that the agent still works after them; `prepare`; `info`; `close`; loading a model again
  after closing; and unloading by the garbage collector.

The suite prints the answer to the refund example. With the English model it is `noul`
0.8364 on the CPU, on every platform; the suite itself only checks that the answer is a
valid probability, so compare that number by eye.

`examples/ask.lua` (`ask.lua` in the zip) is a small program to try by hand:

```
lua ask.lua /models/laya
```

## What has been tested

All with LibLayaX 1.0.14. "Real model" means the English model from Hugging Face; otherwise
a synthetic test model was used, which has the real architecture but random weights.

| Lua | Platform | Model | Result |
|---|---|---|---|
| 5.1.4 | Windows ARM64 (Windows 11 on ARM, Parallels on Apple Silicon) | real | 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | Windows 11 x64 PC (Intel Core Ultra 9 275HX), on the CPU | real | 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | Windows 11 x64 PC, on the GPU (NVIDIA RTX 5080 Laptop GPU, `vulkan` backend, full precision) | real | 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | Windows x64 module under x64 emulation (Windows 11 on ARM) | real | 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | Linux ARM64 (Ubuntu 24.04, Parallels on Apple Silicon) | real | 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | macOS, Apple M3 Ultra, on the CPU | real | 50 checks, 0 failures, `noul` 0.8364 |
| 5.1.4 | macOS, Apple M3 Ultra, on the GPU (`vulkan` backend, full precision) | real | 50 checks, 0 failures, `noul` 0.8366 |
| 5.1.4 | Windows ARM64 and x64, program on `lua5.1.dll` with a forwarding `lua51.dll` | real | 50 checks, 0 failures on both |
| 5.1.4 | Linux x86-64 (the build machine) | synthetic | 50 checks, 0 failures; also clean under the address and undefined-behaviour sanitizers, including a few thousand malformed JSON inputs |
| 5.2.3, 5.3.6, 5.4.9 | Linux x86-64 (the build machine) | synthetic | full run, 0 failures |

Not yet done:

* the Linux x86-64 module away from the build machine, and with the real model;
* the GPU backend on Linux through LLaya;
* the half precisions (`fp16`, `bf16`) through LLaya on any system: the GPU runs above used
  full precision, which is what the test suite asks for;
* LuaJIT;
* the `multilingual` and `typed-decisions` models;
* Lua 5.2, 5.3 and 5.4 on anything but Linux x86-64.

The GPU run on the Mac gives 0.8366 where the CPU gives 0.8364; on the RTX 5080 the GPU and
the CPU agree to four decimals. A small difference between a GPU and the CPU is normal.

If you run the tests on one of these, the output of `lua test.lua /models/laya` is a useful
report.
