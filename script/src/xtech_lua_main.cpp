/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2026 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <luabind/luabind.hpp>

#include "script/luna/lunarender.h"
#include <luabind/function.hpp>
#include <luabind/object.hpp>

#include <Logger/logger.h>
#include <AppPath/app_path.h>
#include <Utils/files.h>

#include "globals.h"
#include "global_dirs.h"
#include "config.h"

#include "xtech_lua_main.h"
#include "xtech_lua_bindings.h"
#include "xtech_lua_data.h"


// ============================================================================
// Global Lua state
// ============================================================================

static lua_State *g_L = nullptr;
static bool g_luaWorking = false;
static bool g_luaInitialized = false;
static bool g_gameLoaded = false;
static int g_sandboxRef = LUA_NOREF;
static bool s_wasOnMap = false;

// Lua function references (level)
static luabind::object g_luaFunc_onLoad;
static luabind::object g_luaFunc_onLoop;
static luabind::object g_luaFunc_onLoopEnd;
static luabind::object g_luaFunc_onRenderStart;
static luabind::object g_luaFunc_onRenderEnd;
static luabind::object g_luaFunc_onRender;
static luabind::object g_luaFunc_onRenderHud;

// ============================================================================
// Sandbox: __newindex proxy — whitelist CustomData to _G
// ============================================================================

static int sandbox_newindex(lua_State* L)
{
    // Stack: 1=sandbox, 2=key, 3=value
    const char* key = lua_tostring(L, 2);

    // Whitelist: "CustomData" writes always go to the global table _G
    if(key && strcmp(key, "CustomData") == 0)
    {
        lua_pushvalue(L, 3);
        lua_setglobal(L, key);
        return 0;
    }

    // All other writes stay in the sandbox: sandbox[key] = value
    // Use rawset to avoid re-triggering __newindex (would cause stack overflow!)
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, 1);
    return 0;
}


// ============================================================================
// Helper: safely call a Lua function
// ============================================================================

