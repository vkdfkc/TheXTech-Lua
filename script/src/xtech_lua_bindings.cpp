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
#include <string>
#include <list>
#include <vector>
#include <map>

extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <luabind/luabind.hpp>
#include <luabind/class.hpp>
#include <luabind/function.hpp>
#include <luabind/object.hpp>

#include <Logger/logger.h>
#include <Utils/files.h>
#include <fmt_format_ne.h>

#include "globals.h"
#include "global_dirs.h"
#include "global_strings.h"
#include "location.h"
#include "layers.h"
#include "player.h"
#include "npc.h"
#include "graphics.h"
#include "sound.h"
#include "collision.h"
#include "config.h"
#include "core/msgbox.h"
#include "game_main.h"
#include "npc_traits.h"
#include "effect.h"
#include "main/cheat_code.h"

#include "script/luna/lunaplayer.h"
#include "script/luna/lunanpc.h"
#include "script/luna/lunalayer.h"
#include "script/luna/lunablock.h"
#include "script/luna/lunadefs.h"
#include "script/luna/autocode_manager.h"
#include "script/luna/lunarender.h"
#include "core/render.h"
#include "script/luna/lunaspriteman.h"
#include "script/luna/lunaimgbox.h"
#include "script/luna/csprite.h"
#include "script/luna/sprite_component.h"
#include "script/luna/sprite_funcs.h"
#include "script/luna/renderop_string.h"
#include "script/luna/renderop_bitmap.h"
#include "script/luna/lunarender.h"
#include "core/render.h"

#include "xtech_lua_bindings.h"
#include "xtech_lua_main.h"
#include "xtech_lua_events.h"

// Forward declaration of event subscribe function (defined in xtech_lua_events.cpp)
extern "C" int xtech_lua_subscribe_event(lua_State *L);


// ============================================================================
// Helper functions
// ============================================================================

static inline int s_round2int(num_t d)
{
    return (int)num_t::floor(d + 0.5_n);
}

static FIELDTYPE StrToFieldtype(const std::string &string)
{
    if(string == "b")
        return FT_BYTE;
    else if(string == "s" || string == "w")
        return FT_WORD;
    else if(string == "dw")
        return FT_DWORD;
    else if(string == "f")
        return FT_FLOAT;
    else if(string == "df")
        return FT_DFLOAT;
    return FT_BYTE;
}

// ============================================================================
// NPC bitfield accessor wrappers (for bitfield members that luabind can't
// directly bind with def_readwrite)
// ============================================================================

namespace LuaNPCExt
{

// Generator bitfield
static bool npc_getGenerator(NPC_t *n) { return n ? n->Generator : false; }
static void npc_setGenerator(NPC_t *n, bool v) { if(n) n->Generator = v; }

static bool npc_getGeneratorActive(NPC_t *n) { return n ? n->GeneratorActive : false; }
static void npc_setGeneratorActive(NPC_t *n, bool v) { if(n) n->GeneratorActive = v; }

static bool npc_getChat(NPC_t *n) { return n ? n->Chat : false; }
static void npc_setChat(NPC_t *n, bool v) { if(n) n->Chat = v; }

static bool npc_getLegacy(NPC_t *n) { return n ? n->Legacy : false; }
static void npc_setLegacy(NPC_t *n, bool v) { if(n) n->Legacy = v; }

static bool npc_getTurnAround(NPC_t *n) { return n ? n->TurnAround : false; }
static void npc_setTurnAround(NPC_t *n, bool v) { if(n) n->TurnAround = v; }

static bool npc_getTurnBackWipe(NPC_t *n) { return n ? n->TurnBackWipe : false; }
static void npc_setTurnBackWipe(NPC_t *n, bool v) { if(n) n->TurnBackWipe = v; }

static bool npc_getplayerTemp(NPC_t *n) { return n ? n->playerTemp : false; }
static void npc_setplayerTemp(NPC_t *n, bool v) { if(n) n->playerTemp = v; }

static bool npc_getNoLavaSplash(NPC_t *n) { return n ? n->NoLavaSplash : false; }
static void npc_setNoLavaSplash(NPC_t *n, bool v) { if(n) n->NoLavaSplash = v; }

static bool npc_getBouce(NPC_t *n) { return n ? n->Bouce : false; }
static void npc_setBouce(NPC_t *n, bool v) { if(n) n->Bouce = v; }

static bool npc_getDefaultStuck(NPC_t *n) { return n ? n->DefaultStuck : false; }
static void npc_setDefaultStuck(NPC_t *n, bool v) { if(n) n->DefaultStuck = v; }

static bool npc_getRespawnDelay(NPC_t *n) { return n ? n->RespawnDelay : false; }
static void npc_setRespawnDelay(NPC_t *n, bool v) { if(n) n->RespawnDelay = v; }

// Generator field accessors (stored in Special3/Special4/Special5)
static int npc_getGeneratorDirection(NPC_t *n) { return n ? n->GeneratorDirection() : 0; }
static int npc_getGeneratorEffect(NPC_t *n) { return n ? n->GeneratorEffect() : 0; }
static int npc_getGeneratorTimeMax(NPC_t *n) { return n ? (int)n->GeneratorTimeMax() : 0; }
static void npc_setGeneratorTimeMax(NPC_t *n, int v) { if(n) n->GeneratorTimeMax() = v; }
static int npc_getGeneratorTime(NPC_t *n) { return n ? (int)n->GeneratorTime() : 0; }
static void npc_setGeneratorTime(NPC_t *n, int v) { if(n) n->GeneratorTime() = v; }

static bool npc_getStuck(NPC_t *n) { return n ? n->Stuck : false; }
static void npc_setStuck(NPC_t *n, bool v) { if(n) n->Stuck = v; }

static bool npc_getShadow(NPC_t *n) { return n ? n->Shadow : false; }
static void npc_setShadow(NPC_t *n, bool v) { if(n) n->Shadow = v; }

static int npc_getQuicksand(NPC_t *n) { return n ? (int)n->Quicksand : 0; }
static void npc_setQuicksand(NPC_t *n, int v) { if(n) n->Quicksand = (uint8_t)v; }

// getPermID: returns the 1-based array index of this NPC
static int npc_getPermID(NPC_t *n)
{
    if(!n) return -1;
    ptrdiff_t idx = n - const_cast<NPC_t*>(NPC.base());
    if(idx >= 0 && idx < numNPCs)
        return (int)(idx + 1); // 1-based
    return -1;
}

// Name getter/setter using string bank
static std::string npc_getName(NPC_t *n) {
    if(!n || n->Name == STRINGINDEX_NONE) return "";
    return GetS(n->Name);
}
static void npc_setName(NPC_t *n, const std::string &v) {
    if(n) SetS(n->Name, v);
}

} // namespace LuaNPCExt


// ============================================================================
// Player bitfield accessor wrappers
// ============================================================================

namespace LuaPlayerExt
{

static bool player_getGroundPound(Player_t *p) { return p ? p->GroundPound : false; }
static void player_setGroundPound(Player_t *p, bool v) { if(p) p->GroundPound = v; }

static bool player_getGroundPound2(Player_t *p) { return p ? p->GroundPound2 : false; }
static void player_setGroundPound2(Player_t *p, bool v) { if(p) p->GroundPound2 = v; }

static bool player_getCanPound(Player_t *p) { return p ? p->CanPound : false; }
static void player_setCanPound(Player_t *p, bool v) { if(p) p->CanPound = v; }

static bool player_getAltRunRelease(Player_t *p) { return p ? p->AltRunRelease : false; }
static void player_setAltRunRelease(Player_t *p, bool v) { if(p) p->AltRunRelease = v; }

static bool player_getDuckRelease(Player_t *p) { return p ? p->DuckRelease : false; }
static void player_setDuckRelease(Player_t *p, bool v) { if(p) p->DuckRelease = v; }

static bool player_getSlippyWall(Player_t *p) { return p ? p->SlippyWall : false; }
static void player_setSlippyWall(Player_t *p, bool v) { if(p) p->SlippyWall = v; }

static bool player_getJumpOffWall(Player_t *p) { return p ? p->JumpOffWall : false; }
static void player_setJumpOffWall(Player_t *p, bool v) { if(p) p->JumpOffWall = v; }

} // namespace LuaPlayerExt


// ============================================================================
// Player API wrappers
// ============================================================================

namespace LuaPlayer
{

static Player_t* get(int num)
{
    return PlayerF::Get(num);
}

static int count()
{
    return numPlayers;
}

static void filterToBig(Player_t *p)
{
    if(p) PlayerF::FilterToBig(p);
}

static void filterToSmall(Player_t *p)
{
    if(p) PlayerF::FilterToSmall(p);
}

static void filterToFire(Player_t *p)
{
    if(p) PlayerF::FilterToFire(p);
}

static void filterMount(Player_t *p)
{
    if(p) PlayerF::FilterMount(p);
}

static void filterReservePowerup(Player_t *p)
{
    if(p) PlayerF::FilterReservePowerup(p);
}

static void cycleRight(Player_t *p)
{
    if(p) PlayerF::CycleRight(p);
}

static void cycleLeft(Player_t *p)
{
    if(p) PlayerF::CycleLeft(p);
}

static void infiniteFlying(int player)
{
    PlayerF::InfiniteFlying(player);
}

static bool pressingUp(Player_t *p)
{
    return p ? PlayerF::PressingUp(p) : false;
}

static bool pressingDown(Player_t *p)
{
    return p ? PlayerF::PressingDown(p) : false;
}

static bool pressingLeft(Player_t *p)
{
    return p ? PlayerF::PressingLeft(p) : false;
}

static bool pressingRight(Player_t *p)
{
    return p ? PlayerF::PressingRight(p) : false;
}

static bool pressingJump(Player_t *p)
{
    return p ? PlayerF::PressingJump(p) : false;
}

static bool pressingRun(Player_t *p)
{
    return p ? PlayerF::PressingRun(p) : false;
}

static bool pressingSEL(Player_t *p)
{
    return p ? PlayerF::PressingSEL(p) : false;
}

static bool isHoldingSpriteType(Player_t *p, int npcId)
{
    return p ? PlayerF::IsHoldingSpriteType(p, npcId) : false;
}

static bool usesHearts(Player_t *p)
{
    return p ? PlayerF::UsesHearts(p) : false;
}

static void memSet(size_t offset, num_t value, int op, int ftype)
{
    PlayerF::MemSet(offset, value, (OPTYPE)op, (FIELDTYPE)ftype);
}

// P6: expose PlayerDismount — silent dismount (mount becomes NPC, no jump/sound)
static void dismount(Player_t *p)
{
    if(!p) return;
    int idx = (int)(p - &Player[1]) + 1; // 1-based player index
    if(idx >= 1 && idx <= numPlayers)
        ::PlayerDismount(idx, false);  // silent: no jump velocity, no sound
}

} // namespace LuaPlayer


// ============================================================================
// NPC API wrappers
// ============================================================================

namespace LuaNPC
{

static NPC_t* get(int index)
{
    return NpcF::Get(index);
}

static int count()
{
    return numNPCs;
}

static NPC_t* getFirstMatch(int id, int section)
{
    return NpcF::GetFirstMatch(id, section);
}

static void memSet(int id, size_t offset, num_t value, int op, int ftype)
{
    NpcF::MemSet(id, offset, value, (OPTYPE)op, (FIELDTYPE)ftype);
}

static void allSetHits(int identity, int section, int hits)
{
    NpcF::AllSetHits(identity, section, hits);
}

static void kill(int a, int b)
{
    KillNPC(a, b);
}

static void hurt(int a, int b, int c)
{
    NPCHit(a, b, c);
}

static NPC_t* getByPermID(int permId)
{
    if(permId >= 1 && permId <= numNPCs)
        return const_cast<NPC_t*>(&NPC[permId]);
    return nullptr;
}

static void forEach(int id, int section, luabind::object callback)
{
    if(!callback.is_valid()) return;

    for(int i = 1; i <= numNPCs; i++)
    {
        NPC_t *npc = const_cast<NPC_t*>(&NPC[i]);
        if(!npc->Active) continue;
        if(id != 0 && npc->Type != (NPCID)id) continue;
        if(section != 0 && npc->Section != section) continue;

        try
        {
            callback(npc);
        }
        catch(const luabind::error &e)
        {
            pLogWarning("Lua error in NPC forEach callback: %s",
                lua_tostring(xtech_lua_getState(), -1));
            lua_pop(xtech_lua_getState(), 1);
        }
        catch(const std::exception &e)
        {
            pLogWarning("Lua exception in NPC forEach callback: %s", e.what());
        }
    }
}

// P3: NCreate - dynamically spawn an NPC
static NPC_t* lua_npc_create(int type, num_t x, num_t y, num_t xspd, num_t yspd)
{
    if(numNPCs >= maxNPCs)
        return nullptr;

    numNPCs++;
    auto &n = NPC[numNPCs];
    n.Type = (NPCID)type;
    n.Location.X = x;
    n.Location.Y = y;
    n.Location.Width = NPCWidth(type);
    n.Location.Height = NPCHeight(type);
    n.Location.SpeedX = xspd;
    n.Location.SpeedY = yspd;
    n.Active = true;
    n.TimeLeft = 100;
    n.Direction = (xspd >= 0 ? 1 : -1);
    n.Frame = 0;
    n.FrameCount = 0;
    n.Reset.fill(false);

    syncLayers_NPC(numNPCs);
    CheckSectionNPC(numNPCs);

    return const_cast<NPC_t*>(&NPC[numNPCs]);
}

// getByName
static NPC_t* npc_getByName(const std::string &name) {
    for(int i = 1; i <= numNPCs; i++)
        if(NPC[i].Name != STRINGINDEX_NONE && GetS(NPC[i].Name) == name)
            return const_cast<NPC_t*>(&NPC[i]);
    return nullptr;
}

} // namespace LuaNPC


// ============================================================================
// Layer API wrappers
// ============================================================================

namespace LuaLayer
{

static Layer_t* get(int index)
{
    return LayerF::Get(index);
}

static int count()
{
    return numLayers;
}

static void setXSpeed(Layer_t *layer, num_t speed)
{
    if(layer) LayerF::SetXSpeed(layer, speed);
}

static void setYSpeed(Layer_t *layer, num_t speed)
{
    if(layer) LayerF::SetYSpeed(layer, speed);
}

static void stop(Layer_t *layer)
{
    if(layer) LayerF::Stop(layer);
}

} // namespace LuaLayer


