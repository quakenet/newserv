function db_queryiter()
  local f = db_numfields() - 1
  local gb = db_getvalue

  return function()
    if not db_nextrow() then
      return nil
    end

    local t = {}
    for j=0,f do
      local v = gb(j)
      table.insert(t, v)
    end

    return unpack(t)
  end
end

local realquery = db_query

function db_query(x, fn, t)
  if not fn then
    realquery(x)
  elseif not t then
    realquery(x, fn, nil)
  else
    realquery(x, fn, t)
  end
end
