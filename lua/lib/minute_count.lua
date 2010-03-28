MinuteCount = class(function(obj, minutes)
  obj.minutes = minutes
  obj.data = {}
  obj.current = 0
end)

function MinuteCount:add()
  self.current = self.current + 1
end

function MinuteCount:moveon()
  table.insert(self.data, 0, self.current)
  table.remove(self.data, self.minutes)
  self.current = 0
end

function MinuteCount:sum()
  local sum = self.current
  for _, v in ipairs(self.data) do
    sum = sum + v
  end

  return sum
end
