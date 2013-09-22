function onstaff(nick)
  return irc_nickonchan(nick, "#qnet.staff")
end

function ontlz(nick)
  if not irc_nickonchan(nick, "#twilightzone") then
    return false
  end

  local umodes = irc_getusermodes(nick)
  if not umodes or not string.find(umodes, "o") then
    return false
  end
  return true
end
