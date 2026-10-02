# Asking questions

The model reads a piece of text and answers questions about it. There are three kinds of
question, and a helper method for each. For more than one question at a time there is
`predict`.

Every method exists in two forms. The plain name returns the answer as **JSON text**; the
name ending in `_table` returns the same answer as a **Lua table**. The examples here use the
table form. The example answers were produced by the English model.

```lua
local llaya = require "llaya"
local agent = assert(llaya.new("/models/laya"))
```

## Yes or no

```lua
local answer = assert(agent:ask_yes_no_table(
  "Please refund the duplicate charge.",
  "Does the customer ask for a refund?",
  "refund"))

local refund = answer.results[1].answers.refund
print(refund.noul)          --> 0.8364    the probability of "yes"
print(refund.confidence)    --> 0.8364    how far from undecided, 0.5 to 1
```

The arguments are the text, the question, and a name for the answer. The name is optional
and defaults to `"q"`; the answer is then at `answers.q`.

Treat `noul` above 0.5 as yes, or choose your own threshold: a higher one when a wrong yes
is expensive.

## One of several

```lua
local answer = assert(agent:ask_choice_table(
  "I want to cancel my subscription.",
  "What does the customer want?",
  { "cancel", "upgrade", "refund" },
  "intent"))

local intent = answer.results[1].answers.intent
print(intent.choice)                  --> cancel
print(intent.probabilities.cancel)    --> 0.9649
print(intent.probabilities.refund)    --> 0.0304
print(intent.confidence)              --> 0.8491
```

The third argument is the list of options, from 2 to 255 of them. `choice` is the winner,
`probabilities` has every option, and `confidence` is 1 when one option takes everything
and 0 when all are equally likely.

## A level on a scale

```lua
local answer = assert(agent:ask_score_table(
  "This is the third time I am writing!!!",
  "How angry is the customer?",
  { "calm", "annoyed", "furious" },
  "anger"))

local anger = answer.results[1].answers.anger
print(anger.score)                --> 0.8568
print(anger.probabilities["1"])   --> 0.6423
print(anger.legend["1"])          --> annoyed
```

The third argument is the list of levels, lowest first. Levels are numbered from 0, so with
three levels `score` runs from 0 to 2. It is an average weighted by probability: 0.8568 is a
little below "annoyed".

`probabilities` and `legend` are keyed by the level number **as a string** (`"0"`, `"1"`,
`"2"`), because that is how they arrive in the JSON. To get the single most likely level:

```lua
local best, best_p = nil, -1
for level, p in pairs(anger.probabilities) do
  if p > best_p then best, best_p = level, p end
end
print(anger.legend[best])         --> annoyed
```

## The text can be a table

The first argument of the three helpers is normally a string. It can also be a table, which
is sent as a JSON object; the model reads it as text.

```lua
local ticket = { subject = "Invoice", body = "I was charged twice this month." }
local answer = assert(agent:ask_yes_no_table(ticket, "Is this about billing?"))
print(answer.results[1].answers.q.noul)
```

## Several questions, several texts: predict

The helpers ask one question about one text. `predict` takes a full request, written as a
Lua table (or as JSON text, if you already have it):

```lua
local answer = assert(agent:predict_table({
  state = "Please refund the duplicate charge.",
  questions = {
    refund = { type = "noul",  instructions = "Does the customer ask for a refund?" },
    tone   = { type = "score", instructions = "How angry is the customer?",
               criteria = { "calm", "annoyed", "furious" } },
  },
}))

print(answer.results[1].answers.refund.noul)
print(answer.results[1].answers.tone.score)
```

A request has these fields:

| Field | Meaning |
|---|---|
| `state` | The text: a string, or a table. |
| `questions` | A table with one entry per question. Each key is the name the answer comes back under. |
| `type` | `"noul"` (yes or no), `"choice"` or `"score"`. |
| `instructions` | The question, in plain words. |
| `criteria` | The options of a `choice`, or the levels of a `score`. Optional for `noul`. |