// ============================================================================
// Block API wrappers
// ============================================================================

namespace LuaBlock
{

static Block_t* get(int index)
{
    return BlocksF::Get(index);
}

static int count()
{
    return numBlock;
}

static void setAll(int type1, int type2)
{
    BlocksF::SetAll(type1, type2);
}

static void swapAll(int type1, int type2)
{
    BlocksF::SwapAll(type1, type2);
}

static void showAll(int type)
{
    BlocksF::ShowAll(type);
}

static void hideAll(int type)
{
    BlocksF::HideAll(type);
}

static bool isPlayerTouchingType(int blockType, int collision, Player_t *player)
{
    if(!player) return false;
    return BlocksF::IsPlayerTouchingType(blockType, collision, player);
}

static Block_t* getByPermID(int permId)
{
    if(permId >= 1 && permId <= numBlock)
        return BlocksF::Get(permId);
    return nullptr;
}
static Block_t* getByPermID_wrapper(int permId) { return getByPermID(permId); }

static void forEach(int type, luabind::object callback)
{
    if(!callback.is_valid()) return;

    for(int i = 1; i <= numBlock; i++)
    {
        Block_t *block = const_cast<Block_t*>(&Block[i]);
        if(type != 0 && block->Type != type) continue;

        try
        {
            callback(block);
        }
        catch(const luabind::error &e)
        {
            pLogWarning("Lua error in Block forEach callback: %s",
                lua_tostring(xtech_lua_getState(), -1));
            lua_pop(xtech_lua_getState(), 1);
        }
        catch(const std::exception &e)
        {
            pLogWarning("Lua exception in Block forEach callback: %s", e.what());
        }
    }
}

// getPermID: returns the 1-based array index of this Block
static int block_getPermID(Block_t *b)
{
    if(!b) return -1;
    ptrdiff_t idx = b - const_cast<Block_t*>(Block.base());
    if(idx >= 0 && idx < numBlock)
        return (int)(idx + 1); // 1-based
    return -1;
}

// Name getter/setter using string bank
static std::string block_getName(Block_t *b) {
    if(!b || b->Name == STRINGINDEX_NONE) return "";
    return GetS(b->Name);
}
static void block_setName(Block_t *b, const std::string &v) {
    if(b) SetS(b->Name, v);
}

// getByName
static Block_t* block_getByName(const std::string &name) {
    for(int i = 1; i <= numBlock; i++)
        if(Block[i].Name != STRINGINDEX_NONE && GetS(Block[i].Name) == name)
            return const_cast<Block_t*>(&Block[i]);
    return nullptr;
}

} // namespace LuaBlock


// ============================================================================
// Audio API wrappers
// ============================================================================

namespace LuaAudio
{

static void playSFX(int index, int loops, int volume)
{
    if(index > 0)
        PlaySound(index, loops, volume);
}

static void playSFXExt(const std::string &filename, int loops, int volume)
{
    if(!filename.empty())
    {
        std::string full_path = g_dirCustom.resolveFileCaseAbs(filename);
        PlayExtSound(full_path, loops, volume);
    }
}

static void stopSFXExt(const std::string &filename)
{
    if(!filename.empty())
    {
        std::string full_path = g_dirCustom.resolveFileCaseAbs(filename);
        StopExtSound(full_path);
    }
}

static void preloadSFXExt(const std::string &filename)
{
    if(!filename.empty())
    {
        std::string full_path = g_dirCustom.resolveFileCaseAbs(filename);
        PreloadExtSound(full_path);
    }
}

static void playMusic(int section, int fadeInMs)
{
    StartMusic(section, fadeInMs);
}

static void playMusicFile(const std::string &filename, int fadeInMs)
{
    if(!filename.empty())
        PlayMusic(filename, fadeInMs);
}

static void setMusic(int sec, int musicId, const std::string &filename)
{
    if(sec >= 0 && sec < numSections)
    {
        if(musicId >= 0 && musicId <= 24)
            bgMusic[sec] = musicId;
        if(filename.length() >= 5)
            CustomMusic[sec] = filename;
    }
}

} // namespace LuaAudio


// ============================================================================
// HUD / Render API wrappers
// ============================================================================

extern void HudRenderNPC(int npcId, int x, int y, int w, int h);
extern void HudRenderImage(int x, int y, int w, int h, StdPicture &tex, int sx, int sy);

// Simple texture storage for HUD images — uses game's native LoadPicture,
// avoiding LunaImage move-semantics issues.
static std::map<int, StdPicture> s_hudTextures;

namespace LuaHUD
{

// NOTE: parameters use double (not num_t) because luabind cannot convert
// Lua numbers to num_t (num_t's double constructor is explicitly deleted).
static void showText(const std::string &text, double x, double y, double font)
{
    if(text.empty())
        return;
    int iFont = static_cast<int>(font);
    if(iFont < 1 || iFont > 5)
        iFont = 3;

    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    int len = static_cast<int>(text.size());
    SuperPrint(len, text.c_str(), iFont, ix, iy);
}

static void showLevelName(double x, double y, double font)
{
    int iFont = static_cast<int>(font);
    if(iFont < 1 || iFont > 5)
        iFont = 3;
    const std::string &name = LevelName.empty() ? FileName : LevelName;
    if(name.empty())
        return;
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    int len = static_cast<int>(name.size());
    SuperPrint(len, name.c_str(), iFont, ix, iy);
}

static void showLevelFile(double x, double y, double font)
{
    int iFont = static_cast<int>(font);
    if(iFont < 1 || iFont > 5)
        iFont = 3;
    if(FileName.empty())
        return;
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    int len = static_cast<int>(FileName.size());
    SuperPrint(len, FileName.c_str(), iFont, ix, iy);
}

static void showImage(double imgResourceCode, double x, double y,
    double sx, double sy, double sw, double sh)
{
    int code = static_cast<int>(imgResourceCode);
    if(code == 0)
        return;

    auto it = s_hudTextures.find(code);
    if(it == s_hudTextures.end() || !it->second.inited)
        return;

    StdPicture &tex = it->second;

    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    int isx = static_cast<int>(sx);
    int isy = static_cast<int>(sy);
    int isw = static_cast<int>(sw);
    int ish = static_cast<int>(sh);

    if(isw <= 0) isw = tex.w;
    if(ish <= 0) ish = tex.h;
    if(isw <= 0 || ish <= 0)
        return;

    HudRenderImage(ix, iy, isw, ish, tex, isx, isy);
}

static void showNPC(double npcId, double x, double y, double w, double h)
{
    HudRenderNPC(static_cast<int>(npcId),
        static_cast<int>(x), static_cast<int>(y),
        static_cast<int>(w), static_cast<int>(h));
}

static void debugPrint(const std::string &text)
{
    pLogDebug("Lua: %s", text.c_str());
}

} // namespace LuaHUD


// ============================================================================
// Variable API wrappers
// ============================================================================

namespace LuaVar
{

static num_t getVar(const std::string &name)
{
    return gAutoMan.GetVar(name);
}

static bool varExists(const std::string &name)
{
    return gAutoMan.VarExists(name);
}

static bool varOperation(const std::string &name, num_t value, int op)
{
    return gAutoMan.VarOperation(name, value, (OPTYPE)op);
}

} // namespace LuaVar


// ============================================================================
// Event API wrappers
// ============================================================================

namespace LuaEvent
{

static void trigger(int section, int eventId)
{
    gAutoMan.ActivateCustomEvents(section, eventId);
}

static void triggerByName(const std::string &eventName)
{
    Autocode *code = gAutoMan.GetEventByRef(eventName);
    if(code)
    {
        code->Activated = true;
        code->Expired = false;
    }
}

static void cancelByName(const std::string &eventName)
{
    Autocode *code = gAutoMan.GetEventByRef(eventName);
    if(code)
        gAutoMan.DeleteEvent(eventName);
}

} // namespace LuaEvent


// ============================================================================
// BGO (Background Object) API wrappers
// ============================================================================

namespace LuaBGO
{

static Background_t* get(int index)
{
    if(index >= 1 && index <= numBackground)
        return const_cast<Background_t*>(&Background[index]);
    return nullptr;
}

static int count()
{
    return numBackground;
}

static Background_t* getByPermID(int permId)
{
    if(permId >= 1 && permId <= numBackground)
        return const_cast<Background_t*>(&Background[permId]);
    return nullptr;
}

static void forEach(int type, luabind::object callback)
{
    if(!callback.is_valid()) return;

    for(int i = 1; i <= numBackground; i++)
    {
        Background_t *bgo = const_cast<Background_t*>(&Background[i]);
        if(type != 0 && bgo->Type != type) continue;

        try
        {
            callback(bgo);
        }
        catch(const luabind::error &e)
        {
            pLogWarning("Lua error in BGO forEach callback: %s",
                lua_tostring(xtech_lua_getState(), -1));
            lua_pop(xtech_lua_getState(), 1);
        }
        catch(const std::exception &e)
        {
            pLogWarning("Lua exception in BGO forEach callback: %s", e.what());
        }
    }
}

// getPermID: returns the 1-based array index of this BGO
static int bgo_getPermID(Background_t *b)
{
    if(!b) return -1;
    ptrdiff_t idx = b - const_cast<Background_t*>(Background.base());
    if(idx >= 0 && idx < numBackground)
        return (int)(idx + 1); // 1-based
    return -1;
}

} // namespace LuaBGO


// ============================================================================
// Water / Liquid API wrappers
// ============================================================================

namespace LuaWater
{

static Water_t* get(int index)
{
    if(index >= 0 && index <= numWater)
        return const_cast<Water_t*>(&Water[index]);
    return nullptr;
}

static int count()
{
    return numWater;
}

static Water_t* getByPermID(int permId)
{
    if(permId >= 0 && permId <= numWater)
        return const_cast<Water_t*>(&Water[permId]);
    return nullptr;
}

// getPermID: returns the 0-based array index of this Water/Liquid
static int water_getPermID(Water_t *w)
{
    if(!w) return -1;
    ptrdiff_t idx = w - const_cast<Water_t*>(Water.base());
    if(idx >= 0 && idx < numWater)
        return (int)(idx + 0); // 0-based
    return -1;
}

// Name getter/setter using string bank
static std::string water_getName(Water_t *w) {
    if(!w || w->Name == STRINGINDEX_NONE) return "";
    return GetS(w->Name);
}
static void water_setName(Water_t *w, const std::string &v) {
    if(w) SetS(w->Name, v);
}

// getByName
static Water_t* water_getByName(const std::string &name) {
    for(int i = 0; i <= numWater; i++)
        if(Water[i].Name != STRINGINDEX_NONE && GetS(Water[i].Name) == name)
            return const_cast<Water_t*>(&Water[i]);
    return nullptr;
}

} // namespace LuaWater


// ============================================================================
// Warp API wrappers
// ============================================================================

namespace LuaWarp
{

static Warp_t* get(int index)
{
    if(index >= 1 && index <= numWarps)
        return const_cast<Warp_t*>(&Warp[index]);
    return nullptr;
}

static int count()
{
    return numWarps;
}

static Warp_t* getByPermID(int permId)
{
    if(permId >= 1 && permId <= numWarps)
        return const_cast<Warp_t*>(&Warp[permId]);
    return nullptr;
}

// getPermID: returns the 1-based array index of this Warp
static int warp_getPermID(Warp_t *w)
{
    if(!w) return -1;
    ptrdiff_t idx = w - const_cast<Warp_t*>(Warp.base());
    if(idx >= 0 && idx < numWarps)
        return (int)(idx + 1); // 1-based
    return -1;
}

} // namespace LuaWarp


// ============================================================================
// Section API wrappers
// ============================================================================

namespace LuaSection
{

static SpeedlessLocation_t* get(int index)
{
    if(index >= 0 && index <= numSections)
        return &level[index];
    return nullptr;
}

static int count()
{
    return numSections;
}

static int getBackground(int index)
{
    if(index >= 0 && index <= numSections)
        return Background2[index];
    return 0;
}

static void setBackground(int index, int bgId)
{
    if(index >= 0 && index <= numSections)
        Background2[index] = bgId;
}

static int getMusic(int index)
{
    if(index >= 0 && index <= numSections)
        return bgMusic[index];
    return 0;
}

static void setMusic(int index, int musicId)
{
    if(index >= 0 && index <= numSections)
        bgMusic[index] = musicId;
}

static const std::string& getMusicFile(int index)
{
    static std::string empty;
    if(index >= 0 && index <= numSections)
        return CustomMusic[index];
    return empty;
}

static void setMusicFile(int index, const std::string &filename)
{
    if(index >= 0 && index <= numSections)
        CustomMusic[index] = filename;
}

static bool getOffScreenExit(int index)
{
    if(index >= 0 && index <= numSections)
        return OffScreenExit[index];
    return false;
}

static void setOffScreenExit(int index, bool value)
{
    if(index >= 0 && index <= numSections)
        OffScreenExit[index] = value;
}

} // namespace LuaSection


// ============================================================================
// Effect API wrappers (P3 - FXCreate equivalent)
// ============================================================================

namespace LuaEffect
{

static Effect_t* get(int index)
{
    if(index >= 1 && index <= numEffects)
        return const_cast<Effect_t*>(&Effect[index]);
    return nullptr;
}

static int count()
{
    return numEffects;
}

static Effect_t* create(int effId, num_t x, num_t y, int direction, bool shadow)
{
    Location_t loc;
    loc.X = x;
    loc.Y = y;
    if(NewEffect((EFFID)effId, loc, direction, shadow))
        return const_cast<Effect_t*>(&Effect[numEffects]);
    return nullptr;
}

static void kill(int index)
{
    if(index >= 1 && index <= numEffects)
        KillEffect(index);
}

} // namespace LuaEffect


// ============================================================================
// Misc API wrappers
// ============================================================================

