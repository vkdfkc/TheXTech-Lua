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
#ifndef XTECH_LUA_BINDINGS_H
#define XTECH_LUA_BINDINGS_H

struct lua_State;

// Register all game bindings to the given Lua state
void xtech_lua_register_bindings(lua_State *L);

// Process delayed calls queue — call each frame from xtech_lua_loop()
void xtech_lua_process_delayed_calls();

// Clear all pending delayed calls — call from xtech_lua_reset()
void xtech_lua_clear_delayed_calls();

#endif // XTECH_LUA_BINDINGS_H