### Many texts in one call

Give `predict` a list of requests, and `results` has one entry per request, in the same
order:

```lua
local messages = { "Refund me.", "Thanks, all good.", "Where is my parcel?" }

local requests = {}
for i, text in ipairs(messages) do
  requests[i] = {
    state = text,
    questions = { refund = { type = "noul", instructions = "Is a refund requested?" } },
  }
end

local answer = assert(agent:predict_table(requests))
for i, result in ipairs(answer.results) do
  print(messages[i], result.answers.refund.noul)
end
```

**On a GPU this is how to get speed.** A GPU works on all the questions of a call together,
so one call with sixteen questions is several times faster than sixteen calls. On the CPU a
batch is convenient but hardly faster per question.

If one request in the list is not valid, the whole call fails and returns `nil` and a
message, with no results.

### Describing the options

A `choice` can describe each option. The model reads the descriptions; the answer uses the
names. In a Lua table the order of such entries is not defined, so when the order matters to
you, write this part as JSON text:

```lua
local answer = assert(agent:predict_table([[
  {"state": "I want to cancel my subscription.",
   "questions": {"intent": {"type": "choice",
     "instructions": "What does the customer want?",
     "criteria": {"cancel":  "wants to end the subscription",
                  "upgrade": "wants a bigger plan",
                  "other":   null}}}}
]]))
print(answer.results[1].answers.intent.choice)
```

A `noul` question can say what yes and no mean:

```lua
criteria = { ["true"] = "money back is requested", ["false"] = "no money back is requested" }
```

## The whole answer

```lua
answer.results            -- a list, one entry per request
answer.results[1].answers -- one entry per question, by name
answer.results[1].usage.input_tokens   -- how many tokens the model read for this request
answer.elapsed_ms         -- time the call took in the engine, in milliseconds
answer.backend            -- "CPU", or for example "Vulkan0"
answer.device             -- the name of the processor or GPU that did the work
```

Every answer to a question has `type`, `confidence` and `action.act_probability` (the output
of the model's action head, passed through unchanged; its meaning is defined by the Laya
project), plus the fields of its kind shown above. All numbers are rounded to four decimals.

The same answer as JSON text, which is what the methods without `_table` return:

```json
{"results":[{"model":"laya-rl-agent",
             "answers":{"refund":{"type":"noul","confidence":0.8364,
                                  "action":{"act_probability":1.0},"noul":0.8364}},
             "usage":{"input_tokens":40,"output_tokens":0}}],
 "elapsed_ms":117.8,"backend":"CPU","device":"Intel(R) Core(TM) Ultra 9 275HX"}
```

## How long a text can be

The model reads at most 512 tokens per question, the question included. A token is a word
or a piece of a word; that leaves room for a text of roughly 350 English words with a short
question. A longer text is refused:

```lua
print(agent:ask_yes_no(string.rep("word ", 3000), "Is it long?"))
--> nil     Question 'q' exceeds state context limit (475 tokens)
```

The number in the message is how many tokens were left for the text after the question.

Two ways to handle long texts:

* Split the text yourself and ask about each part, for example per paragraph.
* Load the model with `allow_truncation = true` ([Options](options.md)). The library then
  cuts off what does not fit and answers about the beginning of the text.

The other limits: the question with all its options may take 192 tokens, one option 48
tokens, and a question may have 2 to 255 options.

## When a question fails

A request the library cannot answer returns `nil` and a message. The agent stays usable.

```lua
local answer, message = agent:predict_table({
  state = "x",
  questions = { q = { type = "essay", instructions = "?" } },
})
print(answer, message)    --> nil     Unsupported question type: essay
```

The common messages are listed in [Troubleshooting](troubleshooting.md#a-question-returns-nil).

The request and answer format is the same for every LibLayaX binding. The LibLayaX
documentation (`docs/requests.md` in that repository) describes it in full.
