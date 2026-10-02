-- Tests for LLaya. Run from the folder that contains the llaya module and the LibLayaX library:
--
--   lua test.lua                      model-free checks only
--   lua test.lua /path/to/model       full run on the CPU
--   lua test.lua /path/to/model vulkan
--
-- (LAYA_TEST_MODEL and LAYA_TEST_BACKEND in the environment work too.) Exit code 0 = all passed.

local llaya = require "llaya"

local checks, failures = 0, 0
local function check(condition, what)
  checks = checks + 1
  if not condition then
    failures = failures + 1
    print("FAIL: " .. tostring(what))
  end
end
local function contains(text, needle)
  return type(text) == "string" and text:find(needle, 1, true) ~= nil
end
-- The "results" member only: elapsed_ms differs from run to run.
local function results_of(json)
  local first = json:find('"results":', 1, true)
  local last = json:find(',"elapsed_ms":', 1, true)
  return first and last and json:sub(first, last - 1) or nil
end

print("module:  " .. llaya._VERSION .. " on " .. _VERSION)
print("library: " .. llaya.version())
check(contains(llaya._VERSION, "LLaya"), "_VERSION")
check(contains(llaya.version(), "laya_c "), "version")
check(llaya.api_version() == 1, "api_version")

-- JSON encode / decode -------------------------------------------------------------------
check(llaya.encode({ 1, 2, "x" }) == '[1,2,"x"]', "encode array: " .. tostring(llaya.encode({ 1, 2, "x" })))
check(llaya.encode({ a = true }) == '{"a":true}', "encode object")
check(llaya.encode({}) == "{}", "encode empty table")
check(llaya.encode("a\"b\\c\n\1") == '"a\\"b\\\\c\\n\\u0001"', "encode string escapes")
check(llaya.encode(0.5) == "0.5" and llaya.encode(3) == "3" and llaya.encode(-1e300) == "-1e+300", "encode numbers")
check(llaya.encode(llaya.null) == "null" and llaya.encode(nil) == "null", "encode null")
check(llaya.encode({ [1] = "a", [3] = "c" }) ~= nil, "a table with holes is written as an object")
local value, message = llaya.encode(print)
check(value == nil and contains(message, "JSON"), "encode rejects functions")
local loop = {}
loop.self = loop
value, message = llaya.encode(loop)
check(value == nil and contains(message, "nested"), "encode rejects a table that contains itself")
value, message = llaya.encode(0 / 0)
check(value == nil and contains(message, "NaN"), "encode rejects NaN")

