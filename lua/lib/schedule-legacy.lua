local scheduler = Scheduler()

function doschedule()
  -- do nothing
end

function schedule(when, callback, ...)
  return scheduler:add(when, callback, ...)
end

function delschedule(tag)
  scheduler:remove(tag)
end

function scheduleempty()
  return #scheduler == 0
end
