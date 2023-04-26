#!/usr/bin/lua

local sqlite3 = require "lsqlite3"
local db = sqlite3.open('/log/mqtt_sub.db', sqlite3.OPEN_READWRITE)
if db == nil then
    print("mqtt messages db not found")
    return -1
end
local stmt = 0

if #arg > 1 then
    print("Usage: db_reader [topic name]")
    return -1
elseif #arg == 1 then
    stmt = db:prepare("SELECT * FROM messages WHERE topic=?")
    stmt:bind_values(arg[1])
else
    stmt = db:prepare(("SELECT * FROM messages"))
end

for row in stmt:nrows() do
    print("Timestamp:", row.timestamp)
    print("Topic:", row.topic)
    print("Payload:", row.payload)
    print("------------")
end

db:close()

return 0