static void safeCallLuaFunc(luabind::object &func, const char *funcName)
{
    if(!g_luaWorking || !func.is_valid())
        return;

    try
    {
        func();
    }
    catch(const luabind::error &e)
    {
        pLogWarning("Lua error in %s: %s", funcName,
            lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
    catch(const std::exception &e)
    {
        pLogWarning("Lua exception in %s: %s", funcName, e.what());
    }
    catch(...)
    {
        pLogWarning("Lua unknown exception in %s", funcName);
    }
}

template<typename... Args>
static void safeCallLuaFuncWith(luabind::object &func, const char *funcName, Args... args)
{
    if(!g_luaWorking || !func.is_valid())
        return;

    try
    {
        func(args...);
    }
    catch(const luabind::error &e)
    {
        pLogWarning("Lua error in %s: %s", funcName,
            lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
    catch(const std::exception &e)
    {
        pLogWarning("Lua exception in %s: %s", funcName, e.what());
    }
    catch(...)
    {
        pLogWarning("Lua unknown exception in %s", funcName);
    }
}


// ============================================================================
// Helper: load and execute a .lua file
// ============================================================================

static bool loadAndRunLuaFile(const std::string &filePath, const char *label)
{
    if(filePath.empty())
        return false;

    // Read file
    Files::Data data = Files::load_file(filePath);
    if(data.empty())
    {
        pLogDebug("Lua: no file to load: %s", filePath.c_str());
        return false;
    }

    // Load and run the chunk
    int result = luaL_loadbuffer(g_L, data.c_str(), data.size(), filePath.c_str());
    if(result != 0)
    {
        pLogWarning("Lua: Error loading %s: %s", label,
            lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return false;
    }

    result = lua_pcall(g_L, 0, 0, 0);
    if(result != 0)
    {
        pLogWarning("Lua: Error running %s: %s", label,
            lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return false;
    }

    pLogInfo("Lua: Loaded and ran script: %s", label);
    return true;
}


// ============================================================================
// Helper: retrieve a global Lua function by name
// ============================================================================

static luabind::object getLuaFunc(const char *funcName)
{
    try
    {
        lua_getglobal(g_L, funcName);
        if(lua_isfunction(g_L, -1))
        {
            luabind::object obj(luabind::from_stack(g_L, -1));
            lua_pop(g_L, 1);
            return obj;
        }
        lua_pop(g_L, 1);
    }
    catch(const luabind::error &) {}
    catch(const std::exception &) {}

    return luabind::object();
}


// ============================================================================
// Public API implementation
// ============================================================================

lua_State* xtech_lua_getState()
{
    return g_L;
}


// Look up a Lua function by name — sandbox first, then _G
bool xtech_lua_getFunc(const char* funcName)
{
    if(!g_L) return false;

    // Try level sandbox first
    if(g_sandboxRef != LUA_NOREF)
    {
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_sandboxRef);
        lua_getfield(g_L, -1, funcName);
        lua_remove(g_L, -2); // remove sandbox, keep function/nil
        if(lua_isfunction(g_L, -1))
            return true;
        lua_pop(g_L, 1); // pop nil
    }

    // Fall back to _G
    lua_getglobal(g_L, funcName);
    return lua_isfunction(g_L, -1);
}

bool xtech_lua_init()
{
    if(g_luaInitialized)
        return true;

    // Check if Lua scripting is enabled in config
    if(!g_config.lua_enable_engine)
    {
        pLogDebug("Lua: Lua engine disabled in config");
        return true; // Not a fatal error
    }

    pLogInfo("Lua: Initializing LuaJIT scripting engine...");

    // Create Lua state
    g_L = luaL_newstate();
    if(!g_L)
    {
        pLogWarning("Lua: Failed to create Lua state");
        return false;
    }

    // Open standard libraries
    luaL_openlibs(g_L);

    // Initialize luabind
    luabind::open(g_L);

    // Register game bindings
    xtech_lua_register_bindings(g_L);

    // Global script (lunaglobal.lua) is deferred to xtech_lua_load()
    // because AppPathManager::assetsRoot() is not ready during init.

    g_luaInitialized = true;
    g_luaWorking = true; // VM is ready even without level script

    pLogInfo("Lua: Initialization complete (VM ready)");
    return true;
}


// ============================================================================
// Game-level: load game.lua once per session
// ============================================================================

void xtech_lua_loadGame()
{
    if(!g_luaInitialized || !g_L || !g_config.lua_enable_engine)
        return;
    if(g_gameLoaded)
        return;

    std::string gamePath = g_dirEpisode.resolveFileCaseExistsAbs("game.lua");
    if(!gamePath.empty())
    {
        pLogInfo("Lua: Loading game.lua from: %s", gamePath.c_str());
        if(loadAndRunLuaFile(gamePath, "game.lua"))
            g_gameLoaded = true;  // only mark loaded if successful
    }
    else
        pLogInfo("Lua: No game.lua found in episode directory, will retry later");
}


// ============================================================================
// Retrieve a function from sandbox by name (sandbox at given registry ref)
// ============================================================================

static luabind::object getSandboxFunc(int sandboxRef, const char* funcName)
{
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, sandboxRef); // [sandbox]
    lua_getfield(g_L, -1, funcName);                  // [sandbox][func/nil]
    if(lua_isfunction(g_L, -1))
    {
        luabind::object obj(luabind::from_stack(g_L, -1));
        lua_pop(g_L, 2);
        return obj;
    }
    lua_pop(g_L, 2);
    return luabind::object();
}


// ============================================================================
// Revised xtech_lua_loadLevel
// ============================================================================

void xtech_lua_loadLevel()
{
    if(!g_luaInitialized || !g_L || !g_config.lua_enable_engine)
        return;

    // Leaving the map: fire OnLeaveMap + reset tracking
    xtech_lua_mapLeave();
    s_wasOnMap = false;

    // Ensure game.lua loaded first
    xtech_lua_loadGame();

    // Clear previous function references
    g_luaFunc_onLoad = luabind::object();
    g_luaFunc_onLoop = luabind::object();
    g_luaFunc_onLoopEnd = luabind::object();
    g_luaFunc_onRenderStart = luabind::object();
    g_luaFunc_onRenderEnd = luabind::object();
    g_luaFunc_onRender = luabind::object();
    g_luaFunc_onRenderHud = luabind::object();
    g_luaWorking = false;

    // Release old sandbox
    if(g_sandboxRef != LUA_NOREF)
    {
        luaL_unref(g_L, LUA_REGISTRYINDEX, g_sandboxRef);
        g_sandboxRef = LUA_NOREF;
    }

    // Find level script
    std::string levelPath = g_dirCustom.resolveFileCaseExistsAbs("level.lua");
    if(levelPath.empty())
        levelPath = g_dirCustom.resolveFileCaseExistsAbs("lunadll.lua");
    if(levelPath.empty())
        return;

    // Read and load the chunk
    Files::Data data = Files::load_file(levelPath);
    if(data.empty())
        return;

    int result = luaL_loadbuffer(g_L, data.c_str(), data.size(), levelPath.c_str());
    if(result != 0)
    {
        pLogWarning("Lua: Error loading level script: %s", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return;
    }

    // Create sandbox table
    lua_newtable(g_L);                           // [chunk][sandbox]

    // Create metatable for sandbox
    lua_newtable(g_L);                           // [chunk][sandbox][mt]
    lua_pushvalue(g_L, LUA_GLOBALSINDEX);        // [chunk][sandbox][mt][_G]
    lua_setfield(g_L, -2, "__index");            // mt.__index = _G
    lua_pushcfunction(g_L, sandbox_newindex);    // [chunk][sandbox][mt][func]
    lua_setfield(g_L, -2, "__newindex");         // mt.__newindex = sandbox_newindex
    lua_setmetatable(g_L, -2);                   // setmetatable(sandbox, mt)
    // [chunk][sandbox]

    // Save sandbox ref before setfenv
    g_sandboxRef = luaL_ref(g_L, LUA_REGISTRYINDEX); // [chunk], sandbox stored

    // Push sandbox back, set as chunk env, execute
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_sandboxRef); // [chunk][sandbox]
    lua_setfenv(g_L, -2);                                // [chunk]
    result = lua_pcall(g_L, 0, 0, 0);                   // []
    if(result != 0)
    {
        pLogWarning("Lua: Error running level script: %s", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        luaL_unref(g_L, LUA_REGISTRYINDEX, g_sandboxRef);
        g_sandboxRef = LUA_NOREF;
        return;
    }

    pLogInfo("Lua: Loaded and ran level script");
    g_luaWorking = true;

    // Retrieve hook functions from sandbox
    g_luaFunc_onLoad       = getSandboxFunc(g_sandboxRef, "onLoad");
    g_luaFunc_onLoop       = getSandboxFunc(g_sandboxRef, "onLoop");
    g_luaFunc_onLoopEnd    = getSandboxFunc(g_sandboxRef, "onLoopEnd");
    g_luaFunc_onRenderStart = getSandboxFunc(g_sandboxRef, "onRenderStart");
    g_luaFunc_onRenderEnd  = getSandboxFunc(g_sandboxRef, "onRenderEnd");
    g_luaFunc_onRender     = getSandboxFunc(g_sandboxRef, "onRender");
    g_luaFunc_onRenderHud  = getSandboxFunc(g_sandboxRef, "onRenderHud");

    // Call onLoad immediately
    safeCallLuaFunc(g_luaFunc_onLoad, "onLoad");
}


// ============================================================================
// Unload level script (no VM destruction)
// ============================================================================

void xtech_lua_unloadLevel()
{
    if(!g_luaInitialized || !g_L)
        return;

    // Fire onLevelExit before tearing down (death, completion, quit all go through here)
    xtech_lua_getFunc("onLevelExit");
    if(lua_isfunction(g_L, -1))
    {
        if(lua_pcall(g_L, 0, 0, 0) != 0)
        {
            pLogWarning("Lua: Error in onLevelExit: %s", lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
    }
    else
        lua_pop(g_L, 1);

    // Clear async delayed calls
    xtech_lua_clear_delayed_calls();

    // Clear function references
    g_luaFunc_onLoad = luabind::object();
    g_luaFunc_onLoop = luabind::object();
    g_luaFunc_onLoopEnd = luabind::object();
    g_luaFunc_onRenderStart = luabind::object();
    g_luaFunc_onRenderEnd = luabind::object();
    g_luaFunc_onRender = luabind::object();
    g_luaFunc_onRenderHud = luabind::object();
    g_luaWorking = false;

    // Release sandbox
    if(g_sandboxRef != LUA_NOREF)
    {
        luaL_unref(g_L, LUA_REGISTRYINDEX, g_sandboxRef);
        g_sandboxRef = LUA_NOREF;
    }
}


// ============================================================================
// Backward-compatible xtech_lua_load (game + level)
// ============================================================================

void xtech_lua_load()
{
    xtech_lua_loadGame();
    xtech_lua_loadLevel();
}


// ============================================================================
// Revised xtech_lua_reset (no VM destruction)
// ============================================================================

void xtech_lua_reset()
{
    xtech_lua_unloadLevel();
}


// ============================================================================
// Game-level hooks: OnGameSave / OnGameLoad
// ============================================================================

void xtech_lua_callGameSave()
{
    if(!g_luaInitialized || !g_L)
        return;

    lua_getglobal(g_L, "OnGameSave");
    if(!lua_isfunction(g_L, -1))
    {
        lua_pop(g_L, 1);
        return;
    }

    // Call OnGameSave(), expects return table
    if(lua_pcall(g_L, 0, 1, 0) != 0)
    {
        pLogWarning("Lua: Error in OnGameSave: %s", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
        return;
    }

    // Table is left on stack for caller to serialize
    // Caller must pop when done
}


void xtech_lua_callGameLoad()
{
    if(!g_luaInitialized || !g_L)
        return;

    // Expect caller pushed a data table: stack = [data_table]
    int topBefore = lua_gettop(g_L);
    if(topBefore < 1)
    {
        pLogWarning("Lua: OnGameLoad skipped — no data table on stack");
        return;
    }

    // Move the table to a known position: use registry ref
    int tableRef = luaL_ref(g_L, LUA_REGISTRYINDEX); // pop table, store ref
    // stack is now clean

    lua_getglobal(g_L, "OnGameLoad");
    int type = lua_type(g_L, -1);
    pLogInfo("Lua: OnGameLoad lookup: type=%d(%s), top=%d",
        type, lua_typename(g_L, type), lua_gettop(g_L));

    if(type != LUA_TFUNCTION)
    {
        pLogWarning("Lua: OnGameLoad not found (type=%s)", lua_typename(g_L, type));
        lua_pop(g_L, 1);
        luaL_unref(g_L, LUA_REGISTRYINDEX, tableRef);
        return;
    }

    // Push table as argument
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, tableRef); // [func][table]
    luaL_unref(g_L, LUA_REGISTRYINDEX, tableRef);  // release registry ref

    // Call: func(table)
    if(lua_pcall(g_L, 1, 0, 0) != 0)
    {
        pLogWarning("Lua: Error in OnGameLoad: %s", lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }

    pLogInfo("Lua: OnGameLoad done, stack=%d", lua_gettop(g_L));
}


// Complete save cycle: call OnGameSave, serialize to JSON, write file
bool xtech_lua_gameSave(const std::string& dataPath)
{
    xtech_lua_loadGame();  // ensure game.lua loaded
    xtech_lua_callGameSave();

    if(!g_L || !lua_istable(g_L, -1))
    {
        if(g_L && lua_gettop(g_L) > 0) lua_pop(g_L, 1);
        return false;
    }

    std::string json = lua_table_to_json(g_L, lua_gettop(g_L));
    lua_pop(g_L, 1);

    if(json.empty() || json == "null")
        return false;

    // Write JSON to file
    FILE* f = Files::utf8_fopen(dataPath.c_str(), "w");
    if(!f) return false;
    fwrite(json.c_str(), 1, json.size(), f);
    fclose(f);
    return true;
}


// Complete load cycle: parse JSON, call OnGameLoad
bool xtech_lua_gameLoad(const std::string& jsonStr)
{
    if(!g_L) return false;
    xtech_lua_loadGame();  // ensure game.lua loaded

    if(jsonStr.empty())
    {
        lua_newtable(g_L);
    }
    else
    {
        json_to_lua_table(g_L, jsonStr);
    }

    xtech_lua_callGameLoad();
    return true;
}


// ============================================================================
// World map render hook
// ============================================================================

// ============================================================================
// Map enter/leave hooks
// ============================================================================

void xtech_lua_mapEnter()
{
    if(!g_luaInitialized || !g_L) return;
    xtech_lua_loadGame();

    lua_getglobal(g_L, "OnEnterMap");
    if(lua_isfunction(g_L, -1))
    {
        if(lua_pcall(g_L, 0, 0, 0) != 0)
        {
            pLogWarning("Lua: Error in OnEnterMap: %s", lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
    }
    else
        lua_pop(g_L, 1);
}

void xtech_lua_mapLeave()
{
    if(!g_luaInitialized || !g_L) return;
    if(!s_wasOnMap) return;  // only fire if we were actually on the map

    lua_getglobal(g_L, "OnLeaveMap");
    if(lua_isfunction(g_L, -1))
    {
        if(lua_pcall(g_L, 0, 0, 0) != 0)
        {
            pLogWarning("Lua: Error in OnLeaveMap: %s", lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
    }
    else
        lua_pop(g_L, 1);
}

void xtech_lua_worldMapRender()
{
    if(!g_luaInitialized || !g_L)
        return;

    xtech_lua_loadGame();  // ensure game.lua loaded

    // Fire OnEnterMap on first render frame after entering the map
    if(!s_wasOnMap)
    {
        s_wasOnMap = true;
        xtech_lua_mapEnter();
    }

    lua_getglobal(g_L, "OnWorldMapRender");
    if(lua_isfunction(g_L, -1))
    {
        if(lua_pcall(g_L, 0, 0, 0) != 0)
        {
            pLogWarning("Lua: Error in OnWorldMapRender: %s", lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
    }
    else
        lua_pop(g_L, 1);
}


void xtech_lua_loop()
{
    if(!g_luaWorking)
        return;

    // Process async delayed calls
    xtech_lua_process_delayed_calls();

    safeCallLuaFunc(g_luaFunc_onLoop, "onLoop");
}


void xtech_lua_renderStart()
{
    // Start the render frame if LunaLua is not managing it
    if(!g_config.luna_enable_engine)
        Renderer::Get().StartFrameRender();
    safeCallLuaFunc(g_luaFunc_onRenderStart, "onRenderStart");
}


void xtech_lua_renderEnd()
{
    safeCallLuaFunc(g_luaFunc_onRenderEnd, "onRenderEnd");
    // End the render frame and clean up render ops if LunaLua is not managing it
    if(!g_config.luna_enable_engine)
    {
        Renderer::Get().EndFrameRender();
        Renderer::Get().ClearQueue();
    }
}


void xtech_lua_render(int screenZ)
{
    safeCallLuaFuncWith(g_luaFunc_onRender, "onRender", screenZ);
}


void xtech_lua_renderHud(int screenZ, int numScreens)
{
    safeCallLuaFuncWith(g_luaFunc_onRenderHud, "onRenderHud", screenZ, numScreens);
}


bool xtech_lua_quit()
{
    if(!g_luaInitialized)
        return true;

    // Call onLoopEnd one last time if available
    safeCallLuaFunc(g_luaFunc_onLoopEnd, "onLoopEnd");

    // Unload level state
    xtech_lua_unloadLevel();

    // Release sandbox
    if(g_sandboxRef != LUA_NOREF)
    {
        luaL_unref(g_L, LUA_REGISTRYINDEX, g_sandboxRef);
        g_sandboxRef = LUA_NOREF;
    }

    g_luaWorking = false;
    g_gameLoaded = false;

    // Destroy Lua state
    if(g_L)
    {
        lua_close(g_L);
        g_L = nullptr;
    }

    g_luaInitialized = false;
    pLogInfo("Lua: Shutdown complete");

    return true;
}
