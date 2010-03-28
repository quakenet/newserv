function db_queryiter()
  local c = db_numrows()
  local i = -1
  local f = db_numfields()
  local gb = db_getvalue

  return function()
    i = i + 1
    if i == c then
      return nil
    end

    local t = {}
    for j=0,f do
      table.insert(t, gb(i, j))
    end

    return t
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
