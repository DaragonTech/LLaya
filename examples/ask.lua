-- Loads a model and asks three questions about one message.
--
--   lua ask.lua /path/to/model [cpu|vulkan|cuda]
--
-- Run it from the folder that contains the llaya module and the LibLayaX library.

local llaya = require "llaya"

local model = arg[1]
if not model then
  io.stderr:write("usage: lua ask.lua MODEL_DIR [cpu|vulkan|cuda]\n")
  os.exit(2)
end

print("library: " .. llaya.version())
local agent = assert(llaya.new(model, { backend = arg[2] or "cpu" }))
print("model:   " .. agent:info())

local message = "This is the third time I am writing. Please refund the duplicate charge!"
print("message: " .. message)

-- as a Lua table ...
local answer = assert(agent:ask_yes_no_table(message, "Does the customer ask for a refund?", "refund"))
print(string.format("refund requested:  P(yes) = %.4f", answer.results[1].answers.refund.noul))

answer = assert(agent:ask_choice_table(message, "What does the customer want?", { "cancel", "upgrade", "refund" }, "intent"))
print("intent:            " .. answer.results[1].answers.intent.choice)

answer = assert(agent:ask_score_table(message, "How angry is the customer?", { "calm", "annoyed", "furious" }, "anger"))
print(string.format("anger (0 to 2):    %.2f", answer.results[1].answers.anger.score))
print(string.format("time:              %.0f ms for the last question", answer.elapsed_ms))

-- ... or as JSON text, for the JSON library you already use
print("as JSON:           " .. assert(agent:ask_yes_no(message, "Does the customer ask for a refund?", "refund")))

agent:close()
