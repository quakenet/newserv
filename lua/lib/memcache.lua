require("lib/scheduler")

Memcache = class(function(obj, ip, port, handler)
  obj.ip = ip
  obj.port = port
  obj.handler = handler
  obj._state = 0
  obj._sched = Scheduler()
  obj._connect(obj)
end)

function Memcache:put(key, value)
  if self._state ~= 2 then
    return false
  end

  self:_send("set", key, "0", "0", #value, "noreply")
  self:_send(value)
  return true
end

function Memcache:delete(key)
  if self._state ~= 2 then
    return false
  end

  self:_send("delete", key, "0", "noreply")
  return true
end

function Memcache:flush_all()
  if self._state ~= 2 then
    return false
  end

  self:_send("flush_all", "noreply")
  return true
end

function Memcache:_send(...)
  local data = { ... }

  data = table.concat(data, " ")
  self:_log("send " .. data)
  if self._state ~= 2 then
    self:_log("send in bad state")
    return
  end

  socket_write(self._socket, data .. "\r\n")
end

function Memcache:_event(socket, event, tag, ...)
  self:_log("socket event: " .. event)
  if event == "connect" then
    self._state = 2
    self.handler("connect")
  elseif event == "close" then
    self._state = 0
    self._socket = nil
    self.handler("disconnect")
    self:_schedule_connect()
  elseif event == "read" then
    local data = ...
    self:_log("read: " .. data:gsub("\r\n", "\\r\\n"))
  end
end

function Memcache:_connect()
  self:_log("connect(ip=" .. self.ip .. " port=" .. self.port .. ") state=" .. self._state)
  if self._state ~= 0 then
    return
  end

  self._socket = socket_tcp_connect(self.ip, self.port, function(...) self:_event(...) end, self)
  if not self._socket then
    self:_schedule_connect()
    return
  end
  self._state = 1
  self:_log("connect() state=" .. self._state .. " s ==" .. self._socket)
end

function Memcache:_schedule_connect()
  self._sched:add(5, function() self:_connect() end)
end

function Memcache:_log(data)
--  chanmsg(data)
end

