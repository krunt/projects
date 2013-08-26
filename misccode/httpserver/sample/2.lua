#!/usr/bin/lua

r = 0
for k in io.lines() do
    r = r + #k
end

print(r)

