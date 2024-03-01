local geomath = {}

local Vector3 = {}
Vector3.__index = Vector3

function Vector3:new(p_x, p_y, p_z)
    local this = { p_x, p_y, p_z }
    setmetatable(this, Vector3)

    return this
end

function Vector3.mul(self, p_scalar)
    local x = p_scalar * self[1]
    local y = p_scalar * self[2]
    local z = p_scalar * self[3]
    return Vector3:new(x, y, z)
end

Vector3.__mul = Vector3.mul;

Vector3.ZERO = Vector3:new(0, 0, 0)
Vector3.HALF = Vector3:new(0.5, 0.5, 0.5)
Vector3.ONE = Vector3:new(1, 1, 1)
Vector3.UNIT_X = Vector3:new(1, 0, 0)
Vector3.UNIT_Y = Vector3:new(0, 1, 0)
Vector3.UNIT_Z = Vector3:new(0, 0, 1)

function geomath.clamp(a, min, max)
    a = math.max(a, min)
    a = math.min(a, max)
    return a
end

geomath.Vector3 = Vector3

return geomath