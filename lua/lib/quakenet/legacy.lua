local function flatten(x)
  local resulta = {}
  local resultb = {}

  for k, v in pairs(x) do
    table.insert(resulta, k)
    table.insert(resultb, v)
  end
  return resulta, resultb
end

local inickpusher, nnickpusher = flatten(nickpusher)
local ichanpusher, nchanpusher = flatten(chanpusher)

local function __nickpush(fn, input)
  local n = { fn(input, nnickpusher) }
  if #n == 0 then
    return
  end

  local result = {}
  for k, v in pairs(n) do
    result[inickpusher[k]] = v
  end

  -- compatibility
  result.account = result.authname
  return result
end

function irc_getnickbynick(nick)
  return __nickpush(irc_fastgetnickbynick, nick)
end

function irc_getnickbynumeric(numeric)
  return __nickpush(irc_fastgetnickbynumeric, numeric)
end

function irc_getchaninfo(channel)
  local c = { irc_fastgetchaninfo(channel, nchanpusher) }
  if #c == 0 then
    return
  end

  local result = {}
  for k, v in pairs(c) do
    result[ichanpusher[k]] = v
  end

  return result
end

function irc_localregisteruser(nickname, ident, hostname, realname, account, usermodes, handler_function)
  return irc_localregisteruserid(nickname, ident, hostname, realname, account, 0, usermodes, handler_function)
end
