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

#pragma once
#ifndef XTECH_LUA_EVENTS_H
#define XTECH_LUA_EVENTS_H

// ============================================================================
// System Event Dispatch — call these from game code (#ifdef ENABLE_XTECH_LUA)
// Each function checks if the corresponding onXxx() Lua function is defined
// and subscribed (for high-frequency events), then calls it safely.
// ============================================================================

// --- NPC Events ---

// Per-frame NPC update. SUBSCRIPTION-REQUIRED: script must call
// xtech_event_subscribe("onNPCUpdate", {npcId, ...}) in onLoad()
void xtech_lua_event_npcUpdate(int permId, int npcId);

void xtech_lua_event_npcDeath(int permId, int npcId, int killerPlayerId);
void xtech_lua_event_npcHurt(int permId, int npcId, int hitterId, int hitType);
void xtech_lua_event_npcActivate(int permId, int npcId);
void xtech_lua_event_npcTalk(int permId, int npcId, int playerId);
void xtech_lua_event_npcTouch(int permId, int npcId, int playerId, int side);
void xtech_lua_event_npcGrab(int permId, int npcId, int playerId, bool fromTop);

// --- Player Events ---

void xtech_lua_event_playerHurt(int playerId, int causeNpcId);
void xtech_lua_event_playerDeath(int playerId, int causeNpcId);
void xtech_lua_event_playerPowerUp(int playerId, int oldState, int newState);
void xtech_lua_event_playerMount(int playerId, int mountType);
void xtech_lua_event_playerDismount(int playerId, int mountType);
void xtech_lua_event_playerSwitchChar(int playerId, int oldChar, int newChar);
void xtech_lua_event_playerRespawn(int playerId);

// --- Block Events ---

void xtech_lua_event_blockHit(int blockId, int blockType, int hitterId, int hitStyle);
void xtech_lua_event_blockDestroy(int blockId, int blockType, int destroyerId);

// --- Level / Game Events ---

void xtech_lua_event_levelComplete();
void xtech_lua_event_levelExit();
void xtech_lua_event_gameOver();
void xtech_lua_event_pause();
void xtech_lua_event_unpause();

// --- Warp Events ---

void xtech_lua_event_warpEnter(int playerId, int warpId);
void xtech_lua_event_warpExit(int playerId, int warpId);

#endif // XTECH_LUA_EVENTS_H
