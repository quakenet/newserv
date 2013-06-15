-- transparent non-blocking writes with buffers!

local socket_write_raw = socket_write
local socket_unix_connect_raw = socket_unix_connect
local socket_unix_bind_raw = socket_unix_bind
local socket_tcp_connect_raw = socket_tcp_connect
local socket_tcp_bind_raw = socket_tcp_bind
local socket_udp_connect_raw = socket_udp_connect
local socket_udp_bind_raw = socket_udp_bind
local socket_close_raw = socket_close

local sockets = {}

local function socket_handler(socket, event, tag, ...)
  if event == "flush" then
    local buf = sockets[socket].writebuf
    local ret = socket_write_raw(socket, buf)

    if ret == -1 then
      return -- close will be called
    end

    sockets[socket].writebuf = buf:sub(ret + 1)

    if sockets[socket].closing and sockets[socket].writebuf == "" then
      socket_close(socket)
    end

    -- no reason to tell the caller
    return
  elseif event == "accept" then
    local newsocket = ...
    socket_new(newsocket, sockets[socket].handler)
  end

  sockets[socket].handler(socket, event, tag, ...)

  if event == "close" then
    sockets[socket] = nil
  end
end

function socket_new(socket, handler)
  sockets[socket] = { writebuf = "", handler = handler }
end

function socket_unix_connect(path, handler, tag)
  local connected, socket = socket_unix_connect_raw(path, socket_handler, tag)
  if connected == nil then
    return nil
  end

  socket_new(socket, handler)
  if connected then
    socket_handler(socket, "connect", tag)
  end

  return socket
end

function socket_unix_bind(path, handler, tag)
  local socket = socket_unix_bind_raw(path, socket_handler, tag)
  if not socket then
    return nil
  end

  socket_new(socket, handler)
  return socket
end

local function socket_ip_connect(fn, address, port, handler, tag)
  local connected, socket = fn(address, port, socket_handler, tag)

  if connected == nil then
    return nil
  end

  socket_new(socket, handler)
  if connected then
    socket_handler(socket, "connect", tag)
  end

  return socket
end

local function socket_ip_bind(fn, address, port, handler, tag)
  local socket = fn(address, port, socket_handler, tag)
  if not socket then
    return nil
  end

  socket_new(socket, handler)
  return socket
end

function socket_tcp_bind(address, port, handler, tag)
  return socket_ip_bind(socket_tcp_bind_raw, address, port, handler, tag)
end

function socket_tcp_connect(address, port, handler, tag)
  return socket_ip_connect(socket_tcp_connect_raw, address, port, handler, tag)
end

function socket_udp_bind(address, port, handler, tag)
  return socket_ip_bind(socket_udp_bind_raw, address, port, handler, tag)
end

function socket_udp_connect(address, port, handler, tag)
  return socket_ip_connect(socket_udp_connect_raw, address, port, handler, tag)
end

function socket_write(socket, data)
  if sockets[socket].writebuf == "" then
    local ret = socket_write_raw(socket, data)
    if ret == -1 then
      return false -- close will be called regardless
    end

    if ret == data:len() then
      return true
    end

    -- lua strings start at 1
    sockets[socket].writebuf = data:sub(ret + 1)
  else
    sockets[socket].writebuf = sockets[socket].writebuf .. data
  end

  return true
end

function socket_close(socket, flush)
  if whenbufferempty and sockets[socket].writebuf ~= "" then
    sockets[socket].closing = true
    return
  end

  return socket_close_raw(socket)
end
