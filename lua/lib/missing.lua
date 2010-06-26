function tablelen(tab)
  local count = 0

  for _, _ in pairs(tab) do
    count = count + 1
  end

  return count
end

function tablesequal(a, b)
  local k, v

  for k, v in pairs(a) do
    if not b[k] or b[k] ~= v then
      return false
    end
  end

  for k, v in pairs(b) do
    if not a[k] or a[k] ~= v then
      return false
    end
  end

  return true
end

function trim(s)
  return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

function explode(d,p,m)
  t={}
  ll=0
  f=0
  if m and m < 1 then
    return { p }
  end

  while true do
    l=string.find(p,d,ll+1,true) -- find the next d in the string
    if l~=nil then -- if "not not" found then..
      table.insert(t, string.sub(p,ll,l-1)) -- Save it in our array.
      ll=l+1 -- save just after where we found it for searching next time.
      f = f + 1
      if m and f >= m then
        table.insert(t, string.sub(p,ll))
        break
      end
    else
      table.insert(t, string.sub(p,ll)) -- Save what's left in our array.
      break -- Break at end, as it should be, according to the lua manual.
    end
  end
  return t
end

function tableempty(table)
  for k, v in pairs(table) do
    return false
  end

  return true
end

function urlencode(str)
  if (str) then
    str = string.gsub (str, "\n", "\r\n")
    str = string.gsub (str, "([^%w ])",
        function (c) return string.format ("%%%02X", string.byte(c)) end)
    str = string.gsub (str, " ", "+")
  end
  return str	
end

function matchtoregex(match)
  local x = match:gsub("([%^%$%(%)%%%.%[%]%+%-])", "%%%1")
  x = x:gsub("%*", ".*")
  x = x:gsub("%?", ".")
  x = "^" .. x .. "$"
  return x
end

