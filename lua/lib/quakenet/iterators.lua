nickpusher = { nick = 0, ident = 1, hostname = 2, realname = 3, authname = 4, ipaddress = 5, numeric = 6, timestamp = 7, accountts = 8, umodes = 9, country = 10, accountid = 11, servername = 12, servernumeric = 13 }
chanpusher = { name = 0, totalusers = 1, topic = 2, realusers = 3, timestamp = 4, modes = 5 }

function channelusers_iter(channel, items)
  local t = { irc_getfirstchannick(channel, items) }
  if #t == 0 then
    return function()
      return nil
    end
  end
  local b = false

  return function()
    if not b then
      b = true
      return t
    end

    local t = { irc_getnextchannick() }
    if #t == 0 then
      return nil
    end
    return t
  end
end

function channelhash_iter(items)
  local t = { irc_getfirstchan(items) }
  if #t == 0 then
    return function()
      return nil
    end
  end
  local b = false

  return function()
    if not b then
      b = true
      return t
    end

    local t = { irc_getnextchan() }
    if #t == 0 then
      return nil
    end
    return t
  end
end

function nickhash_iter(items)
  local t = { irc_getfirstnick(items) }
  if #t == 0 then
    return function()
      return nil
    end
  end
  local b = false

  return function()
    if not b then
      b = true
      return t
    end

    local t = { irc_getnextnick() }
    if #t == 0 then
      return nil
    end
    return t
  end
end

function nickchannels_iter(nick)
  local c = irc_getnickchancount(nick)
  local i = -1

  return function()
    i = i + 1
    if i == c then
      return nil
    end

    return irc_getnickchanindex(nick, i)
  end
end