namespace LuaMisc
{

static int getFrame()
{
    return (int)CommonFrame;
}

static int getSection(Player_t *p)
{
    return p ? (int)p->Section : 0;
}

static void showMsg(const std::string &text)
{
    if(!text.empty())
    {
        MessageText = text;
        g_MessageType = MESSAGE_TYPE_NORMAL;
        PauseGame(PauseCode::Message);
    }
}

static void showMsgInfo(const std::string &text)
{
    if(!text.empty())
    {
        MessageText = text;
        g_MessageType = MESSAGE_TYPE_SYS_INFO;
        PauseGame(PauseCode::Message);
    }
}

static void showMsgWarn(const std::string &text)
{
    if(!text.empty())
    {
        MessageText = text;
        g_MessageType = MESSAGE_TYPE_SYS_WARNING;
        PauseGame(PauseCode::Message);
    }
}

static void cheat(const std::string &code)
{
    if(!code.empty())
    {
        cheats_setBuffer(code, true);
    }
}

static void logMsg(const std::string &msg)
{
    pLogInfo("Lua: %s", msg.c_str());
}

static void logWarn(const std::string &msg)
{
    pLogWarning("Lua: %s", msg.c_str());
}

static void logDebug(const std::string &msg)
{
    pLogDebug("Lua: %s", msg.c_str());
}

// ============================================================================
// Sysval - global game state variables (P3)
// ============================================================================

static int sysval_getLives() { return Lives; }
static void sysval_setLives(int v) { Lives = v; }

static int sysval_getCoins() { return Coins; }
static void sysval_setCoins(int v) { Coins = v; }

static int sysval_getScore() { return Score; }
static void sysval_setScore(int v) { Score = v; }

static int sysval_getScreenX(int screen)
{
    if(screen >= 0 && screen <= 2)
        return (int)vScreen[screen].X;
    return 0;
}

static int sysval_getScreenY(int screen)
{
    if(screen >= 0 && screen <= 2)
        return (int)vScreen[screen].Y;
    return 0;
}

static int sysval_getScreenWidth(int screen)
{
    if(screen >= 0 && screen <= 2)
        return (int)vScreen[screen].Width;
    return 800;
}

static int sysval_getScreenHeight(int screen)
{
    if(screen >= 0 && screen <= 2)
        return (int)vScreen[screen].Height;
    return 600;
}

static int sysval_getScreenTop(int screen)
{
    if(screen >= 0 && screen <= 2)
    {
        int h = (int)vScreen[screen].Height;
        return (h > 600) ? h / 2 - 300 : 0;
    }
    return 0;
}

static int sysval_getScreenCenterX(int screen)
{
    if(screen >= 0 && screen <= 2)
        return (int)vScreen[screen].Width / 2;
    return 400;
}

static bool sysval_getShowHud() { return ShowOnScreenHUD; }
static void sysval_setShowHud(bool v) { ShowOnScreenHUD = v; }

static bool sysval_getShowInterface() { return ShowInterface; }
static void sysval_setShowInterface(bool v) { ShowInterface = v; }

static bool sysval_getBattleMode() { return BattleMode; }

static bool sysval_getEndLevel() { return EndLevel; }
static void sysval_setEndLevel(bool v) { EndLevel = v; }

static int sysval_getLevelMacro() { return (int)LevelMacro; }
static void sysval_setLevelMacro(int v) { LevelMacro = (LevelMacro_t)v; }
static int sysval_getLevelMacroCounter() { return LevelMacroCounter; }

static int sysval_getLevelBeatCode() { return (int)LevelBeatCode; }
static void sysval_setLevelBeatCode(int v) { LevelBeatCode = (LevelBeatCode_t)v; }

static int sysval_getGameTime() { return (int)CommonFrame; }

static int sysval_getCheckpointCount() { return (int)CheckpointsList.size(); }
static int sysval_getCheckpointId(int index)
{
    // 1-based index matching Lua convention
    if(index < 1 || size_t(index) > CheckpointsList.size())
        return 0;
    return CheckpointsList[size_t(index) - 1].id;
}


static std::string sysval_getLevelName()
{
    return LevelName.empty() ? FileName : LevelName;
}


static int misc_getStrWidth(const std::string &text, double font)
{
    int iFont = static_cast<int>(font);
    if(iFont < 1 || iFont > 5)
        iFont = 3;
    if(text.empty())
        return 0;
    return SuperTextPixLen(text, iFont);
}

static int misc_getStrWidth(const std::string &text)
{
    return misc_getStrWidth(text, 3.0);
}



// ============================================================================
// Async/Delay support (P3: named timers)

struct LuaDelayedCall
{
    int framesLeft;
    std::string name;      // P3: timer name (empty = anonymous)
    luabind::object callback;
};

static std::list<LuaDelayedCall> g_delayedCalls;

// Helper function for delayed calls (raw C bridge for luabind compatibility)
static int lua_misc_wait_raw(lua_State *L)
{
    // args: callback (at 1), frames (at 2)
    int frames = (int)luaL_checkinteger(L, 2);
    if(frames <= 0) frames = 1;
    lua_pushvalue(L, 1); // copy callback
    luabind::object cb(luabind::from_stack(L, -1));
    lua_pop(L, 1);
    g_delayedCalls.push_back({frames, std::string(), cb});
    return 0;
}

// Helper function for delayed calls, exposed to Lua
static void lua_misc_wait(luabind::object callback, int frames)
{
    if(frames <= 0) frames = 1;
    g_delayedCalls.push_back({frames, std::string(), callback});
}

// P3: Named timer - create or replace (raw C bridge)
static int lua_timer_create_raw(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int frames = (int)luaL_checkinteger(L, 3);
    if(frames <= 0) frames = 1;
    lua_pushvalue(L, 2);
    luabind::object cb(luabind::from_stack(L, -1));
    lua_pop(L, 1);
    for(auto &dc : g_delayedCalls)
    {
        if(!dc.name.empty() && dc.name == name)
        {
            dc.framesLeft = frames;
            dc.callback = cb;
            return 0;
        }
    }
    g_delayedCalls.push_back({frames, name, cb});
    return 0;
}

// P3: Named timer - create or replace a named timer
static void lua_timer_create(const std::string &name, luabind::object callback, int frames)
{
    if(frames <= 0) frames = 1;
    for(auto &dc : g_delayedCalls)
    {
        if(!dc.name.empty() && dc.name == name)
        {
            dc.framesLeft = frames;
            dc.callback = callback;
            return;
        }
    }
    g_delayedCalls.push_back({frames, name, callback});
}

// P3: Delay-trigger a named game event (TeaScript TCreate equivalent)
// Creates a timer that fires xtech_event_triggerByName(eventName) after delay
static int lua_timer_createEvent_raw(lua_State *L)
{
    const char *eventName = luaL_checkstring(L, 1);
    int frames = (int)luaL_checkinteger(L, 2);
    if(frames <= 0) frames = 1;
    // Build a Lua callback that triggers the event
    std::string luaCode = "xtech_event_triggerByName(\"";
    luaCode += eventName;
    luaCode += "\")";
    if(luaL_loadstring(L, luaCode.c_str()) == 0)
    {
        luabind::object cb(luabind::from_stack(L, -1));
        lua_pop(L, 1);
        // Use empty name for timer (TClear by event name is done via cancel)
        // Use the event name as timer name for cancel support
        for(auto &dc : LuaMisc::g_delayedCalls)
        {
            if(!dc.name.empty() && dc.name == eventName)
            {
                dc.framesLeft = frames;
                dc.callback = cb;
                return 0;
            }
        }
        LuaMisc::g_delayedCalls.push_back({frames, eventName, cb});
    }
    else
    {
        lua_pop(L, 1); // pop error string
    }
    return 0;
}

// P3: Cancel a named timer
static void lua_timer_cancel(const std::string &name)
{
    for(auto it = g_delayedCalls.begin(); it != g_delayedCalls.end(); )
    {
        if(it->name == name)
            it = g_delayedCalls.erase(it);
        else
            ++it;
    }
}

// P3: Cancel all timers (both named and anonymous)
static void lua_timer_clearAll()
{
    g_delayedCalls.clear();
}

} // namespace LuaMisc