local t = llaya.decode('{"a":[1,2,{"b":null}],"s":"caf\\u00e9 \\ud83d\\ude00","n":-1.5e2,"t":true,"f":false,"e":{},"l":[]}')
check(type(t) == "table" and #t.a == 3 and t.a[1] == 1 and t.a[3].b == llaya.null, "decode nested")
check(t.s == "caf\195\169 \240\159\152\128", "decode \\u escapes to UTF-8")
check(t.n == -150 and t.t == true and t.f == false, "decode numbers and booleans")
check(type(t.e) == "table" and next(t.e) == nil and type(t.l) == "table", "decode empty object and array")
check(llaya.decode(" 42 ") == 42 and llaya.decode('"x"') == "x", "decode scalars")
value, message = llaya.decode("[1,2")
check(value == nil and contains(message, "position"), "decode reports errors: " .. tostring(message))
value, message = llaya.decode('{"a":1} x')
check(value == nil, "decode rejects trailing text")
local round = llaya.decode(llaya.encode({ list = { 1, 2, 3 }, name = "x", nested = { ok = true } }))
check(round.list[3] == 3 and round.name == "x" and round.nested.ok == true, "encode/decode round trip")
if math.type then -- Lua 5.3+: integers stay integers
  check(math.type(llaya.decode("7")) == "integer" and math.type(llaya.decode("7.5")) == "float", "integer subtype")
end

-- Loading failures are values, not errors --------------------------------------------------
local agent
agent, message = llaya.new("/no/such/model")
check(agent == nil and contains(message, "rl_agent_config.json"), "missing model: " .. tostring(message))
agent, message = llaya.new("/no/such/model", { backend = "tpu" })
check(agent == nil and contains(message, "Unknown backend"), "bad backend (table options): " .. tostring(message))
agent, message = llaya.new("/no/such/model", '{"nope":1}')
check(agent == nil and contains(message, "Unknown option"), "unknown option (JSON options): " .. tostring(message))
agent, message = llaya.new("/no/such/model", 42)
check(agent == nil and contains(message, "table"), "options of the wrong type")
check(not pcall(llaya.new), "llaya.new() without a folder raises an argument error")

-- Model tests ---------------------------------------------------------------------------
local model = arg and arg[1] or os.getenv("LAYA_TEST_MODEL")
local backend = arg and arg[2] or os.getenv("LAYA_TEST_BACKEND") or "cpu"
if model and model ~= "" then
  agent = assert(llaya.new(model, { backend = backend }))
  check(contains(tostring(agent), "llaya.agent"), "tostring")

  local info = agent:info()
  print("info: " .. tostring(info))
  check(contains(info, '"backend":') and contains(info, '"max_len":'), "info")
  local info_table = agent:info_table()
  check(type(info_table) == "table" and type(info_table.max_len) == "number" and info_table.model_dir ~= nil, "info_table")

  -- the three question types, as JSON text and as tables
  local json = agent:ask_yes_no("Please refund the duplicate charge.", "Does the customer ask for a refund?", "refund")
  print("yes/no: " .. tostring(json))
  check(contains(json, '"refund":{"type":"noul"') and contains(json, '"noul":'), "ask_yes_no")
  local answer = agent:ask_yes_no_table("Please refund the duplicate charge.", "Does the customer ask for a refund?", "refund")
  local p = answer and answer.results[1].answers.refund.noul
  check(type(p) == "number" and p >= 0 and p <= 1, "ask_yes_no_table: noul = " .. tostring(p))
  check(results_of(json) == results_of(agent:ask_yes_no("Please refund the duplicate charge.", "Does the customer ask for a refund?", "refund")), "same question, same answer")

  answer = agent:ask_choice_table("I want to cancel my subscription.", "What does the customer want?", { "cancel", "upgrade", "refund" }, "intent")
  local intent = answer and answer.results[1].answers.intent
  print("choice: " .. tostring(intent and intent.choice))
  check(intent and type(intent.choice) == "string" and type(intent.probabilities.cancel) == "number", "ask_choice_table")
  check(contains(agent:ask_choice("I want to cancel.", "What does the customer want?", { "cancel", "upgrade" }), '"choice":'), "ask_choice (default id)")

  answer = agent:ask_score_table("Third time I am writing!!!", "How angry is the customer?", { "calm", "annoyed", "furious" }, "anger")
  local anger = answer and answer.results[1].answers.anger
  print("score: " .. tostring(anger and anger.score))
  check(anger and type(anger.score) == "number" and type(anger.legend) == "table", "ask_score_table")
  check(contains(agent:ask_score("Hm.", "How angry?", { "calm", "furious" }), '"score":'), "ask_score")

  -- a table as the state is sent as a JSON object; quoting and Unicode survive
  answer = agent:ask_yes_no_table({ ticket = 42, text = "Caf\195\169 \226\128\148 \"money back\" now!\n" }, "Does the customer ask for a refund?")
  check(answer and type(answer.results[1].answers.q.noul) == "number", "table state, default id")

  -- predict: a request as a table (a batch of two) and as JSON text
  local batch = {
    { state = "I want my money back", questions = { r = { type = "noul", instructions = "Refund?" } } },
    { state = "Great service, thanks", questions = { r = { type = "noul", instructions = "Refund?" },
                                                      mood = { type = "choice", instructions = "Mood?", criteria = { "happy", "angry" } } } },
  }
  answer = agent:predict_table(batch)
  check(answer and #answer.results == 2 and answer.results[2].answers.mood.choice ~= nil, "predict_table with a table request")
  json = agent:predict('{"state":"x","questions":{"a":{"type":"noul","instructions":"y"}}}')
  check(contains(json, '"results":') and contains(json, '"elapsed_ms":'), "predict with JSON text")
  check(contains(agent:predict(llaya.encode(batch)), '"results":'), "predict with llaya.encode")

  -- rejected requests come back as nil + message, and the agent stays usable
  value, message = agent:predict('{"state":"x","questions":{"a":{"type":"essay","instructions":"x"}}}')
  check(value == nil and contains(message, "Unsupported question type"), "bad question type: " .. tostring(message))
  value, message = agent:predict("{broken")
  check(value == nil and type(message) == "string", "malformed JSON")
  value, message = agent:predict_table({ state = "x" })
  check(value == nil and type(message) == "string", "request without questions")
  check(not pcall(agent.predict, agent), "predict() without a request raises an argument error")
  check(contains(agent:ask_yes_no("Still working?", "Is this a question?"), '"noul":'), "agent usable after errors")

  check(contains(agent:prepare('{"state":"x","questions":{"a":{"type":"noul","instructions":"y"}}}'), '"ids":'), "prepare")
  check(type(agent:prepare_table({ state = "x", questions = { a = { type = "noul", instructions = "y" } } })) == "table", "prepare_table")

  -- close: explicit, repeatable, and methods then report it
  agent:close()
  agent:close()
  value, message = agent:info()
  check(value == nil and contains(message, "closed"), "methods after close")
  check(contains(tostring(agent), "closed"), "tostring after close")

  -- an agent that is simply dropped is unloaded by the garbage collector
  agent = assert(llaya.new(model, { backend = backend }))
  check(agent:info() ~= nil, "reload")
  agent = nil
  collectgarbage()
  collectgarbage()
else
  print("(model tests skipped: pass MODEL_DIR)")
end

print(checks .. " checks, " .. failures .. " failures")
os.exit(failures == 0 and 0 or 1)
