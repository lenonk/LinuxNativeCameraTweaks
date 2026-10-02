-- Hands MCM's values to the Linux Native Camera Tweaks bg3le plugin, which keeps its own copy for the next launch.
local PLUGIN = "LinuxNativeCameraTweaks"
local SETTINGS = {
    "roll_sensitivity", "invert_roll", "roll_min", "roll_max",
    "zoom_step", "invert_zoom", "zoom_limit", "zoom_min", "zoom_max",
    "controller_roll_speed", "controller_deadzone",
}

local warned = false

local function pluginRunning()
    if not (Ext.Plugins and Ext.Plugins.List) then
        return false, "bg3le isn't running; this mod needs bg3le's native plugins"
    end
    for _, p in ipairs(Ext.Plugins.List()) do
        if p.Name == PLUGIN then
            if p.Loaded then return true end
            return false, PLUGIN .. " failed to load: " .. tostring(p.Error)
        end
    end
    return false, PLUGIN .. " isn't installed in bg3le's plugins directory"
end

local function push(id)
    local value = MCM.Get(id)
    if value == nil then return end
    local ok, err = Ext.Plugins.Set(PLUGIN, id, value)
    if not ok then Ext.Utils.PrintWarning("[LNCTSettings] " .. tostring(err)) end
end

local function pushAll()
    if MCM == nil then return end -- no MCM: the plugin keeps its saved settings
    local running, why = pluginRunning()
    if not running then
        if not warned then
            Ext.Utils.PrintWarning("[LNCTSettings] " .. why)
            warned = true
        end
        return
    end
    for _, id in ipairs(SETTINGS) do push(id) end
end

-- Once every mod is loaded: MCM may load after this one, or not be installed at all.
local subscribed = false
local function on(events, name, handler)
    local event = events[name]
    if event then event:Subscribe(handler) end
end

local function subscribe()
    if subscribed or MCM == nil then return end
    local ok, events = pcall(function() return Ext.ModEvents.BG3MCM end)
    if not ok or type(events) ~= "table" and type(events) ~= "userdata" then return end
    subscribed = true
    local function ours(payload) return payload and payload.modUUID == ModuleUUID end
    on(events, "MCM_Setting_Saved", function(payload)
        if ours(payload) and pluginRunning() then push(payload.settingId) end
    end)
    on(events, "MCM_Setting_Reset", function(payload)
        if ours(payload) and pluginRunning() then push(payload.settingId) end
    end)
    on(events, "MCM_Profile_Activated", pushAll)
end

Ext.Events.SessionLoaded:Subscribe(function()
    subscribe()
    pushAll()
end)
