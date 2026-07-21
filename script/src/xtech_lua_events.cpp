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

#include <cstring>
#include <set>
#include <string>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <luabind/luabind.hpp>
#include <luabind/function.hpp>
#include <luabind/object.hpp>

#include "xtech_lua_main.h"

#include <Logger/logger.h>

#include "xtech_lua_events.h"


// ============================================================================
// Subscription system for high-frequency events
// ============================================================================

// Set of NPC IDs that are subscribed to onNPCUpdate
static std::set<int> s_npcUpdateSubscribed;

// Generic subscription: event name -> set of IDs
static std::set<std::string> s_subscribedEvents;

// ============================================================================
// Helper: safely call a global Lua function with arguments
// ============================================================================

// Forward-declare helper for safe Lua calls
static bool s_callLuaFunc(lua_State *L, const char *funcName)
{
    if(!L) return false;
    lua_getglobal(L, funcName);
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

template<typename... Args>
static void s_safeCall(const char *funcName, Args... args)
{
    lua_State *L = xtech_lua_getState();
    if(!L) return;

    // Skip if Lua function not defined
    lua_getglobal(L, funcName);
    if(!lua_isfunction(L, -1))
    {
        lua_pop(L, 1);
        return;
    }

    // Push args
    int nargs = 0;
    (void)std::initializer_list<int>{(lua_pushinteger(L, (lua_Integer)(args)), ++nargs)...};

    // Protected call
    int result = lua_pcall(L, nargs, 0, 0);
    if(result != 0)
    {
        const char *err = lua_tostring(L, -1);
        pLogWarning("Lua event %s error: %s", funcName, err ? err : "(no message)");
        lua_pop(L, 1);
    }
}


// ============================================================================
// Register subscription function
// ============================================================================

extern "C" int xtech_lua_subscribe_event(lua_State *L)
{
    // xtech_event_subscribe(eventName, {npcId1, npcId2, ...})
    const char *eventName = luaL_checkstring(L, 1);

    if(!lua_istable(L, 2))
    {
        luaL_error(L, "xtech_event_subscribe: second argument must be a table of IDs");
        return 0;
    }

    if(strcmp(eventName, "onNPCUpdate") == 0)
    {
        // Parse the table of NPC IDs
        lua_pushnil(L);
        while(lua_next(L, 2) != 0)
        {
            int npcId = (int)lua_tointeger(L, -1);
            s_npcUpdateSubscribed.insert(npcId);
            lua_pop(L, 1);
        }
    }

    s_subscribedEvents.insert(eventName);
    return 0;
}


// ============================================================================
// NPC Event dispatch
// ============================================================================

void xtech_lua_event_npcUpdate(int permId, int npcId)
{
    // Subscription check for performance
    if(s_npcUpdateSubscribed.find(npcId) == s_npcUpdateSubscribed.end())
        return;

    s_safeCall("onNPCUpdate", permId, npcId);
}

void xtech_lua_event_npcDeath(int permId, int npcId, int killerPlayerId)
{
    s_safeCall("onNPCDeath", permId, npcId, killerPlayerId);
}

void xtech_lua_event_npcHurt(int permId, int npcId, int hitterId, int hitType)
{
    s_safeCall("onNPCHurt", permId, npcId, hitterId, hitType);
}

void xtech_lua_event_npcActivate(int permId, int npcId)
{
    s_safeCall("onNPCActivate", permId, npcId);
}

void xtech_lua_event_npcTalk(int permId, int npcId, int playerId)
{
    s_safeCall("onNPCTalk", permId, npcId, playerId);
}

void xtech_lua_event_npcTouch(int permId, int npcId, int playerId, int side)
{
    s_safeCall("onNPCTouch", permId, npcId, playerId, side);
}

void xtech_lua_event_npcGrab(int permId, int npcId, int playerId, bool fromTop)
{
    s_safeCall("onNPCGrab", permId, npcId, playerId, (int)fromTop);
}


// ============================================================================
// Player Event dispatch
// ============================================================================

void xtech_lua_event_playerHurt(int playerId, int causeNpcId)
{
    s_safeCall("onPlayerHurt", playerId, causeNpcId);
}

void xtech_lua_event_playerDeath(int playerId, int causeNpcId)
{
    s_safeCall("onPlayerDeath", playerId, causeNpcId);
}

void xtech_lua_event_playerPowerUp(int playerId, int oldState, int newState)
{
    s_safeCall("onPlayerPowerUp", playerId, oldState, newState);
}

void xtech_lua_event_playerMount(int playerId, int mountType)
{
    s_safeCall("onPlayerMount", playerId, mountType);
}

void xtech_lua_event_playerDismount(int playerId, int mountType)
{
    s_safeCall("onPlayerDismount", playerId, mountType);
}

void xtech_lua_event_playerSwitchChar(int playerId, int oldChar, int newChar)
{
    s_safeCall("onPlayerSwitchChar", playerId, oldChar, newChar);
}

void xtech_lua_event_playerRespawn(int playerId)
{
    s_safeCall("onPlayerRespawn", playerId);
}


// ============================================================================
// Block Event dispatch
// ============================================================================

void xtech_lua_event_blockHit(int blockId, int blockType, int hitterId, int hitStyle)
{
    s_safeCall("onBlockHit", blockId, blockType, hitterId, hitStyle);
}

void xtech_lua_event_blockDestroy(int blockId, int blockType, int destroyerId)
{
    s_safeCall("onBlockDestroy", blockId, blockType, destroyerId);
}


// ============================================================================
// Level / Game Event dispatch
// ============================================================================

void xtech_lua_event_levelComplete()
{
    s_safeCall("onLevelComplete");
}

void xtech_lua_event_postMacro()
{
    s_safeCall("onPostMacro");
}

void xtech_lua_event_levelExit()
{
    s_safeCall("onLevelExit");
}

void xtech_lua_event_gameOver()
{
    s_safeCall("onGameOver");
}

void xtech_lua_event_pause()
{
    s_safeCall("onPause");
}

void xtech_lua_event_unpause()
{
    s_safeCall("onUnpause");
}


// ============================================================================
// Warp Event dispatch
// ============================================================================

void xtech_lua_event_warpEnter(int playerId, int warpId)
{
    s_safeCall("onWarpEnter", playerId, warpId);
}

void xtech_lua_event_warpExit(int playerId, int warpId)
{
    s_safeCall("onWarpExit", playerId, warpId);
}