// Process delayed calls 鈥?called from xtech_lua_loop()
void xtech_lua_process_delayed_calls()
{
    if(LuaMisc::g_delayedCalls.empty())
        return;

    for(auto it = LuaMisc::g_delayedCalls.begin(); it != LuaMisc::g_delayedCalls.end(); )
    {
        it->framesLeft--;
        if(it->framesLeft <= 0)
        {
            try
            {
                it->callback();
            }
            catch(const luabind::error &e)
            {
                pLogWarning("Lua error in delayed call: %s",
                    lua_tostring(xtech_lua_getState(), -1));
                lua_pop(xtech_lua_getState(), 1);
            }
            catch(const std::exception &e)
            {
                pLogWarning("Lua exception in delayed call: %s", e.what());
            }
            it = LuaMisc::g_delayedCalls.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void xtech_lua_clear_delayed_calls()
{
    LuaMisc::g_delayedCalls.clear();
}


// ============================================================================
// Sprite API wrappers
// ============================================================================

namespace LuaSprite
{

// Resolve an image path for HUD use (level custom dir first, then episode dir)
static std::string resolveHudImagePath(const std::string &filename)
{
    std::string path = AutocodeManager::resolveCustomFileCase(filename);
    if(!path.empty())
        return path;
    return AutocodeManager::resolveWorldFileCase(filename);
}

static void loadImage(const std::string &filename, double resourceCode, uint32_t transColor)
{
    int code = static_cast<int>(resourceCode);
    if(code == 0)
        return;

    // Load via game's native system for showImage
    std::string path = resolveHudImagePath(filename);
    if(!path.empty())
    {
        auto it = s_hudTextures.find(code);
        if(it != s_hudTextures.end())
            it->second.reset();
        XRender::LoadPicture(s_hudTextures[code], path);
        // Apply transparent color for BMP/JPG (PNG uses alpha channel)
        if(Files::hasSuffix(path, ".jpg") || Files::hasSuffix(path, ".bmp"))
            XRender::setTransparentColor(s_hudTextures[code], transColor);
    }

    // Also load via LunaRender for backward compat (sprites etc.)
    Renderer::Get().LoadBitmapResource(filename, code, transColor);
}

static void loadImageSimple(const std::string &filename, double resourceCode)
{
    int code = static_cast<int>(resourceCode);
    if(code == 0)
        return;

    // Load via game's native system for showImage
    std::string path = resolveHudImagePath(filename);
    if(!path.empty())
    {
        auto it = s_hudTextures.find(code);
        if(it != s_hudTextures.end())
            it->second.reset();
        XRender::LoadPicture(s_hudTextures[code], path);
    }

    // Also load via LunaRender for backward compat (sprites etc.)
    Renderer::Get().LoadBitmapResource(filename, code);
}

static LunaImage* getImage(double resourceCode)
{
    return Renderer::Get().GetImageForResourceCode(static_cast<int>(resourceCode));
}

static bool deleteImage(double resourceCode)
{
    int code = static_cast<int>(resourceCode);

    // Clear from HUD texture map
    auto it = s_hudTextures.find(code);
    if(it != s_hudTextures.end())
    {
        it->second.reset();
        s_hudTextures.erase(it);
    }

    // Also clear from LunaRender
    return Renderer::Get().DeleteImage(code);
}

// Place a sprite into the world
static CSprite* placeSprite(double type, double imgResourceCode, double x, double y, double lifetime)
{
    CSpriteRequest req;
    req.type = static_cast<int>(type);
    req.img_resource_code = static_cast<int>(imgResourceCode);
    req.x = static_cast<int>(x);
    req.y = static_cast<int>(y);
    req.time = static_cast<int>(lifetime);
    gSpriteMan.InstantiateSprite(&req, true);
    // Return the most recently added sprite
    if(!gSpriteMan.m_SpriteList.empty())
        return gSpriteMan.m_SpriteList.back();
    return nullptr;
}

static CSprite* placeSpriteExt(double type, double imgResourceCode, double x, double y, double lifetime,
    num_t xSpeed, num_t ySpeed, bool spawned)
{
    CSpriteRequest req;
    req.type = static_cast<int>(type);
    req.img_resource_code = static_cast<int>(imgResourceCode);
    req.x = static_cast<int>(x);
    req.y = static_cast<int>(y);
    req.time = static_cast<int>(lifetime);
    req.x_speed = xSpeed;
    req.y_speed = ySpeed;
    req.spawned = spawned;
    gSpriteMan.InstantiateSprite(&req, true);
    if(!gSpriteMan.m_SpriteList.empty())
        return gSpriteMan.m_SpriteList.back();
    return nullptr;
}

static int spriteCount()
{
    return gSpriteMan.CountSprites();
}

static void clearAllSprites()
{
    gSpriteMan.ClearAllSprites();
}

static void clearSpritesByCode(double imgResourceCode)
{
    gSpriteMan.ClearSprites(static_cast<int>(imgResourceCode));
}

static void addBlueprint(const std::string &name, CSprite *spr)
{
    if(spr)
        gSpriteMan.AddBlueprint(name.c_str(), spr);
}

static CSprite* copyFromBlueprint(const std::string &name)
{
    return gSpriteMan.CopyFromBlueprint(name.c_str());
}

} // namespace LuaSprite


// ============================================================================
// Main binding registration
// ============================================================================

// Helper macro to define an NPC ID constant in luabind
// (disabled: GCC 12 + luabind has call_types<int> bug; using raw C API instead)

// Helper macro to define an SFX constant in luabind

// Helper macro to define an EFFID constant in luabind


void xtech_lua_register_bindings(lua_State *L)
{
    using namespace luabind;

    module(L)
    [
        // ================================================================
        // Location_t class binding
        // ================================================================
        class_<Location_t>("Location")
            .def(constructor<>())
            .def_readwrite("X", &Location_t::X)
            .def_readwrite("Y", &Location_t::Y)
            .def_readwrite("Width", &Location_t::Width)
            .def_readwrite("Height", &Location_t::Height)
            .def_readwrite("SpeedX", &Location_t::SpeedX)
            .def_readwrite("SpeedY", &Location_t::SpeedY)
            .def("luaX", &Location_t::luaX)
            .def("luaY", &Location_t::luaY),

        // ================================================================
        // LunaRect struct binding (for sprite rendering rects)
        // ================================================================
        class_<LunaRect>("LunaRect")
            .def(constructor<>())
            .def_readwrite("left", &LunaRect::left)
            .def_readwrite("top", &LunaRect::top)
            .def_readwrite("right", &LunaRect::right)
            .def_readwrite("bottom", &LunaRect::bottom),

        // ================================================================
        // Hitbox struct binding
        // ================================================================
        class_<Hitbox>("Hitbox")
            .def(constructor<>())
            .def_readwrite("Left_off", &Hitbox::Left_off)
            .def_readwrite("Top_off", &Hitbox::Top_off)
            .def_readwrite("W", &Hitbox::W)
            .def_readwrite("H", &Hitbox::H)
            .def_readwrite("CollisionType", &Hitbox::CollisionType),

        // ================================================================
        // SpriteComponent struct binding
        // ================================================================
        class_<SpriteComponent>("SpriteComponent")
            .def_readwrite("data1", &SpriteComponent::data1)
            .def_readwrite("data2", &SpriteComponent::data2)
            .def_readwrite("data3", &SpriteComponent::data3)
            .def_readwrite("data4", &SpriteComponent::data4)
            .def_readwrite("lookup_code", &SpriteComponent::lookup_code)
            .def_readwrite("run_time", &SpriteComponent::run_time)
            .def_readwrite("org_time", &SpriteComponent::org_time)
            .def_readwrite("data5", &SpriteComponent::data5)
            .def_readwrite("expired", &SpriteComponent::expired),

        // ================================================================
        // LunaImage class binding (loaded image resource)
        // ================================================================
        class_<LunaImage>("LunaImage")
            .def("getWidth", &LunaImage::getW)
            .def("getHeight", &LunaImage::getH)
            .def("getUID", &LunaImage::getUID)
            .def("isLoaded", &LunaImage::ImageLoaded),

        // ================================================================
        // CSprite class binding
        // ================================================================
        class_<CSprite>("CSprite")
            .def_readwrite("ImgResCode", &CSprite::m_ImgResCode)
            .def_readwrite("CollisionCode", &CSprite::m_CollisionCode)
            .def_readwrite("FramesLeft", &CSprite::m_FramesLeft)
            .def_readwrite("DrawPriorityLevel", &CSprite::m_DrawPriorityLevel)
            .def_readwrite("OffscreenCount", &CSprite::m_OffscreenCount)
            .def_readwrite("FrameCounter", &CSprite::m_FrameCounter)
            .def_readwrite("GfxXOffset", &CSprite::m_GfxXOffset)
            .def_readwrite("GfxYOffset", &CSprite::m_GfxYOffset)
            .def_readwrite("StaticScreenPos", &CSprite::m_StaticScreenPos)
            .def_readwrite("Visible", &CSprite::m_Visible)
            .def_readwrite("Birthed", &CSprite::m_Birthed)
            .def_readwrite("Died", &CSprite::m_Died)
            .def_readwrite("Invalidated", &CSprite::m_Invalidated)
            .def_readwrite("LimitedFrameLife", &CSprite::m_LimitedFrameLife)
            .def_readwrite("AnimationSet", &CSprite::m_AnimationSet)
            .def_readwrite("AlwaysProcess", &CSprite::m_AlwaysProcess)
            .def_readwrite("Xpos", &CSprite::m_Xpos)
            .def_readwrite("Ypos", &CSprite::m_Ypos)
            .def_readwrite("Ht", &CSprite::m_Ht)
            .def_readwrite("Wd", &CSprite::m_Wd)
            .def_readwrite("Xspd", &CSprite::m_Xspd)
            .def_readwrite("Yspd", &CSprite::m_Yspd)
            .def_readwrite("Hitbox", &CSprite::m_Hitbox)
            .def_readwrite("AnimationPhase", &CSprite::m_AnimationPhase)
            .def_readwrite("AnimationTimer", &CSprite::m_AnimationTimer)
            .def_readwrite("AnimationFrame", &CSprite::m_AnimationFrame)
            .def("getExtX", &CSprite::GetExtX)
            .def("getExtY", &CSprite::GetExtY)
            .def("getFrameCols", &CSprite::GetFrameCols)
            .def("setImage", &CSprite::SetImage)
            .def("setImageResource", &CSprite::SetImageResource)
            .def("makeLimitedLife", &CSprite::MakeLimitedLifetime)
            .def("setCustomVar", &CSprite::SetCustomVar)
            .def("customVarExists", &CSprite::CustomVarExists)
            .def("getCustomVar", &CSprite::GetCustomVar)
            .def("birth", &CSprite::Birth)
            .def("die", &CSprite::Die),

        // ================================================================
        // NPC_t class binding (read/write access to key fields)
        // ================================================================
        class_<NPC_t>("NPC")
            .def_readwrite("Type", &NPC_t::Type)
            .def_readwrite("Killed", &NPC_t::Killed)
            .def_readwrite("Frame", &NPC_t::Frame)
            .def_readwrite("Active", &NPC_t::Active)
            .def_readwrite("Hidden", &NPC_t::Hidden)
            .def_readwrite("Inert", &NPC_t::Inert)
            .def_readwrite("Location", &NPC_t::Location)
            .def_readwrite("SpecialX", &NPC_t::SpecialX)
            .def_readwrite("SpecialY", &NPC_t::SpecialY)
            .def_readwrite("Special", &NPC_t::Special)
            .def_readwrite("Special2", &NPC_t::Special2)
            .def_readwrite("Special3", &NPC_t::Special3)
            .def_readwrite("Special4", &NPC_t::Special4)
            .def_readwrite("Special5", &NPC_t::Special5)
            .def_readwrite("Section", &NPC_t::Section)
            .def_readwrite("Wet", &NPC_t::Wet)
            .def_readwrite("Direction", &NPC_t::Direction)
            .def_readwrite("Damage", &NPC_t::Damage)
            .def_readwrite("Immune", &NPC_t::Immune)
            .def_readwrite("CantHurt", &NPC_t::CantHurt)
            .def_readwrite("Multiplier", &NPC_t::Multiplier)
            .def_readwrite("TailCD", &NPC_t::TailCD)
            .def_readwrite("Projectile", &NPC_t::Projectile)
            .def_readwrite("Variant", &NPC_t::Variant)
            .def_readwrite("BattleOwner", &NPC_t::BattleOwner)
            .def_readwrite("HoldingPlayer", &NPC_t::HoldingPlayer)
            .def_readwrite("CantHurtPlayer", &NPC_t::CantHurtPlayer)
            .def_readwrite("RealSpeedX", &NPC_t::RealSpeedX)
            .def_readwrite("BeltSpeed", &NPC_t::BeltSpeed)
            .def_readwrite("oldAddBelt", &NPC_t::oldAddBelt)
            .def_readwrite("Effect", &NPC_t::Effect)
            .def_readwrite("Effect2", &NPC_t::Effect2)
            .def_readwrite("Effect3", &NPC_t::Effect3)
            .def_readwrite("TimeLeft", &NPC_t::TimeLeft)
            .def_readwrite("JustActivated", &NPC_t::JustActivated)
            .def_readwrite("Slope", &NPC_t::Slope)
            .def_readwrite("vehiclePlr", &NPC_t::vehiclePlr)
            .def_readwrite("vehicleYOffset", &NPC_t::vehicleYOffset)
            .def_readwrite("WallDeath", &NPC_t::WallDeath)
            .def_readwrite("GFXSlot", &NPC_t::GFXSlot)
            .def_readwrite("Wings", &NPC_t::Wings)
            .def_readwrite("FrameCount", &NPC_t::FrameCount)
            // --- Newly added NPC fields (P0) ---
            .def_readwrite("DefaultLocationX", &NPC_t::DefaultLocationX)
            .def_readwrite("DefaultLocationY", &NPC_t::DefaultLocationY)
            .def_readwrite("DefaultType", &NPC_t::DefaultType)
            .def_readwrite("DefaultSpecial", &NPC_t::DefaultSpecial)
            .def_readwrite("DefaultDirection", &NPC_t::DefaultDirection)
            .def_readwrite("DefaultWings", &NPC_t::DefaultWings)
            // Event/string indices (mapped as integers)
            .def_readwrite("TriggerActivate", &NPC_t::TriggerActivate)
            .def_readwrite("TriggerDeath", &NPC_t::TriggerDeath)
            .def_readwrite("TriggerTalk", &NPC_t::TriggerTalk)
            .def_readwrite("TriggerLast", &NPC_t::TriggerLast)
            .def_readwrite("Text", &NPC_t::Text)
            .def_readwrite("Layer", &NPC_t::Layer)
            .def_readwrite("AttLayer", &NPC_t::AttLayer)
            .def_readwrite("extx", &NPC_t::extx)
            .def_readwrite("exty", &NPC_t::exty)
            // Bitfield members via getter/setter
            .def("getGenerator", &LuaNPCExt::npc_getGenerator)
            .def("setGenerator", &LuaNPCExt::npc_setGenerator)
            .def("getGeneratorActive", &LuaNPCExt::npc_getGeneratorActive)
            .def("setGeneratorActive", &LuaNPCExt::npc_setGeneratorActive)
            .def("getChat", &LuaNPCExt::npc_getChat)
            .def("setChat", &LuaNPCExt::npc_setChat)
            .def("getLegacy", &LuaNPCExt::npc_getLegacy)
            .def("setLegacy", &LuaNPCExt::npc_setLegacy)
            .def("getTurnAround", &LuaNPCExt::npc_getTurnAround)
            .def("setTurnAround", &LuaNPCExt::npc_setTurnAround)
            .def("getTurnBackWipe", &LuaNPCExt::npc_getTurnBackWipe)
            .def("setTurnBackWipe", &LuaNPCExt::npc_setTurnBackWipe)
            .def("getPlayerTemp", &LuaNPCExt::npc_getplayerTemp)
            .def("setPlayerTemp", &LuaNPCExt::npc_setplayerTemp)
            .def("getNoLavaSplash", &LuaNPCExt::npc_getNoLavaSplash)
            .def("setNoLavaSplash", &LuaNPCExt::npc_setNoLavaSplash)
            .def("getBouce", &LuaNPCExt::npc_getBouce)
            .def("setBouce", &LuaNPCExt::npc_setBouce)
            .def("getDefaultStuck", &LuaNPCExt::npc_getDefaultStuck)
            .def("setDefaultStuck", &LuaNPCExt::npc_setDefaultStuck)
            .def("getRespawnDelay", &LuaNPCExt::npc_getRespawnDelay)
            .def("setRespawnDelay", &LuaNPCExt::npc_setRespawnDelay)
            .def("getStuck", &LuaNPCExt::npc_getStuck)
            .def("setStuck", &LuaNPCExt::npc_setStuck)
            .def("getShadow", &LuaNPCExt::npc_getShadow)
            .def("setShadow", &LuaNPCExt::npc_setShadow)
            .def("getQuicksand", &LuaNPCExt::npc_getQuicksand)
            .def("setQuicksand", &LuaNPCExt::npc_setQuicksand)
            // Generator sub-fields
            .def("getGeneratorDirection", &LuaNPCExt::npc_getGeneratorDirection)
            .def("getGeneratorEffect", &LuaNPCExt::npc_getGeneratorEffect)
            .def("getGeneratorTimeMax", &LuaNPCExt::npc_getGeneratorTimeMax)
            .def("setGeneratorTimeMax", &LuaNPCExt::npc_setGeneratorTimeMax)
            .def("getGeneratorTime", &LuaNPCExt::npc_getGeneratorTime)
            .def("setGeneratorTime", &LuaNPCExt::npc_setGeneratorTime)
            .def("getPermID", &LuaNPCExt::npc_getPermID)
            .def("getName", &LuaNPCExt::npc_getName)
            .def("setName", &LuaNPCExt::npc_setName)
            .def("luaX", +[](NPC_t *n) -> lua_Integer { return (lua_Integer)(n->Location.X.i >> 32); })
            .def("luaY", +[](NPC_t *n) -> lua_Integer { return (lua_Integer)(n->Location.Y.i >> 32); }),

        // ================================================================
        // Player_t class binding (read/write access to all fields)
        // ================================================================
        class_<Player_t>("Player")
            .def_readwrite("DoubleJump", &Player_t::DoubleJump)
            .def_readwrite("FlySparks", &Player_t::FlySparks)
            .def_readwrite("Driving", &Player_t::Driving)
            .def_readwrite("Quicksand", &Player_t::Quicksand)
            .def_readwrite("Bombs", &Player_t::Bombs)
            .def_readwrite("Slippy", &Player_t::Slippy)
            .def_readwrite("Fairy", &Player_t::Fairy)
            .def_readwrite("FairyCD", &Player_t::FairyCD)
            .def_readwrite("FairyTime", &Player_t::FairyTime)
            .def_readwrite("HasKey", &Player_t::HasKey)
            .def_readwrite("Hearts", &Player_t::Hearts)
            .def_readwrite("CanFloat", &Player_t::CanFloat)
            .def_readwrite("FloatRelease", &Player_t::FloatRelease)
            .def_readwrite("FloatTime", &Player_t::FloatTime)
            .def_readwrite("FloatSpeed", &Player_t::FloatSpeed)
            .def_readwrite("FloatDir", &Player_t::FloatDir)
            .def_readwrite("SwordPoke", &Player_t::SwordPoke)
            .def_readwrite("GrabTime", &Player_t::GrabTime)
            .def_readwrite("GrabSpeed", &Player_t::GrabSpeed)
            .def_readwrite("VineNPC", &Player_t::VineNPC)
            .def_readwrite("VineBGO", &Player_t::VineBGO)
            .def_readwrite("Wet", &Player_t::Wet)
            .def_readwrite("WetFrame", &Player_t::WetFrame)
            .def_readwrite("SwimCount", &Player_t::SwimCount)
            .def_readwrite("NoGravity", &Player_t::NoGravity)
            .def_readwrite("Slide", &Player_t::Slide)
            .def_readwrite("SlideKill", &Player_t::SlideKill)
            .def_readwrite("Vine", &Player_t::Vine)
            .def_readwrite("ShellSurf", &Player_t::ShellSurf)
            .def_readwrite("Rolling", &Player_t::Rolling)
            .def_readwrite("StateNPC", &Player_t::StateNPC)
            .def_readwrite("Slope", &Player_t::Slope)
            .def_readwrite("Stoned", &Player_t::Stoned)
            .def_readwrite("AquaticSwim", &Player_t::AquaticSwim)
            .def_readwrite("StonedCD", &Player_t::StonedCD)
            .def_readwrite("StonedTime", &Player_t::StonedTime)
            .def_readwrite("SpinJump", &Player_t::SpinJump)
            .def_readwrite("SpinFrame", &Player_t::SpinFrame)
            .def_readwrite("SpinFireDir", &Player_t::SpinFireDir)
            .def_readwrite("Multiplier", &Player_t::Multiplier)
            .def_readwrite("SlideCounter", &Player_t::SlideCounter)
            .def_readwrite("ShowWarp", &Player_t::ShowWarp)
            .def_readwrite("ForceHold", &Player_t::ForceHold)
            .def_readwrite("YoshiYellow", &Player_t::YoshiYellow)
            .def_readwrite("YoshiBlue", &Player_t::YoshiBlue)
            .def_readwrite("YoshiRed", &Player_t::YoshiRed)
            .def_readwrite("YoshiWingsFrame", &Player_t::YoshiWingsFrame)
            .def_readwrite("YoshiWingsFrameCount", &Player_t::YoshiWingsFrameCount)
            .def_readwrite("YoshiTX", &Player_t::YoshiTX)
            .def_readwrite("YoshiTY", &Player_t::YoshiTY)
            .def_readwrite("YoshiTFrame", &Player_t::YoshiTFrame)
            .def_readwrite("YoshiTFrameCount", &Player_t::YoshiTFrameCount)
            .def_readwrite("YoshiBX", &Player_t::YoshiBX)
            .def_readwrite("YoshiBY", &Player_t::YoshiBY)
            .def_readwrite("YoshiBFrame", &Player_t::YoshiBFrame)
            .def_readwrite("YoshiBFrameCount", &Player_t::YoshiBFrameCount)
            .def_readwrite("YoshiTongue", &Player_t::YoshiTongue)
            .def_readwrite("YoshiTonugeBool", &Player_t::YoshiTonugeBool)
            .def_readwrite("YoshiTongueLength", &Player_t::YoshiTongueLength)
            .def_readwrite("YoshiNPC", &Player_t::YoshiNPC)
            .def_readwrite("YoshiPlayer", &Player_t::YoshiPlayer)
            .def_readwrite("Dismount", &Player_t::Dismount)
            .def_readwrite("Location", &Player_t::Location)
            .def_readwrite("Character", &Player_t::Character)
            .def_readwrite("Direction", &Player_t::Direction)
            .def_readwrite("Mount", &Player_t::Mount)
            .def_readwrite("MountType", &Player_t::MountType)
            .def_readwrite("MountSpecial", &Player_t::MountSpecial)
            .def_readwrite("MountOffsetY", &Player_t::MountOffsetY)
            .def_readwrite("MountFrame", &Player_t::MountFrame)
            .def_readwrite("State", &Player_t::State)
            .def_readwrite("Frame", &Player_t::Frame)
            .def_readwrite("FrameCount", &Player_t::FrameCount)
            .def_readwrite("Jump", &Player_t::Jump)
            .def_readwrite("CanJump", &Player_t::CanJump)
            .def_readwrite("CanAltJump", &Player_t::CanAltJump)
            .def_readwrite("Effect", &Player_t::Effect)
            .def_readwrite("Effect2", &Player_t::Effect2)
            .def_readwrite("RespawnY", &Player_t::RespawnY)
            .def_readwrite("Duck", &Player_t::Duck)
            .def_readwrite("DropRelease", &Player_t::DropRelease)
            .def_readwrite("StandUp", &Player_t::StandUp)
            .def_readwrite("StandUp2", &Player_t::StandUp2)
            .def_readwrite("Bumped", &Player_t::Bumped)
            // --- Newly added Player_t fields (high priority) ---
            .def_readwrite("Bumped2", &Player_t::Bumped2)
            .def_readwrite("Dead", &Player_t::Dead)
            .def_readwrite("TimeToLive", &Player_t::TimeToLive)
            .def_readwrite("Immune", &Player_t::Immune)
            .def_readwrite("Immune2", &Player_t::Immune2)
            .def_readwrite("ForceHitSpot3", &Player_t::ForceHitSpot3)
            .def_readwrite("HoldingNPC", &Player_t::HoldingNPC)
            .def_readwrite("CanGrabNPCs", &Player_t::CanGrabNPCs)
            .def_readwrite("HeldBonus", &Player_t::HeldBonus)
            .def_readwrite("Section", &Player_t::Section)
            .def_readwrite("WarpCD", &Player_t::WarpCD)
            .def_readwrite("Warp", &Player_t::Warp)
            .def_readwrite("WarpBackward", &Player_t::WarpBackward)
            .def_readwrite("WarpShooted", &Player_t::WarpShooted)
            .def_readwrite("FireBallCD", &Player_t::FireBallCD)
            .def_readwrite("FireBallCD2", &Player_t::FireBallCD2)
            .def_readwrite("TailCount", &Player_t::TailCount)
            .def_readwrite("RunCount", &Player_t::RunCount)
            .def_readwrite("CanFly", &Player_t::CanFly)
            .def_readwrite("CanFly2", &Player_t::CanFly2)
            .def_readwrite("FlyCount", &Player_t::FlyCount)
            .def_readwrite("RunRelease", &Player_t::RunRelease)
            .def_readwrite("JumpRelease", &Player_t::JumpRelease)
            .def_readwrite("StandingOnNPC", &Player_t::StandingOnNPC)
            .def_readwrite("StandingOnVehiclePlr", &Player_t::StandingOnVehiclePlr)
            .def_readwrite("UnStart", &Player_t::UnStart)
            .def_readwrite("mountBump", &Player_t::mountBump)
            .def_readwrite("CurMazeZone", &Player_t::CurMazeZone)
            .def_readwrite("MazeZoneStatus", &Player_t::MazeZoneStatus)
            // Bitfield members via getter/setter
            .def("getGroundPound", &LuaPlayerExt::player_getGroundPound)
            .def("setGroundPound", &LuaPlayerExt::player_setGroundPound)
            .def("getGroundPound2", &LuaPlayerExt::player_getGroundPound2)
            .def("setGroundPound2", &LuaPlayerExt::player_setGroundPound2)
            .def("getCanPound", &LuaPlayerExt::player_getCanPound)
            .def("setCanPound", &LuaPlayerExt::player_setCanPound)
            .def("getAltRunRelease", &LuaPlayerExt::player_getAltRunRelease)
            .def("setAltRunRelease", &LuaPlayerExt::player_setAltRunRelease)
            .def("getDuckRelease", &LuaPlayerExt::player_getDuckRelease)
            .def("setDuckRelease", &LuaPlayerExt::player_setDuckRelease)
            .def("getSlippyWall", &LuaPlayerExt::player_getSlippyWall)
            .def("setSlippyWall", &LuaPlayerExt::player_setSlippyWall)
            .def("getJumpOffWall", &LuaPlayerExt::player_getJumpOffWall)
            .def("setJumpOffWall", &LuaPlayerExt::player_setJumpOffWall)
            .def("luaX", +[](Player_t *p) -> lua_Integer { return (lua_Integer)(p->Location.X.i >> 32); })
            .def("luaY", +[](Player_t *p) -> lua_Integer { return (lua_Integer)(p->Location.Y.i >> 32); }),

        // ================================================================
        // Layer_t class binding
        // ================================================================
        class_<Layer_t>("Layer")
            .def_readonly("Name", &Layer_t::Name)
            .def_readwrite("SpeedX", &Layer_t::SpeedX)
            .def_readwrite("SpeedY", &Layer_t::SpeedY)
            .def_readwrite("Hidden", &Layer_t::Hidden)
            .def_readwrite("EffectStop", &Layer_t::EffectStop),

        // ================================================================
        // Block_t class binding
        // ================================================================
        class_<Block_t>("Block")
            .def_readwrite("Location", &Block_t::Location)
            .def_readwrite("Type", &Block_t::Type)
            .def_readwrite("Special", &Block_t::Special)
            .def_readwrite("Invis", &Block_t::Invis)
            .def_readwrite("Hidden", &Block_t::Hidden)
            .def_readwrite("Slippy", &Block_t::Slippy)
            .def_readwrite("Kill", &Block_t::Kill)
            .def_readwrite("RapidHit", &Block_t::RapidHit)
            // --- Newly added Block fields (P0) ---
            .def_readwrite("forceSmashable", &Block_t::forceSmashable)
            .def_readwrite("RespawnDelay_ScreensLeft", &Block_t::RespawnDelay_ScreensLeft)
            .def_readwrite("DefaultType", &Block_t::DefaultType)
            .def_readwrite("DefaultSpecial", &Block_t::DefaultSpecial)
            .def_readwrite("TriggerHit", &Block_t::TriggerHit)
            .def_readwrite("TriggerDeath", &Block_t::TriggerDeath)
            .def_readwrite("TriggerLast", &Block_t::TriggerLast)
            .def_readwrite("Layer", &Block_t::Layer)
            .def_readwrite("ShakeCounter", &Block_t::ShakeCounter)
            .def_readwrite("ShakeOffset", &Block_t::ShakeOffset)
            .def_readwrite("coinSwitchNpcType", &Block_t::coinSwitchNpcType)
            .def_readwrite("tempBlockVehiclePlr", &Block_t::tempBlockVehiclePlr)
            .def_readwrite("tempBlockNpcType", &Block_t::tempBlockNpcType)
            .def_readwrite("tempBlockVehicleYOffset", &Block_t::tempBlockVehicleYOffset)
            .def_readwrite("tempBlockNpcIdx", &Block_t::tempBlockNpcIdx)
            .def_readwrite("extx", &Block_t::extx)
            .def_readwrite("exty", &Block_t::exty)
            .def("getPermID", &LuaBlock::block_getPermID)
            .def("getName", &LuaBlock::block_getName)
            .def("setName", &LuaBlock::block_setName)
            .def("luaX", +[](Block_t *b) -> lua_Integer { return (lua_Integer)(b->Location.X.i >> 32); })
            .def("luaY", +[](Block_t *b) -> lua_Integer { return (lua_Integer)(b->Location.Y.i >> 32); }),

        // ================================================================
        // SpeedlessLocation_t struct binding
        // (used by Background_t, Water_t, Warp_t, Sections)
        // ================================================================
        class_<SpeedlessLocation_t>("SpeedlessLocation")
            .def(constructor<>())
            .def_readwrite("X", &SpeedlessLocation_t::X)
            .def_readwrite("Y", &SpeedlessLocation_t::Y)
            .def_readwrite("Width", &SpeedlessLocation_t::Width)
            .def_readwrite("Height", &SpeedlessLocation_t::Height)
            .def("luaX", &SpeedlessLocation_t::luaX)
            .def("luaY", &SpeedlessLocation_t::luaY),

        // ================================================================
        // Background_t (BGO) class binding
        // ================================================================
        class_<Background_t>("BGO")
            .def_readwrite("Type", &Background_t::Type)
            .def_readwrite("Hidden", &Background_t::Hidden)
            .def_readwrite("Layer", &Background_t::Layer)
            .def_readwrite("SortPriority", &Background_t::SortPriority)
            .def_readwrite("Location", &Background_t::Location)
            .def_readwrite("extx", &Background_t::extx)
            .def_readwrite("exty", &Background_t::exty)
            .def("getPermID", &LuaBGO::bgo_getPermID),

        // ================================================================
        // Water_t (Liquid) class binding
        // ================================================================
        class_<Water_t>("Liquid")
            .def_readwrite("Location", &Water_t::Location)
            .def_readwrite("Type", &Water_t::Type)
            .def_readwrite("Hidden", &Water_t::Hidden)
            .def_readwrite("Layer", &Water_t::Layer)
            .def("getPermID", &LuaWater::water_getPermID)
            .def("getName", &LuaWater::water_getName)
            .def("setName", &LuaWater::water_setName),

        // ================================================================
        // Warp_t class binding
        // ================================================================
        class_<Warp_t>("Warp")
            .def_readwrite("Entrance", &Warp_t::Entrance)
            .def_readwrite("Exit", &Warp_t::Exit)
            .def_readwrite("Locked", &Warp_t::Locked)
            .def_readwrite("WarpNPC", &Warp_t::WarpNPC)
            .def_readwrite("NoYoshi", &Warp_t::NoYoshi)
            .def_readwrite("Hidden", &Warp_t::Hidden)
            .def_readwrite("Stars", &Warp_t::Stars)
            .def_readwrite("Effect", &Warp_t::Effect)
            .def_readwrite("LevelWarp", &Warp_t::LevelWarp)
            .def_readwrite("LevelEnt", &Warp_t::LevelEnt)
            .def_readwrite("Direction", &Warp_t::Direction)
            .def_readwrite("Direction2", &Warp_t::Direction2)
            .def_readwrite("MapWarp", &Warp_t::MapWarp)
            .def_readwrite("MapX", &Warp_t::MapX)
            .def_readwrite("MapY", &Warp_t::MapY)
            .def_readwrite("curStars", &Warp_t::curStars)
            .def_readwrite("twoWay", &Warp_t::twoWay)
            .def_readwrite("noPrintStars", &Warp_t::noPrintStars)
            .def_readwrite("noEntranceScene", &Warp_t::noEntranceScene)
            .def_readwrite("cannonExit", &Warp_t::cannonExit)
            .def_readwrite("cannonExitSpeed", &Warp_t::cannonExitSpeed)
            .def_readwrite("stoodRequired", &Warp_t::stoodRequired)
            .def_readwrite("eventEnter", &Warp_t::eventEnter)
            .def_readwrite("eventExit", &Warp_t::eventExit)
            .def_readwrite("StarsMsg", &Warp_t::StarsMsg)
            .def_readwrite("transitEffect", &Warp_t::transitEffect)
            .def_readwrite("Layer", &Warp_t::Layer)
            .def("getPermID", &LuaWarp::warp_getPermID),

        // ================================================================
        // Effect_t class binding (P3 - FXCreate equivalent)
        // ================================================================
        class_<Effect_t>("Effect")
            .def_readwrite("Location", &Effect_t::Location)
            .def_readwrite("Type", &Effect_t::Type)
            .def_readwrite("Frame", &Effect_t::Frame)
            .def_readwrite("FrameCount", &Effect_t::FrameCount)
            .def_readwrite("Life", &Effect_t::Life)
            .def_readwrite("NewNpc", &Effect_t::NewNpc)
            .def_readwrite("NewNpcSpecial", &Effect_t::NewNpcSpecial)
            .def_readwrite("Shadow", &Effect_t::Shadow),

        // ================================================================
        // Player API functions
        // ================================================================
        def("xtech_player_get", &LuaPlayer::get),
        def("xtech_player_count", &LuaPlayer::count),
        def("xtech_player_filterToBig", &LuaPlayer::filterToBig),
        def("xtech_player_filterToSmall", &LuaPlayer::filterToSmall),
        def("xtech_player_filterToFire", &LuaPlayer::filterToFire),
        def("xtech_player_filterMount", &LuaPlayer::filterMount),
        def("xtech_player_filterReservePowerup", &LuaPlayer::filterReservePowerup),
        def("xtech_player_cycleRight", &LuaPlayer::cycleRight),
        def("xtech_player_cycleLeft", &LuaPlayer::cycleLeft),
        def("xtech_player_infiniteFlying", &LuaPlayer::infiniteFlying),
        def("xtech_player_pressingUp", &LuaPlayer::pressingUp),
        def("xtech_player_pressingDown", &LuaPlayer::pressingDown),
        def("xtech_player_pressingLeft", &LuaPlayer::pressingLeft),
        def("xtech_player_pressingRight", &LuaPlayer::pressingRight),
        def("xtech_player_pressingJump", &LuaPlayer::pressingJump),
        def("xtech_player_pressingRun", &LuaPlayer::pressingRun),
        def("xtech_player_pressingSEL", &LuaPlayer::pressingSEL),
        def("xtech_player_isHoldingSpriteType", &LuaPlayer::isHoldingSpriteType),
        def("xtech_player_usesHearts", &LuaPlayer::usesHearts),
        def("xtech_player_memSet", &LuaPlayer::memSet),
        def("xtech_player_dismount", &LuaPlayer::dismount),

        // ================================================================
        // NPC API functions
        // ================================================================
        def("xtech_npc_get", &LuaNPC::get),
        def("xtech_npc_count", &LuaNPC::count),
        def("xtech_npc_getFirstMatch", &LuaNPC::getFirstMatch),
        def("xtech_npc_memSet", &LuaNPC::memSet),
        def("xtech_npc_allSetHits", &LuaNPC::allSetHits),
        def("xtech_npc_kill", &LuaNPC::kill),
        def("xtech_npc_hurt", &LuaNPC::hurt),
        def("xtech_npc_getByPermID", &LuaNPC::getByPermID),
        def("xtech_npc_getByName", &LuaNPC::npc_getByName),
        def("xtech_npc_forEach", &LuaNPC::forEach),
        def("xtech_npc_create", &LuaNPC::lua_npc_create),

        // ================================================================
        // Layer API functions
        // ================================================================
        def("xtech_layer_get", &LuaLayer::get),
        def("xtech_layer_count", &LuaLayer::count),
        def("xtech_layer_setXSpeed", &LuaLayer::setXSpeed),
        def("xtech_layer_setYSpeed", &LuaLayer::setYSpeed),
        def("xtech_layer_stop", &LuaLayer::stop),

        // ================================================================
        // Block API functions
        // ================================================================
        def("xtech_block_get", &LuaBlock::get),
        def("xtech_block_count", &LuaBlock::count),
        def("xtech_block_setAll", &LuaBlock::setAll),
        def("xtech_block_swapAll", &LuaBlock::swapAll),
        def("xtech_block_showAll", &LuaBlock::showAll),
        def("xtech_block_hideAll", &LuaBlock::hideAll),
        def("xtech_block_isPlayerTouchingType", &LuaBlock::isPlayerTouchingType),
        def("xtech_block_getByPermID", &LuaBlock::getByPermID_wrapper),
        def("xtech_block_getByName", &LuaBlock::block_getByName),
        def("xtech_block_forEach", &LuaBlock::forEach),

        // ================================================================
        // BGO API functions
        // ================================================================
        def("xtech_bgo_get", &LuaBGO::get),
        def("xtech_bgo_count", &LuaBGO::count),
        def("xtech_bgo_getByPermID", &LuaBGO::getByPermID),
        def("xtech_bgo_forEach", &LuaBGO::forEach),

        // ================================================================
        // Liquid / Water API functions
        // ================================================================
        def("xtech_liquid_get", &LuaWater::get),
        def("xtech_liquid_count", &LuaWater::count),
        def("xtech_liquid_getByPermID", &LuaWater::getByPermID),
        def("xtech_liquid_getByName", &LuaWater::water_getByName),

        // ================================================================
        // Warp API functions
        // ================================================================
        def("xtech_warp_get", &LuaWarp::get),
        def("xtech_warp_count", &LuaWarp::count),
        def("xtech_warp_getByPermID", &LuaWarp::getByPermID),

        // ================================================================
        // Section API functions
        // ================================================================
        def("xtech_section_get", &LuaSection::get),
        def("xtech_section_count", &LuaSection::count),
        def("xtech_section_getBackground", &LuaSection::getBackground),
        def("xtech_section_setBackground", &LuaSection::setBackground),
        def("xtech_section_getMusic", &LuaSection::getMusic),
        def("xtech_section_setMusic", &LuaSection::setMusic),
        def("xtech_section_getMusicFile", &LuaSection::getMusicFile),
        def("xtech_section_setMusicFile", &LuaSection::setMusicFile),
        def("xtech_section_getOffScreenExit", &LuaSection::getOffScreenExit),
        def("xtech_section_setOffScreenExit", &LuaSection::setOffScreenExit),

        // ================================================================
        // Effect API functions (P3)
        // ================================================================
        def("xtech_effect_get", &LuaEffect::get),
        def("xtech_effect_count", &LuaEffect::count),
        def("xtech_effect_create", &LuaEffect::create),
        def("xtech_effect_kill", &LuaEffect::kill),

        // ================================================================
        // Audio API functions
        // ================================================================
        def("xtech_audio_playSFX", &LuaAudio::playSFX),
        def("xtech_audio_playSFXExt", &LuaAudio::playSFXExt),
        def("xtech_audio_stopSFXExt", &LuaAudio::stopSFXExt),
        def("xtech_audio_preloadSFXExt", &LuaAudio::preloadSFXExt),
        def("xtech_audio_playMusic", &LuaAudio::playMusic),
        def("xtech_audio_playMusicFile", &LuaAudio::playMusicFile),
        def("xtech_audio_setMusic", &LuaAudio::setMusic),

        // ================================================================
        // HUD / Render API functions
        // ================================================================
        def("xtech_hud_showText", &LuaHUD::showText),
        def("xtech_hud_showLevelName", &LuaHUD::showLevelName),
        def("xtech_hud_showLevelFile", &LuaHUD::showLevelFile),
        def("xtech_hud_debugPrint", &LuaHUD::debugPrint),
        def("xtech_hud_showImage", &LuaHUD::showImage),
        def("xtech_hud_showNPC", &LuaHUD::showNPC),

        // ================================================================
        // Variable API functions
        // ================================================================
        def("xtech_var_get", &LuaVar::getVar),
        def("xtech_var_exists", &LuaVar::varExists),
        def("xtech_var_operation", &LuaVar::varOperation),

        // ================================================================
        // Event API functions
        // ================================================================
        def("xtech_event_trigger", &LuaEvent::trigger),
        def("xtech_event_triggerByName", &LuaEvent::triggerByName),
        def("xtech_event_cancelByName", &LuaEvent::cancelByName),

        // ================================================================
        // Misc API functions
        // ================================================================
        def("xtech_misc_getFrame", &LuaMisc::getFrame),
        def("xtech_misc_getSection", &LuaMisc::getSection),
        def("xtech_misc_showMsg", &LuaMisc::showMsg),
        def("xtech_misc_showMsgInfo", &LuaMisc::showMsgInfo),
        def("xtech_misc_showMsgWarn", &LuaMisc::showMsgWarn),
        def("xtech_misc_cheat", &LuaMisc::cheat),
        def("xtech_misc_log", &LuaMisc::logMsg),
        def("xtech_misc_logWarn", &LuaMisc::logWarn),
        def("xtech_misc_logDebug", &LuaMisc::logDebug),
        // xtech_misc_wait: registered manually (luabind::object param)
        // P3: Sysval functions
        def("xtech_sysval_getLives", &LuaMisc::sysval_getLives),
        def("xtech_sysval_setLives", &LuaMisc::sysval_setLives),
        def("xtech_sysval_getCoins", &LuaMisc::sysval_getCoins),
        def("xtech_sysval_setCoins", &LuaMisc::sysval_setCoins),
        def("xtech_sysval_getScore", &LuaMisc::sysval_getScore),
        def("xtech_sysval_setScore", &LuaMisc::sysval_setScore),
        def("xtech_sysval_getScreenX", &LuaMisc::sysval_getScreenX),
        def("xtech_sysval_getScreenY", &LuaMisc::sysval_getScreenY),
        def("xtech_sysval_getScreenWidth", &LuaMisc::sysval_getScreenWidth),
        def("xtech_sysval_getScreenHeight", &LuaMisc::sysval_getScreenHeight),
        def("xtech_sysval_getScreenTop", &LuaMisc::sysval_getScreenTop),
        def("xtech_sysval_getScreenCenterX", &LuaMisc::sysval_getScreenCenterX),
        def("xtech_sysval_getShowHud", &LuaMisc::sysval_getShowHud),
        def("xtech_sysval_setShowHud", &LuaMisc::sysval_setShowHud),
        def("xtech_sysval_getShowInterface", &LuaMisc::sysval_getShowInterface),
        def("xtech_sysval_setShowInterface", &LuaMisc::sysval_setShowInterface),
        def("xtech_sysval_getBattleMode", &LuaMisc::sysval_getBattleMode),
        def("xtech_sysval_getEndLevel", &LuaMisc::sysval_getEndLevel),
        def("xtech_sysval_setEndLevel", &LuaMisc::sysval_setEndLevel),
        def("xtech_sysval_getLevelMacro", &LuaMisc::sysval_getLevelMacro),
        def("xtech_sysval_setLevelMacro", &LuaMisc::sysval_setLevelMacro),
        def("xtech_sysval_getLevelMacroCounter", &LuaMisc::sysval_getLevelMacroCounter),
        def("xtech_sysval_getLevelBeatCode", &LuaMisc::sysval_getLevelBeatCode),
        def("xtech_sysval_setLevelBeatCode", &LuaMisc::sysval_setLevelBeatCode),
        def("xtech_sysval_getGameTime", &LuaMisc::sysval_getGameTime),
        def("xtech_sysval_getCheckpointCount", &LuaMisc::sysval_getCheckpointCount),
        def("xtech_sysval_getCheckpointId", &LuaMisc::sysval_getCheckpointId),
        // P3: Named timer functions
        // xtech_timer_create: registered manually (luabind::object param)
        def("xtech_timer_cancel", &LuaMisc::lua_timer_cancel),
        def("xtech_timer_clearAll", &LuaMisc::lua_timer_clearAll),
        def("xtech_sysval_getLevelName", &LuaMisc::sysval_getLevelName),
        def("xtech_getStrWidth", (int(*)(const std::string&, double))&LuaMisc::misc_getStrWidth),
        def("xtech_getStrWidth", (int(*)(const std::string&))&LuaMisc::misc_getStrWidth),

        // ================================================================
        // Sprite API functions
        // ================================================================
        def("xtech_sprite_loadImage", &LuaSprite::loadImage),
        def("xtech_sprite_loadImageSimple", &LuaSprite::loadImageSimple),
        def("xtech_sprite_getImage", &LuaSprite::getImage),
        def("xtech_sprite_deleteImage", &LuaSprite::deleteImage),
        def("xtech_sprite_place", &LuaSprite::placeSprite),
        def("xtech_sprite_placeExt", &LuaSprite::placeSpriteExt),
        def("xtech_sprite_count", &LuaSprite::spriteCount),
        def("xtech_sprite_clearAll", &LuaSprite::clearAllSprites),
        def("xtech_sprite_clearByCode", &LuaSprite::clearSpritesByCode),
        def("xtech_sprite_addBlueprint", &LuaSprite::addBlueprint),
        def("xtech_sprite_copyFromBlueprint", &LuaSprite::copyFromBlueprint)

        // Constants registered via raw Lua C API below (GCC 12 compat)
    ];

    // ================================================================
    // Manual registrations for functions with luabind::object params
    // (luabind's def() + GCC 12 have template issues with these)
    // ================================================================
    lua_pushcfunction(L, LuaMisc::lua_misc_wait_raw);
    lua_setglobal(L, "xtech_misc_wait");

    lua_pushcfunction(L, LuaMisc::lua_timer_create_raw);
    lua_setglobal(L, "xtech_timer_create");

    lua_pushcfunction(L, LuaMisc::lua_timer_createEvent_raw);
    lua_setglobal(L, "xtech_timer_createEvent");

    // P4: Event subscription
    lua_pushcfunction(L, xtech_lua_subscribe_event);
    lua_setglobal(L, "xtech_event_subscribe");

    // ================================================================
    // Manual constant registrations: luabind's def() with integer
    // constants triggers call_types<int> which is broken on GCC 12.
    // Use raw Lua C API instead.
    // ================================================================
#define LUA_INT_CONST(name, val) do { lua_pushinteger(L, (int)(val)); lua_setglobal(L, name); } while(0)

    LUA_INT_CONST("CONST_DIR_UP", DIR_UP);
    LUA_INT_CONST("CONST_DIR_RIGHT", DIR_RIGHT);
    LUA_INT_CONST("CONST_DIR_DOWN", DIR_DOWN);
    LUA_INT_CONST("CONST_DIR_LEFT", DIR_LEFT);

    LUA_INT_CONST("CONST_OP_ASSIGN", 0);
    LUA_INT_CONST("CONST_OP_ADD", 1);
    LUA_INT_CONST("CONST_OP_SUB", 2);
    LUA_INT_CONST("CONST_OP_MULT", 3);
    LUA_INT_CONST("CONST_OP_DIV", 4);
    LUA_INT_CONST("CONST_OP_XOR", 5);
    LUA_INT_CONST("CONST_OP_ABS", 6);

    LUA_INT_CONST("CONST_CMPT_EQUALS", 0);
    LUA_INT_CONST("CONST_CMPT_GREATER", 1);
    LUA_INT_CONST("CONST_CMPT_LESS", 2);
    LUA_INT_CONST("CONST_CMPT_NOTEQ", 3);

    LUA_INT_CONST("CONST_PRI_LOW", 0);
    LUA_INT_CONST("CONST_PRI_MID", 1);
    LUA_INT_CONST("CONST_PRI_HIGH", 2);

    LUA_INT_CONST("CONST_PLAYER_SMALL", 1);
    LUA_INT_CONST("CONST_PLAYER_SUPER", 2);
    LUA_INT_CONST("CONST_PLAYER_FIRE", 3);
    LUA_INT_CONST("CONST_PLAYER_RACCOON", 4);
    LUA_INT_CONST("CONST_PLAYER_TANOOKI", 5);
    LUA_INT_CONST("CONST_PLAYER_HAMMER", 6);

    LUA_INT_CONST("CONST_MOUNT_NONE", 0);
    LUA_INT_CONST("CONST_MOUNT_BOOT", 1);
    LUA_INT_CONST("CONST_MOUNT_CLOWN_CAR", 2);
    LUA_INT_CONST("CONST_MOUNT_YOSHI", 3);

    LUA_INT_CONST("CONST_SPRITE_CUSTOM", 0);
    LUA_INT_CONST("CONST_SPRITE_STATIC", 1);
    LUA_INT_CONST("CONST_SPRITE_NORMAL", 2);
    LUA_INT_CONST("CONST_SPRITE_ITEM", 3);
    LUA_INT_CONST("CONST_SPRITE_BAR", 4);
    LUA_INT_CONST("CONST_SPRITE_PHANTO", 5);

    LUA_INT_CONST("CONST_BPAT_BEHAVIOR", 0);
    LUA_INT_CONST("CONST_BPAT_DRAW", 1);
    LUA_INT_CONST("CONST_BPAT_BIRTH", 2);
    LUA_INT_CONST("CONST_BPAT_DEATH", 3);

    // Level Macro types
    LUA_INT_CONST("CONST_LEVELMACRO_OFF", 0);
    LUA_INT_CONST("CONST_LEVELMACRO_CARD_ROULETTE", 1);
    LUA_INT_CONST("CONST_LEVELMACRO_QUESTION_SPHERE", 2);
    LUA_INT_CONST("CONST_LEVELMACRO_KEYHOLE", 3);
    LUA_INT_CONST("CONST_LEVELMACRO_CRYSTAL_BALL", 4);
    LUA_INT_CONST("CONST_LEVELMACRO_GAME_COMPLETE", 5);
    LUA_INT_CONST("CONST_LEVELMACRO_STAR", 6);
    LUA_INT_CONST("CONST_LEVELMACRO_GOAL_TAPE", 7);
    LUA_INT_CONST("CONST_LEVELMACRO_FLAG", 8);

    // Beat codes (world map exit paths)
    LUA_INT_CONST("CONST_BEATCODE_NONE", 0);
    LUA_INT_CONST("CONST_BEATCODE_CARD_ROULETTE", 1);
    LUA_INT_CONST("CONST_BEATCODE_QUESTION_SPHERE", 2);
    LUA_INT_CONST("CONST_BEATCODE_OFFSCREEN", 3);
    LUA_INT_CONST("CONST_BEATCODE_KEYHOLE", 4);
    LUA_INT_CONST("CONST_BEATCODE_CRYSTAL_BALL", 5);
    LUA_INT_CONST("CONST_BEATCODE_WARP", 6);
    LUA_INT_CONST("CONST_BEATCODE_STAR", 7);
    LUA_INT_CONST("CONST_BEATCODE_GOAL_TAPE", 8);
    LUA_INT_CONST("CONST_BEATCODE_FLAG", 9);
    LUA_INT_CONST("CONST_BEATCODE_ALT_FLAG", 10);
    LUA_INT_CONST("CONST_BEATCODE_RESERVED_1", 11);
    LUA_INT_CONST("CONST_BEATCODE_RESERVED_2", 12);
    LUA_INT_CONST("CONST_BEATCODE_RESERVED_3", 13);
    LUA_INT_CONST("CONST_BEATCODE_RESERVED_4", 14);
    LUA_INT_CONST("CONST_BEATCODE_GAME_COMPLETE", 15);

    LUA_INT_CONST("CONST_COLTYPE_NONE", 0);
    LUA_INT_CONST("CONST_COLTYPE_LEFT", 1);
    LUA_INT_CONST("CONST_COLTYPE_RIGHT", 2);
    LUA_INT_CONST("CONST_COLTYPE_TOP", 3);
    LUA_INT_CONST("CONST_COLTYPE_BOT", 4);

    LUA_INT_CONST("CONST_FT_BYTE", (int)FT_BYTE);
    LUA_INT_CONST("CONST_FT_WORD", (int)FT_WORD);
    LUA_INT_CONST("CONST_FT_DWORD", (int)FT_DWORD);
    LUA_INT_CONST("CONST_FT_FLOAT", (int)FT_FLOAT);
    LUA_INT_CONST("CONST_FT_DFLOAT", (int)FT_DFLOAT);

    // Physics constants (P6: exposed for Lua P-meter / speed-based logic)
    LUA_INT_CONST("CONST_PLAYER_RUN_SPEED", Physics.PlayerRunSpeed);

#undef LUA_INT_CONST

    // ================================================================
    // Full constant registrations via raw Lua C API
    // (bypasses luabind def() which is broken with GCC 12 for int/enum)
    // ================================================================
#define R_NPC(id) lua_pushinteger(L, (int)NPCID_##id); lua_setglobal(L, "CONST_NPC_" #id);
#define R_SFX(id) lua_pushinteger(L, (int)SFX_##id); lua_setglobal(L, "CONST_SFX_" #id);
#define R_EFF(id) lua_pushinteger(L, (int)EFFID_##id); lua_setglobal(L, "CONST_EFF_" #id);

    // --- NPC IDs (306 entries) ---
    R_NPC(NULL); R_NPC(FODDER_S3); R_NPC(RED_FODDER); R_NPC(RED_FLY_FODDER);
    R_NPC(GRN_TURTLE_S3); R_NPC(GRN_SHELL_S3); R_NPC(RED_TURTLE_S3); R_NPC(RED_SHELL_S3);
    R_NPC(PLANT_S3); R_NPC(POWER_S3); R_NPC(COIN_S3); R_NPC(ITEMGOAL);
    R_NPC(LAVABUBBLE); R_NPC(PLR_FIREBALL); R_NPC(FIRE_POWER_S3); R_NPC(MINIBOSS);
    R_NPC(GOALORB_S3); R_NPC(BULLET); R_NPC(BIG_BULLET); R_NPC(BLU_GUY);
    R_NPC(RED_GUY); R_NPC(CANNONENEMY); R_NPC(CANNONITEM); R_NPC(GLASS_TURTLE);
    R_NPC(GLASS_SHELL); R_NPC(JUMPER_S3); R_NPC(SPRING); R_NPC(UNDER_FODDER);
    R_NPC(RED_FISH_S1); R_NPC(HEAVY_THROWER); R_NPC(HEAVY_THROWN); R_NPC(KEY);
    R_NPC(COIN_SWITCH); R_NPC(COIN_S4); R_NPC(LEAF_POWER); R_NPC(GRN_BOOT);
    R_NPC(SPIKY_S3); R_NPC(STONE_S3); R_NPC(GHOST_S3); R_NPC(SPIT_BOSS);
    R_NPC(SPIT_BOSS_BALL); R_NPC(GOALORB_S2); R_NPC(GHOST_FAST); R_NPC(GHOST_S4);
    R_NPC(BIG_GHOST); R_NPC(SLIDE_BLOCK); R_NPC(FALL_BLOCK_RED); R_NPC(SPIKY_THROWER);
    R_NPC(SPIKY_BALL_S3); R_NPC(TOOTHYPIPE); R_NPC(TOOTHY); R_NPC(BOTTOM_PLANT);
    R_NPC(SIDE_PLANT); R_NPC(CRAB); R_NPC(FLY); R_NPC(EXT_TURTLE);
    R_NPC(VEHICLE); R_NPC(CONVEYOR); R_NPC(METALBARREL); R_NPC(YELSWITCH_FODDER);
    R_NPC(YEL_PLATFORM); R_NPC(BLUSWITCH_FODDER); R_NPC(BLU_PLATFORM); R_NPC(GRNSWITCH_FODDER);
    R_NPC(GRN_PLATFORM); R_NPC(REDSWITCH_FODDER); R_NPC(RED_PLATFORM); R_NPC(HPIPE_SHORT);
    R_NPC(HPIPE_LONG); R_NPC(VPIPE_SHORT); R_NPC(VPIPE_LONG); R_NPC(BIG_FODDER);
    R_NPC(BIG_TURTLE); R_NPC(BIG_SHELL); R_NPC(BIG_PLANT); R_NPC(CIVILIAN_SCARED);
    R_NPC(GRN_FLY_TURTLE_S3); R_NPC(JUMPER_S4); R_NPC(TANK_TREADS); R_NPC(SHORT_WOOD);
    R_NPC(LONG_WOOD); R_NPC(SLANT_WOOD_L); R_NPC(SLANT_WOOD_R); R_NPC(SLANT_WOOD_M);
    R_NPC(STATUE_S3); R_NPC(STATUE_FIRE); R_NPC(VILLAIN_S3); R_NPC(VILLAIN_FIRE);
    R_NPC(COIN_S1); R_NPC(FODDER_S1); R_NPC(LIFE_S3); R_NPC(ITEM_BURIED);
    R_NPC(VEGGIE_1); R_NPC(PLANT_S1); R_NPC(CIVILIAN); R_NPC(PET_GREEN);
    R_NPC(ITEM_POD); R_NPC(STAR_EXIT); R_NPC(PET_BLUE); R_NPC(PET_YELLOW);
    R_NPC(PET_RED); R_NPC(CHAR2); R_NPC(CHAR5); R_NPC(RED_COIN);
    R_NPC(PLATFORM_S3); R_NPC(CHECKER_PLATFORM); R_NPC(PLATFORM_S1); R_NPC(PINK_CIVILIAN);
    R_NPC(PET_FIRE); R_NPC(GRN_TURTLE_S4); R_NPC(RED_TURTLE_S4); R_NPC(BLU_TURTLE_S4);
    R_NPC(YEL_TURTLE_S4); R_NPC(GRN_SHELL_S4); R_NPC(RED_SHELL_S4); R_NPC(BLU_SHELL_S4);
    R_NPC(YEL_SHELL_S4); R_NPC(GRN_HIT_TURTLE_S4); R_NPC(RED_HIT_TURTLE_S4); R_NPC(BLU_HIT_TURTLE_S4);
    R_NPC(YEL_HIT_TURTLE_S4); R_NPC(GRN_FLY_TURTLE_S4); R_NPC(RED_FLY_TURTLE_S4); R_NPC(BLU_FLY_TURTLE_S4);
    R_NPC(YEL_FLY_TURTLE_S4); R_NPC(KNIGHT); R_NPC(BLU_SLIME); R_NPC(CYAN_SLIME);
    R_NPC(RED_SLIME); R_NPC(BIRD); R_NPC(RED_SPIT_GUY); R_NPC(BLU_SPIT_GUY);
    R_NPC(GRY_SPIT_GUY); R_NPC(SPIT_GUY_BALL); R_NPC(BOMB); R_NPC(WALK_BOMB_S2);
    R_NPC(WALK_BOMB_S3); R_NPC(LIT_BOMB_S3); R_NPC(COIN_S2); R_NPC(VEGGIE_2);
    R_NPC(VEGGIE_3); R_NPC(VEGGIE_4); R_NPC(VEGGIE_5); R_NPC(VEGGIE_6);
    R_NPC(VEGGIE_7); R_NPC(VEGGIE_8); R_NPC(VEGGIE_9); R_NPC(VEGGIE_RANDOM);
    R_NPC(PET_BLACK); R_NPC(PET_PURPLE); R_NPC(PET_PINK); R_NPC(SIGN);
    R_NPC(RING); R_NPC(POISON); R_NPC(CARRY_BLOCK_A); R_NPC(CARRY_BLOCK_B);
    R_NPC(CARRY_BLOCK_C); R_NPC(CARRY_BLOCK_D); R_NPC(CARRY_BUDDY); R_NPC(LIFT_SAND);
    R_NPC(ROCKET_WOOD); R_NPC(RED_FLY_TURTLE_S3); R_NPC(BRUTE); R_NPC(BRUTE_SQUISHED);
    R_NPC(BIG_GUY); R_NPC(CARRY_FODDER); R_NPC(HIT_CARRY_FODDER); R_NPC(FLY_CARRY_FODDER);
    R_NPC(CHASER); R_NPC(STATUE_POWER); R_NPC(HEAVY_POWER); R_NPC(PLR_HEAVY);
    R_NPC(GRN_SHELL_S1); R_NPC(GRN_TURTLE_S1); R_NPC(RED_SHELL_S1); R_NPC(RED_TURTLE_S1);
    R_NPC(GRN_FLY_TURTLE_S1); R_NPC(RED_FLY_TURTLE_S1); R_NPC(AXE); R_NPC(SAW);
    R_NPC(STONE_S4); R_NPC(STATUE_S4); R_NPC(FIRE_POWER_S1); R_NPC(FIRE_POWER_S4);
    R_NPC(POWER_S1); R_NPC(POWER_S4); R_NPC(LIFE_S1); R_NPC(LIFE_S4);
    R_NPC(3_LIFE); R_NPC(SKELETON); R_NPC(RAFT); R_NPC(RED_BOOT);
    R_NPC(CHECKPOINT); R_NPC(BLU_BOOT); R_NPC(RAINBOW_SHELL); R_NPC(FLIPPED_RAINBOW_SHELL);
    R_NPC(STAR_COLLECT); R_NPC(GOALTAPE); R_NPC(CHAR3); R_NPC(LAVA_MONSTER);
    R_NPC(VILLAIN_S1); R_NPC(SICK_BOSS); R_NPC(SICK_BOSS_BALL); R_NPC(FLIER);
    R_NPC(ROCKET_FLIER); R_NPC(WALL_BUG); R_NPC(WALL_SPARK); R_NPC(WALL_TURTLE);
    R_NPC(BOSS_CASE); R_NPC(BOSS_FRAGILE); R_NPC(HOMING_BALL); R_NPC(HOMING_BALL_GEN);
    R_NPC(FALL_BLOCK_BROWN); R_NPC(GRN_VINE_S3); R_NPC(RED_VINE_S3); R_NPC(GRN_VINE_S2);
    R_NPC(YEL_VINE_S2); R_NPC(BLU_VINE_S2); R_NPC(GRN_VINE_BASE_S2); R_NPC(YEL_VINE_BASE_S2);
    R_NPC(BLU_VINE_BASE_S2); R_NPC(LADDER); R_NPC(GRN_VINE_S1); R_NPC(GRN_VINE_TOP_S1);
    R_NPC(GRN_VINE_S4); R_NPC(RED_VINE_TOP_S3); R_NPC(GRN_VINE_TOP_S3); R_NPC(GRN_VINE_TOP_S4);
    R_NPC(PET_CYAN); R_NPC(GRN_FISH_S3); R_NPC(RED_FISH_S3); R_NPC(SQUID_S3);
    R_NPC(GRN_FISH_S4); R_NPC(GRN_FISH_S1); R_NPC(BONE_FISH); R_NPC(SQUID_S1);
    R_NPC(YEL_FISH_S4); R_NPC(ICE_BLOCK); R_NPC(TIME_SWITCH); R_NPC(TNT);
    R_NPC(TIMER_S2); R_NPC(EARTHQUAKE_BLOCK); R_NPC(FODDER_S5); R_NPC(FLY_FODDER_S5);
    R_NPC(FLY_FODDER_S3); R_NPC(FIRE_PLANT); R_NPC(PLANT_FIREBALL); R_NPC(STACKER);
    R_NPC(TIMER_S3); R_NPC(POWER_S2); R_NPC(POWER_S5); R_NPC(GEM_1);
    R_NPC(GEM_5); R_NPC(GEM_20); R_NPC(FLY_POWER); R_NPC(LOCK_DOOR);
    R_NPC(LONG_PLANT_UP); R_NPC(LONG_PLANT_DOWN); R_NPC(COIN_5); R_NPC(FIRE_DISK);
    R_NPC(FIRE_CHAIN); R_NPC(WALK_PLANT); R_NPC(BOMBER_BOSS); R_NPC(ICE_CUBE);
    R_NPC(ICE_POWER_S3); R_NPC(PLR_ICEBALL); R_NPC(SWORDBEAM); R_NPC(MAGIC_BOSS);
    R_NPC(MAGIC_BOSS_SHELL); R_NPC(MAGIC_BOSS_BALL); R_NPC(JUMP_PLANT); R_NPC(BAT);
    R_NPC(VINE_BUG); R_NPC(SWAP_POWER); R_NPC(MEDAL); R_NPC(QUAD_SPITTER);
    R_NPC(QUAD_BALL); R_NPC(ICE_POWER_S4); R_NPC(FLY_BLOCK); R_NPC(FLY_CANNON);
    R_NPC(FIRE_BOSS); R_NPC(FIRE_BOSS_SHELL); R_NPC(FIRE_BOSS_FIRE); R_NPC(ITEM_BUBBLE);
    R_NPC(ITEM_THROWER); R_NPC(SPIKY_S4); R_NPC(SPIKY_BALL_S4); R_NPC(RANDOM_POWER);
    R_NPC(DOOR_MAKER); R_NPC(MAGIC_DOOR); R_NPC(COCKPIT); R_NPC(CHAR3_HEAVY);
    R_NPC(CHAR4_HEAVY); R_NPC(RESERVED_293); R_NPC(RESERVED_294); R_NPC(RESERVED_295);
    R_NPC(RESERVED_296); R_NPC(RESERVED_297); R_NPC(RESERVED_298); R_NPC(RESERVED_299);
    R_NPC(RESERVED_300); R_NPC(INVINCIBILITY_POWER); R_NPC(AQUATIC_POWER); R_NPC(POLAR_POWER);
    R_NPC(CYCLONE_POWER); R_NPC(SHELL_POWER); R_NPC(FLAG_EXIT);

    // --- SFX IDs (106 entries) ---
    R_SFX(Jump); R_SFX(Stomp); R_SFX(BlockHit); R_SFX(BlockSmashed);
    R_SFX(PlayerShrink); R_SFX(PlayerGrow); R_SFX(ItemEmerge); R_SFX(PlayerDied);
    R_SFX(ShellHit); R_SFX(Skid); R_SFX(DropItem); R_SFX(GotItem);
    R_SFX(Camera); R_SFX(Coin); R_SFX(1up); R_SFX(Lava);
    R_SFX(Warp); R_SFX(Fireball); R_SFX(CardRouletteClear); R_SFX(BossBeat);
    R_SFX(DungeonClear); R_SFX(Bullet); R_SFX(Grab); R_SFX(Spring);
    R_SFX(HeavyToss); R_SFX(Slide); R_SFX(NewPath); R_SFX(LevelSelect);
    R_SFX(Do); R_SFX(Pause); R_SFX(Key); R_SFX(PSwitch);
    R_SFX(Whip); R_SFX(Transform); R_SFX(Boot); R_SFX(Smash);
    R_SFX(Stone); R_SFX(SpitBossSpit); R_SFX(SpitBossHit); R_SFX(CrystalBallExit);
    R_SFX(SpitBossBeat); R_SFX(BigFireball); R_SFX(Fireworks); R_SFX(VillainKilled);
    R_SFX(GameBeat); R_SFX(Door); R_SFX(Message); R_SFX(Pet);
    R_SFX(PetHurt); R_SFX(PetTongue); R_SFX(PetBirth); R_SFX(GotStar);
    R_SFX(HeroKill); R_SFX(PlayerDied2); R_SFX(PetSwallow); R_SFX(RingGet);
    R_SFX(Skeleton); R_SFX(Checkpoint); R_SFX(MedalGet); R_SFX(TapeExit);
    R_SFX(LavaMonster); R_SFX(SickBossSpit); R_SFX(SickBossKilled); R_SFX(SMBlockHit);
    R_SFX(SMKilled); R_SFX(SMHurt); R_SFX(SMGlass); R_SFX(SMBossHit);
    R_SFX(SMCry); R_SFX(SMExplosion); R_SFX(Climbing); R_SFX(Swim);
    R_SFX(Grab2); R_SFX(Saw); R_SFX(Throw); R_SFX(PlayerHit);
    R_SFX(HeroStab); R_SFX(HeroHurt); R_SFX(HeroHeart); R_SFX(HeroDied);
    R_SFX(HeroRupee); R_SFX(HeroFire); R_SFX(HeroItem); R_SFX(HeroKey);
    R_SFX(HeroShield); R_SFX(HeroDash); R_SFX(HeroFairy); R_SFX(HeroGrass);
    R_SFX(HeroHit); R_SFX(HeroSwordBeam); R_SFX(Bubble); R_SFX(CoinSwitchTimeout);
    R_SFX(BatFlap); R_SFX(Iceball); R_SFX(Freeze); R_SFX(Icebreak);
    R_SFX(PlayerHeavy); R_SFX(SproutVine); R_SFX(MagicBossShell); R_SFX(MagicBossKilled);
    R_SFX(FireBossKilled); R_SFX(HeroIce); R_SFX(HeroFireRod); R_SFX(FlameThrower);
    R_SFX(FlagExit);

    // --- EFFID (152 entries) ---
    R_EFF(SMOKE_S3_CENTER); R_EFF(BLOCK_SMASH); R_EFF(FODDER_S3_SQUISH); R_EFF(CHAR1_DIE);
    R_EFF(FODDER_S3_DIE); R_EFF(CHAR2_DIE); R_EFF(RED_FODDER_SQUISH); R_EFF(RED_FODDER_DIE);
    R_EFF(GRN_SHELL_S3_DIE); R_EFF(RED_SHELL_S3_DIE); R_EFF(SMOKE_S3); R_EFF(COIN_BLOCK_S3);
    R_EFF(BIG_FIREBALL_TAIL); R_EFF(LAVA_SPLASH); R_EFF(MINIBOSS_DIE); R_EFF(BULLET_DIE);
    R_EFF(BIG_BULLET_DIE); R_EFF(RED_GUY_DIE); R_EFF(BLU_GUY_DIE); R_EFF(GLASS_SHELL_DIE);
    R_EFF(JUMPER_S3_DIE); R_EFF(BLU_BLOCK_SMASH); R_EFF(UNDER_FODDER_DIE); R_EFF(UNDER_FODDER_SQUISH);
    R_EFF(RED_FISH_S1_DIE); R_EFF(HEAVY_THROWER_DIE); R_EFF(GRN_BOOT_DIE); R_EFF(SPIKY_S3_DIE);
    R_EFF(SPIT_BOSS_BALL_DIE); R_EFF(SPIT_BOSS_DIE); R_EFF(SLIDE_BLOCK_SMASH); R_EFF(SPIKY_BALL_S3_DIE);
    R_EFF(SPIKY_THROWER_DIE); R_EFF(CRAB_DIE); R_EFF(FLY_DIE); R_EFF(EXT_TURTLE_SQUISH);
    R_EFF(EXT_TURTLE_DIE); R_EFF(YELSWITCH_FODDER_SQUISH); R_EFF(YELSWITCH_FODDER_DIE); R_EFF(BLUSWITCH_FODDER_SQUISH);
    R_EFF(BLUSWITCH_FODDER_DIE); R_EFF(GRNSWITCH_FODDER_SQUISH); R_EFF(GRNSWITCH_FODDER_DIE); R_EFF(REDSWITCH_FODDER_SQUISH);
    R_EFF(REDSWITCH_FODDER_DIE); R_EFF(BIG_FODDER_SQUISH); R_EFF(BIG_FODDER_DIE); R_EFF(BIG_SHELL_DIE);
    R_EFF(POWER_S3_DIE); R_EFF(JUMPER_S4_DIE); R_EFF(VILLAIN_S3_DIE); R_EFF(BLOCK_S1_SMASH);
    R_EFF(FODDER_S1_SQUISH); R_EFF(FODDER_S1_DIE); R_EFF(DOOR_S2_OPEN); R_EFF(DOOR_DOUBLE_S3_OPEN);
    R_EFF(ITEM_POD_OPEN); R_EFF(ITEM_POD_BREAK); R_EFF(PET_BIRTH); R_EFF(DOOR_SIDE_S3_OPEN);
    R_EFF(SHELL_S4_DIE); R_EFF(HIT_TURTLE_S4_DIE); R_EFF(HIT_TURTLE_S4_SQUISH); R_EFF(SMOKE_S5);
    R_EFF(BIRD_DIE); R_EFF(RED_SPIT_GUY_DIE); R_EFF(BLU_SPIT_GUY_DIE); R_EFF(GRY_SPIT_GUY_DIE);
    R_EFF(SPIT_GUY_BALL_DIE); R_EFF(BOMB_S2_EXPLODE); R_EFF(BOMB_S3_EXPLODE_SEED); R_EFF(BOMB_S3_EXPLODE);
    R_EFF(WALK_BOMB_S3_DIE); R_EFF(WHIP); R_EFF(SKID_DUST); R_EFF(WHACK);
    R_EFF(BOOT_STOMP); R_EFF(PLR_FIREBALL_TRAIL); R_EFF(COIN_COLLECT); R_EFF(SCORE);
    R_EFF(SPARKLE); R_EFF(COIN_SWITCH_PRESS); R_EFF(SPINBLOCK); R_EFF(CARRY_BUDDY_DIE);
    R_EFF(BRUTE_SQUISH); R_EFF(BRUTE_SQUISHED_DIE); R_EFF(BRUTE_DIE); R_EFF(BIG_GUY_DIE);
    R_EFF(CARRY_FODDER_DIE); R_EFF(CHASER_DIE); R_EFF(STONE_S3_DIE); R_EFF(BIG_GHOST_DIE);
    R_EFF(GHOST_S4_DIE); R_EFF(GHOST_FAST_DIE); R_EFF(GHOST_S3_DIE); R_EFF(GRN_SHELL_S1_DIE);
    R_EFF(RED_SHELL_S1_DIE); R_EFF(SKELETON_DIE); R_EFF(STONE_S4_DIE); R_EFF(SAW_DIE);
    R_EFF(GRY_BLOCK_SMASH); R_EFF(RED_BOOT_DIE); R_EFF(BLU_BOOT_DIE); R_EFF(BIG_DOOR_OPEN);
    R_EFF(LAVA_MONSTER_LOOK); R_EFF(VILLAIN_S1_DIE); R_EFF(SICK_BOSS_DIE); R_EFF(SPACE_BLOCK_SMASH);
    R_EFF(BOSS_FRAGILE_EXPLODE); R_EFF(WALL_TURTLE_DIE); R_EFF(WALL_SPARK_DIE); R_EFF(BOSS_CASE_BREAK);
    R_EFF(BOSS_FRAGILE_DIE); R_EFF(AIR_BUBBLE); R_EFF(WATER_SPLASH); R_EFF(GRN_FISH_S3_DIE);
    R_EFF(RED_FISH_S3_DIE); R_EFF(SQUID_S3_DIE); R_EFF(GRN_FISH_S4_DIE); R_EFF(GRN_FISH_S1_DIE);
    R_EFF(BONE_FISH_DIE); R_EFF(SQUID_S1_DIE); R_EFF(YEL_FISH_S4_DIE); R_EFF(TIME_SWITCH_PRESS);
    R_EFF(TNT_PRESS); R_EFF(EARTHQUAKE_BLOCK_HIT); R_EFF(FODDER_S5_SQUISH); R_EFF(FODDER_S5_DIE);
    R_EFF(STACKER_DIE); R_EFF(CHAR3_DIE); R_EFF(CHAR4_DIE); R_EFF(SMOKE_S4);
    R_EFF(STOMP_INIT); R_EFF(STOMP_STAR); R_EFF(CHAR5_DIE); R_EFF(DIRT_BLOCK_SMASH);
    R_EFF(FIRE_DISK_DIE); R_EFF(WALK_PLANT_DIE); R_EFF(BOMBER_BOSS_DIE); R_EFF(PLR_ICEBALL_TRAIL);
    R_EFF(MAGIC_BOSS_DIE); R_EFF(BAT_DIE); R_EFF(VINE_BUG_DIE); R_EFF(FIRE_BOSS_DIE);
    R_EFF(BUBBLE_POP); R_EFF(ITEM_THROWER_DIE); R_EFF(SPIKY_S4_DIE); R_EFF(SMOKE_S2);
    R_EFF(CHAR3_HEAVY_EXPLODE); R_EFF(GENERIC_NPC_DIE); R_EFF(GENERIC_NPC_SQUISH);

#undef R_NPC
#undef R_SFX
#undef R_EFF
}
