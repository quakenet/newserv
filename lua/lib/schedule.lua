local sched = {}
local tag = 0

local function timesort(a, b)
  return a[1] < b[1]
end

function doschedule()
  local ct = os.time()
  local out = {}
  local callers = {}

  for t, e in pairs(sched) do
    if t <= ct then
      table.insert(callers, { t, e })
    else
      out[t] = e
    end
  end
  table.sort(callers, timesort)

  sched = out

  for _, e in ipairs(callers) do
    for _, v in pairs(e[2]) do
      if v[1] then
        if v[2] then
          v[1](unpack(v[2]))
        else
          v[1]()
        end
      else
        scripterror("schedule.lua: event is nil!")
      end
    end
  end
end

function schedule(when, callback, ...)
  tag = tag + 1
  local n = { callback, { ... }, tag }
  local w = os.time() + when

  if sched[w] then
    table.insert(sched[w], n)
  else
    sched[w] = { n }
  end
  return { w, tag }
end

function delschedule(tag)
  local c = {}

  local w, o = unpack(tag)
  if not sched[w] then
    return
  end

  for i, v in pairs(sched[w]) do
    if v[3] == o then
      if #sched[w] == 1 then
        sched[w] = nil
      else
        sched[w][i] = nil
      end
      return
    end
  end
end

function scheduleempty()
  return tableempty(sched)
end
