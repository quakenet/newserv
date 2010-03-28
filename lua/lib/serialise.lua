function savetable(filename, t)
  local f = assert(io.open(filename, "w"))

  f:write("return\n")

  serialise(f, t)

  f:close()
end

function loadtable(filename)
  local func, err = loadfile(filename)

  if not func then
    return nil
  end

  return func()
end

function serialise(f, o)
  if type(o) == "number" then
    f:write(o)
  elseif type(o) == "string" then
    f:write(string.format("%q", o))
  elseif type(o) == "boolean" then
    if o == true then
      f:write("true")
    else
      f:write("false")
    end
  elseif type(o) == "table" then
    f:write("{\n")
    for k,v in pairs(o) do
      f:write("  [")
      serialise(f, k)
      f:write("] = ")
      serialise(f, v)
      f:write(",\n")
    end
    f:write("}\n")
  elseif type(o) == "nil" then
    f:write("nil")
  else
    error("cannot serialise a " .. type(o))
  end
end

function loadtextfile(file, fn)
  local f = io.open(file, "r")
  if not f then
    return false
  end
  
  while true do
    local line = f:read()
    if line == nil then
      return true
    end

    fn(line)
  end
end
