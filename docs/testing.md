# Testing

## The test kits

Each release has two archives that need nothing installed. Every folder in them is complete
for one platform: the Lua 5.1.4 interpreter, the `llaya` module, the LibLayaX library, the
scripts and a `run-tests` script that runs them.

`LLaya-0.1.0-testkit-cpu.tar.xz`, with CPU-only libraries:

| Folder | For |
|---|---|
| `windows-x64` | 64-bit Windows, processor with AVX2 (Intel since 2013, AMD since Zen) |
| `windows-x64-compat` | 64-bit Windows on any processor, and x64 programs on Windows on ARM |
| `windows-arm64` | Windows on ARM, native |
| `linux-x64` | Linux x86-64, processor with AVX2 |
| `linux-x64-compat` | Linux x86-64, any processor |
| `linux-arm64` | Linux ARM64 |
| `macos-arm64` | macOS on Apple Silicon |

`LLaya-0.1.0-testkit-gpu.tar.xz`, with libraries that have the GPU backend and the CPU
backend:

| Folder | For |
|---|---|
| `windows-x64` | 64-bit Windows with an NVIDIA, AMD or Intel GPU |
| `linux-x64` | Linux x86-64 with a GPU |
| `macos-arm64` | macOS on Apple Silicon (MoltenVK is included) |

Unpack with `tar -xf <archive>`; Windows 11 and macOS also open the archive with a double
click, and older Windows needs 7-Zip. Then, from the folder for your system:

```
run-tests.bat D:\models\laya          Windows
sh run-tests.sh /models/laya          Linux, macOS
```

| Kit | What `run-tests` does |
|---|---|
| CPU | 1. the test suite on the CPU; 2. the speed test on the CPU |
| GPU | 1. the test suite on the GPU, full precision; 2. to 4. the speed test on the GPU with `fp16`, `bf16` and `fp32`; 5. the speed test on the CPU, for comparison |

A step that cannot run on the machine (no GPU, or a precision the GPU does not support)
prints `NOT RUN:` with the reason, and the script goes on to the next step. To keep the
output, add `> log.txt 2>&1`.

On macOS the script removes the download quarantine from the folder and, in the GPU kit, sets
`MVK_CONFIG_LOG_LEVEL=1` so that MoltenVK does not print its 180 lines of information.

The kits contain the standard Lua 5.1.4 interpreter only so that they run on a machine
without Lua. The license texts of everything inside are in the `licenses` folder of each
kit.

## The speed test

`speed.lua` is in every kit folder. It measures one backend and precision:

```
lua speed.lua /models/laya                  the CPU
lua speed.lua /models/laya vulkan fp16      the GPU, half precision
lua speed.lua /models/laya vulkan bf16
lua speed.lua /models/laya vulkan fp32      the GPU, full precision
```

| Line it prints | Meaning |
|---|---|
| `backend: … device: … precision: …` | What is really in use. On a GPU the backend is `Vulkan0`, `Vulkan1`, … and the device is the GPU's name. |
| `first question after loading` | The first call, which on a GPU includes one-time setup and can take a second or more. |
| `one question alone` | A single question, best of three. |
| `batch of 16` | Sixteen questions in one call: the time per question, and questions per second. This is the number that shows what a GPU can do. |
| `answer: noul = …` | The answer to the reference question, alone and as the last of the batch. |
| `CPU reference … OK` or `DIFFERENT` | With the english model: whether the answer is within 0.01 of the CPU's 0.8364. |

Times are the library's own measurement (`elapsed_ms` in the answer).

## Running the tests

The test suite is one Lua file, `test/test.lua` (`test.lua` in the kits and in the binaries
zip). `run-tests` in a kit runs it for you; by hand, run it from a folder that contains the
module and the LibLayaX library:

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
  full precision, which is what the test suite asks for. The GPU kit's `run-tests` measures
  them;
* LuaJIT;
* the `multilingual` and `typed-decisions` models;
* Lua 5.2, 5.3 and 5.4 on anything but Linux x86-64.

The GPU run on the Mac gives 0.8366 where the CPU gives 0.8364; on the RTX 5080 the GPU and
the CPU agree to four decimals. A small difference between a GPU and the CPU is normal.

If you run a kit on one of these, the output of `run-tests` is a useful report.
