require("lib/quakenet")
require("lib/missing")
require("lib/schedule")
require("lib/serialise")
require("lib/country")
require("lib/db")
require("lib/socket")
require("lib/json")
require("lib/class")

local LASTERROR = 0
local NOTREPORTING = false

function scripterror(err)
  local cerror = os.time()
  if cerror > LASTERROR then
    LASTERROR = cerror
    NOTREPORTING = false
  end

  if not NOTREPORTING then
    chanmsgn(traceback(err, 2))

    LASTERROR = LASTERROR + 1
    if LASTERROR > cerror + 10 then
      NOTREPORTING = true
      chanmsg("Too many errors for this script -- no longer reporting")
    end
  end
end

math.randomseed(os.time())
