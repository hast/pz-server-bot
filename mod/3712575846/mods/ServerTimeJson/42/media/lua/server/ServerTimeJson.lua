local WRITE_INTERVAL_MS = 10000
local lastWriteMs = 0

local function buildJson()
    local gt = getGameTime()

    local year = gt:getYear()
    local month = gt:getMonth() + 1
    local day = gt:getDayPlusOne()

    local timeOfDay = gt:getTimeOfDay()
    local hour = math.floor(timeOfDay)
    local minute = math.floor((timeOfDay - hour) * 60)

    return string.format(
        '{\n' ..
        '  "year": %d,\n' ..
        '  "month": %d,\n' ..
        '  "day": %d,\n' ..
        '  "hour": %d,\n' ..
        '  "minute": %d,\n' ..
        '  "date": "%04d-%02d-%02d",\n' ..
        '  "time": "%02d:%02d:00"\n' ..
        '}',
        year, month, day, hour, minute,
        year, month, day, hour, minute
    )
end

local function writeJson()
    local writer = getModFileWriter("ServerTimeJson", "server_time.json", true, false)
    if not writer then
        writer = getFileWriter("server_time.json", true, false)
    end
    if not writer then
        print("[ServerTimeJson] Failed to open writer")
        return
    end

    writer:write(buildJson())
    writer:close()
end

local function maybeWrite()
    local nowMs = getTimestampMs()
    if lastWriteMs == 0 or (nowMs - lastWriteMs) >= WRITE_INTERVAL_MS then
        lastWriteMs = nowMs
        writeJson()
    end
end

Events.EveryOneMinute.Add(maybeWrite)
maybeWrite()