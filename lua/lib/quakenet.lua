require("lib/quakenet/access")
require("lib/quakenet/iterators")
require("lib/quakenet/legacy")
require("lib/quakenet/achievements")

function chanmsg(a)
  irc_smsg(a, "#qnet.keepout")
end

function crapchanmsg(a)
  irc_smsg(a, "#qnet.trj")
end

function statusmsg(a)
  irc_smsg("dd," .. a, "#qnet.keepout")
end

function chanmsgn(t)
  string.gsub(t, "[^\r\n]+", chanmsg)
end

function noticen(n, t)
  string.gsub(t, "[^\r\n]+", function(s) irc_notice(n, s) end);
end

local irc_localrealovmode = irc_localovmode;

function irc_localovmode(source, chan, ...)
  if type(...) == 'table' then
    irc_localrealovmode(source, chan, ...)
  else
    irc_localrealovmode(source, chan, { ... })
  end
end

function irc_localaction(n, c, m)
  irc_localchanmsg(n, c, string.char(1) .. "ACTION " .. m .. string.char(1))
end

function irctolower(l)
  l = l:lower()
  l = l:gsub("%[", "{")
  l = l:gsub("%]", "}")
  l = l:gsub("\\", "|")
  l = l:gsub("%^", "~")
  return l
end

function irc_localregisteruser(nickname, ident, hostname, realname, account, usermodes, handler_function)
  return irc_localregisteruserid(nickname, ident, hostname, realname, account, 0, usermodes, handler_function)
end
