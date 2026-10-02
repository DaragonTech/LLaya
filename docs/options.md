# Options

The second argument of `llaya.new` sets how the model runs. It is a table, or the same as
JSON text. Without it the model runs on the CPU with all cores.

```lua
local agent = assert(llaya.new("/models/laya", { backend = "vulkan", precision = "fp16" }))
```

| Key | Values | Default |
|---|---|---|
| `backend` | `"cpu"`, `"vulkan"`, `"cuda"` | `"cpu"` |
| `precision` | `"fp32"`, `"fp16"`, `"bf16"` | `"fp32"` |
| `device` | a GPU number, or part of its name | the first discrete GPU |
| `threads` | 0 to 1024 | 0, meaning all cores |
| `variant` | `"english"`, `"multilingual"`, `"typed-decisions"` | the folder as given |
| `allow_truncation` | `true`, `false` | `false` |
| `flash`, `tensor_core` | `true`, `false` | set automatically |

A key that is not in this table is refused, so a typing mistake is reported:

```lua
print(llaya.new("/models/laya", { backnd = "cpu" }))
--> nil     cannot load Laya model: Unknown option: backnd
```

Options are fixed when the model is loaded. To change one, close the agent and create
another.

## Running on the GPU

The GPU is reached through Vulkan, and only a library that was built with it can do so. Use
the LibLayaX `vulkan` package (Windows x64, Linux x86-64) or the macOS GPU package.
`llaya.version()` shows what the loaded library contains:

```lua
print(llaya.version())      --> laya_c 1.0.14 (api 1; backends: cpu,vulkan)
```

Then ask for it, with a half precision, which is much the fastest:

```lua
local agent = assert(llaya.new("/models/laya", { backend = "vulkan", precision = "fp16" }))
print(assert(agent:info_table()).device)      -- the GPU that was chosen
```

| System | Recommended |
|---|---|
| Windows or Linux with an NVIDIA, AMD or Intel GPU | `{ backend = "vulkan", precision = "fp16" }` |
| Mac with Apple Silicon | `{ backend = "vulkan", precision = "bf16" }` |

A `vulkan` library also works on machines without a GPU, so a program can try the GPU and
fall back:

```lua
local agent = llaya.new(model, { backend = "vulkan", precision = "fp16" })
           or assert(llaya.new(model, { backend = "cpu" }))
```

On a GPU, send several questions per call; that is where the speed comes from
([Asking questions](asking-questions.md#many-texts-in-one-call)).

To find out which precision is fastest on a particular machine, run the GPU test kit: it
measures `fp16`, `bf16` and `fp32` there and compares each answer with the CPU's
([Testing](testing.md#the-test-kits)).

`"cuda"` exists in the engine but is not in any released LibLayaX package.

Through LLaya the GPU backend has been run on an NVIDIA RTX 5080 Laptop GPU (Windows 11) and
on an Apple M3 Ultra (macOS), with full precision: the whole test suite passes on both. The
half precisions, and the GPU on Linux, have not yet been run from Lua; the half precisions
are the library's own and have been run through its C interface on both of those GPUs.

On a laptop with an integrated and a dedicated GPU the library picks the dedicated one by
itself. In that test the answer reported `"backend":"Vulkan1"`: the number is the GPU's
position in the system's list, and the RTX was the second entry.

On a Mac the first use of the GPU prints about 180 lines of information from MoltenVK, the
layer between Vulkan and Apple's Metal. `MVK_CONFIG_LOG_LEVEL=1` in the environment switches
them off. See
[Troubleshooting](troubleshooting.md#macos-many-lines-of-mvk-info-on-the-gpu).

## The keys one by one

**`backend`** chooses the processor (`"cpu"`) or the GPU (`"vulkan"`). Asking for a backend
the library does not have fails with `This build has no Vulkan backend`.

**`precision`** is the number format used on the GPU. `"fp32"` is full precision and the
only choice on the CPU. `"fp16"` and `"bf16"` are half precisions for the GPU: several times
faster, with probabilities within about 0.003 of the CPU's in the measured runs.

**`device`** picks a GPU on a machine with more than one: a number counted from 0, or part
of the GPU's name such as `"RTX"`. Without it the first discrete GPU is used.

**`threads`** is the number of processor threads for the CPU backend. Lower it when the
program must leave cores free for other work.

**`variant`** selects one of the three published models when `model_dir` is the top folder
of the download: `"english"` is in the top folder itself, `"multilingual"` and
`"typed-decisions"` are in subfolders of those names. Giving the subfolder as `model_dir`
does the same. Only the English model has been run so far.

**`allow_truncation`** changes what happens with a text that is too long: with `true` it is
cut to fit and answered, without it the request is refused
([Asking questions](asking-questions.md#how-long-a-text-can-be)).

**`flash`** and **`tensor_core`** are settings of the engine's GPU code that are chosen
automatically and rarely need touching.

The LibLayaX documentation (`docs/options.md` in that repository) describes every option in
full, with the combinations that are refused.
