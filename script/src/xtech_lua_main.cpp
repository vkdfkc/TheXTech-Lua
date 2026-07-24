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


// ============================================================================
// Global Lua state
// ============================================================================

static lua_State *g_L = nullptr;
static bool g_luaWorking = false;
static bool g_luaInitialized = false;

// Lua function references
static luabind::object g_luaFunc_onLoad;
static luabind::object g_luaFunc_onLoop;
static luabind::object g_luaFunc_onLoopEnd;
static luabind::object g_luaFunc_onRenderStart;
static luabind::object g_luaFunc_onRenderEnd;
static luabind::object g_luaFunc_onRender;
static luabind::object g_luaFunc_onRenderHud;


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


void xtech_lua_load()
{
    if(!g_luaInitialized || !g_L || !g_config.lua_enable_engine)
        return;

    // Clear previous function references
    g_luaFunc_onLoad = luabind::object();
    g_luaFunc_onLoop = luabind::object();
    g_luaFunc_onLoopEnd = luabind::object();
    g_luaFunc_onRenderStart = luabind::object();
    g_luaFunc_onRenderEnd = luabind::object();
    g_luaFunc_onRender = luabind::object();
    g_luaFunc_onRenderHud = luabind::object();
    g_luaWorking = false;

    // Load global script (once per session, deferred from init)
    static bool s_globalLoaded = false;
    if(!s_globalLoaded)
    {
        s_globalLoaded = true;
        std::string globalPath = AppPathManager::assetsRoot() + "lunaglobal.lua";
        if(Files::fileExists(globalPath))
            loadAndRunLuaFile(globalPath, "lunaglobal.lua");
    }

    // Load level-specific script
    bool levelLoaded = false;
    std::string levelPath = g_dirCustom.resolveFileCaseExistsAbs("level.lua");
    if(!levelPath.empty())
    {
        levelLoaded = loadAndRunLuaFile(levelPath, "level.lua");
    }

    // Also try lunadll.lua as fallback (similar to lunadll.txt naming)
    if(!levelLoaded)
    {
        std::string altPath = g_dirCustom.resolveFileCaseExistsAbs("lunadll.lua");
        if(!altPath.empty())
        {
            levelLoaded = loadAndRunLuaFile(altPath, "lunadll.lua");
        }
    }

    g_luaWorking = levelLoaded;

    if(!g_luaWorking)
        return;

    // Retrieve hook functions from the loaded script
    g_luaFunc_onLoad       = getLuaFunc("onLoad");
    g_luaFunc_onLoop       = getLuaFunc("onLoop");
    g_luaFunc_onLoopEnd    = getLuaFunc("onLoopEnd");
    g_luaFunc_onRenderStart = getLuaFunc("onRenderStart");
    g_luaFunc_onRenderEnd  = getLuaFunc("onRenderEnd");
    g_luaFunc_onRender     = getLuaFunc("onRender");
    g_luaFunc_onRenderHud  = getLuaFunc("onRenderHud");

    // Call onLoad immediately
    safeCallLuaFunc(g_luaFunc_onLoad, "onLoad");
}


void xtech_lua_loop()
{
    if(!g_luaWorking)
        return;

    // Process async delayed calls
    xtech_lua_process_delayed_calls();

    safeCallLuaFunc(g_luaFunc_onLoop, "onLoop");
}


void xtech_lua_reset()
{
    if(!g_luaInitialized || !g_L)
        return;

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

    // Clear Lua globals by recreating the state
    if(g_L)
    {
        lua_close(g_L);
        g_L = nullptr;
    }

    g_luaInitialized = false;

    // Re-initialize if enabled
    if(g_config.lua_enable_engine)
        xtech_lua_init();
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

    // Clear function references
    g_luaFunc_onLoad = luabind::object();
    g_luaFunc_onLoop = luabind::object();
    g_luaFunc_onLoopEnd = luabind::object();
    g_luaFunc_onRenderStart = luabind::object();
    g_luaFunc_onRenderEnd = luabind::object();
    g_luaFunc_onRender = luabind::object();
    g_luaFunc_onRenderHud = luabind::object();
    g_luaWorking = false;

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
