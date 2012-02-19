require "lib/class"

Achievements = class(function(obj, bot_id, types)
  obj.bot_id = bot_id
  obj.types = types
end)

function Achievements:send(nick, achievement, ...)
  local extra = ...
  if not extra then
    extra = 0
  end

  local ntype = self.types[achievement]
  if ntype then
    achievement = ntype
  end

  local user_numeric
  if type(nick) == "table" then
    user_numeric = irc_numerictobase64(nick.numeric)
  else -- assume it's a numeric
    user_numeric = irc_numerictobase64(nick)
  end
  
  if not user_numeric then
    return
  end

  irc_privmsg("C", "EXTACH " .. user_numeric .. " " .. self.bot_id .. " " .. achievement .. " " .. extra)
end
