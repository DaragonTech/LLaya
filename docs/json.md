# Tables and JSON

The LibLayaX library speaks JSON. LLaya converts in both directions so that a Lua program
can work with tables: requests may be given as tables, and the methods ending in `_table`
return answers as tables. The same converter is available directly as `llaya.encode` and
`llaya.decode`.

If your program already uses a JSON library, you can ignore all of this: pass JSON text to
`predict`, use the methods without `_table`, and decode the result with your own library.

## From JSON to Lua

| JSON | Lua |
|---|---|
| object `{"a": 1}` | table `{ a = 1 }` |
| array `[10, 20]` | table `{ 10, 20 }`, indexed from 1 |
| string | string, UTF-8 |
| number | number; in Lua 5.3 and later a whole number becomes an integer |
| `true`, `false` | boolean |
| `null` | `llaya.null` |

```lua
local t = assert(llaya.decode('{"a":[1,2,null],"b":null,"c":1.5}'))
print(#t.a)                     --> 3
print(t.a[3] == llaya.null)     --> true
print(t.c)                      --> 1.5
```

Invalid JSON gives `nil` and a message with the position of the problem:

```lua
print(llaya.decode('{"a":'))    --> nil     unexpected end of JSON at position 6
```

## null

JSON `null` cannot become Lua `nil`: a `nil` in a table means "no such key", so the key
would disappear, and a list with a `nil` in it would lose its length. The decoder therefore
uses a special value, `llaya.null`.

```lua
if value == llaya.null then … end
```

Note that `llaya.null` is not false: `if t.b then` is true when `t.b` is `llaya.null`.
Compare with `llaya.null` explicitly.

In the answers of the model, `null` does not normally occur, so this matters mainly when you
use `llaya.decode` for your own data.

## From Lua to JSON

| Lua | JSON |
|---|---|
| table whose keys are exactly 1, 2, … n | array |
| any other table | object; keys must be strings or numbers |
| empty table `{}` | object `{}` |
| string | string, with quotes, backslashes and control characters escaped |
| number | number |
| boolean | `true`, `false` |
| `nil`, `llaya.null` | `null` |

```lua
print(llaya.encode({ 1, 2, "x" }))            --> [1,2,"x"]
print(llaya.encode({ a = true }))             --> {"a":true}
print(llaya.encode({}))                       --> {}
print(llaya.encode({ [1] = "a", [3] = "c" })) --> {"1":"a","3":"c"}
print(llaya.encode("say \"hi\"\n"))           --> "say \"hi\"\n"
```

Things that follow from these rules:

* **A list with a gap is not a list.** `{ [1] = "a", [3] = "c" }` is written as an object
  with the keys `"1"` and `"3"`. Keep lists dense.
* **A table with both list entries and named entries is an object.** `{ 10, 20, n = 2 }`
  becomes `{"1":10,"2":20,"n":2}`.
* **An empty table is an object.** There is no way to write an empty array from a table; the
  library never needs one. If your own data does, write that part as JSON text.
* **The order of the keys of an object is not defined**, as with `pairs`. JSON does not give
  meaning to that order, with one exception here: the options of a `choice` question given
  as an object with descriptions. See
  [Asking questions](asking-questions.md#describing-the-options).
* **Number keys become strings**, because JSON keys are always strings. That is why the
  `probabilities` of a `score` answer are read as `probabilities["1"]`.

What cannot be written:

| Value | Message |
|---|---|
| a function, a userdata, a thread | `only nil, booleans, numbers, strings and tables can be written as JSON` |
| a table with a boolean or table as key | `table keys must be strings or numbers to be written as JSON` |
| not-a-number or infinity | `cannot encode NaN or infinity as JSON` |
| a table nested more than 100 levels, or one that contains itself | `table is nested too deeply (or refers to itself)` |

Each gives `nil` and the message.

## Numbers

Whole numbers are written without a decimal point, other numbers in the shortest form that
reads back as the same value. In Lua 5.3 and later, integers and floats are kept apart in
both directions.

## Strings and tables as requests

Wherever a method accepts a request or options, a string and a table are treated
differently:

* a **table** is converted to JSON by the rules above;
* a **string** is taken to be JSON text already and is passed to the library unchanged.

So these two calls are the same:

```lua
agent:predict({ state = "Hello", questions = { q = { type = "noul", instructions = "A greeting?" } } })
agent:predict('{"state":"Hello","questions":{"q":{"type":"noul","instructions":"A greeting?"}}}')
```

In the three `ask_` helpers the first two arguments are ordinary text, not JSON: the module
escapes them for you, so quotes and line breaks in a message are safe.
