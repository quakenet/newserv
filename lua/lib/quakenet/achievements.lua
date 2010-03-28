require "lib/class"

Achievements = class(function(obj, bot_id, types)
  obj.bot_id = bot_id
  obj.types = types
end)

function Achievements:send(nick, type, ...)
  local extra = ...
  if not extra then
    extra = 0
  end

  local ntype = self.types[type]
  if ntype then
    type = ntype
  end

  local user_numeric = irc_numerictobase64(nick.numeric)
  irc_privmsg("C", "EXTACH " .. user_numeric .. " " .. self.bot_id .. " " .. type .. " " .. extra)
end
