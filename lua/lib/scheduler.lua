Scheduler = class(function(obj)
  obj.sched = raw_scheduler_new()
end)

function Scheduler:add(when, callback, ...)
  return self:add_abs(os.time() + when, callback, ...)
end

function Scheduler:add_abs(w, callback, ...)
  local args = { ... }
  local pfn = function()
    callback(unpack(args))
  end

  return raw_scheduler_add(self.sched, w, function()
    local status, err = pcall(pfn)
    if not status then
      scripterror(err)
    end
  end)
end

function Scheduler:remove(tag)
  raw_scheduler_remove(tag)
end

function Scheduler:getn()
  return raw_scheduler_count(self.sched)
end
