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

#ifndef NPC_TRAITS_H
#define NPC_TRAITS_H

#include <cstdint>

#include "range_arr.hpp"

#include "globals.h"
#include "npc_constant_traits.h"

struct NPCTraits_t
{
    //Public NPCFrameOffsetX(0 To maxNPCType) As Integer 'NPC frame offset X
    int16_t FrameOffsetX = 0;
    //Public NPCFrameOffsetY(0 To maxNPCType) As Integer 'NPC frame offset Y
    int16_t FrameOffsetY = 0;
    //Public NPCWidth(0 To maxNPCType) As Integer 'NPC width
    int16_t TWidth = 32;
    //Public NPCHeight(0 To maxNPCType) As Integer 'NPC height
    int16_t THeight = 32;
    //Public NPCWidthGFX(0 To maxNPCType) As Integer 'NPC gfx width
    int16_t WidthGFX = 0;
    //Public NPCHeightGFX(0 To maxNPCType) As Integer 'NPC gfx height
    int16_t HeightGFX = 0;
    //! Total sprite sheet height for vertical frame splitting (0 = use default heuristic)
    int16_t GfxSplitHeight = 0;
    //Public NPCSpeedvar(0 To maxNPCType) As Single 'NPC Speed Change
    numf_t Speedvar = 1;

    //Public NPCIsABlock(0 To maxNPCType) As Boolean 'Flag NPC as a block
    bool IsABlock = false;
    //Public NPCIsAHit1Block(0 To maxNPCType) As Boolean 'Flag NPC as a hit1 block
    bool IsAHit1Block = false;
    //Public NPCJumpHurt(0 To maxNPCType) As Boolean 'Hurts the player even if it jumps on the NPC
    bool JumpHurt = false;
    //Public NPCNoClipping(0 To maxNPCType) As Boolean 'NPC can go through blocks
    bool NoClipping = false;
    //Public NPCScore(0 To maxNPCType) As Integer 'NPC score value
    int8_t Score = 2;
    //Public NPCCanWalkOn(0 To maxNPCType) As Boolean  'NPC can be walked on
    bool CanWalkOn = false;
    //Public NPCGrabFromTop(0 To maxNPCType) As Boolean  'NPC can be grabbed from the top
    bool GrabFromTop = false;
    //Public NPCTurnsAtCliffs(0 To maxNPCType) As Boolean  'NPC turns around at cliffs
    bool TurnsAtCliffs = false;
    //Public NPCWontHurt(0 To maxNPCType) As Boolean  'NPC wont hurt the player
    bool WontHurt = false;
    //Public NPCMovesPlayer(0 To maxNPCType) As Boolean 'Player can not walk through the NPC
    bool MovesPlayer = false;
    //Public NPCStandsOnPlayer(0 To maxNPCType) As Boolean 'for the clown car
    bool StandsOnPlayer = false;
    //Public NPCIsGrabbable(0 To maxNPCType) As Boolean 'Player can grab the NPC
    bool IsGrabbable = false;
    //Public NPCNoYoshi(0 To maxNPCType) As Boolean 'Player can't eat the NPC
    bool NoYoshi = false;
    //Public NPCForeground(0 To maxNPCType) As Boolean 'draw the npc in front
    bool Foreground = false;
    //Public NPCNoFireBall(0 To maxNPCType) As Boolean 'not hurt by fireball
    bool NoFireBall = false;
    //Public NPCNoIceBall(0 To maxNPCType) As Boolean 'not hurt by fireball
    bool NoIceBall = false;
    //Public NPCNoGravity(0 To maxNPCType) As Boolean 'not affected by gravity
    bool NoGravity = false;

    //Public NPCFrame(0 To maxNPCType) As Integer
    int16_t TFrames = 0;
    //Public NPCFrameSpeed(0 To maxNPCType) As Integer
    int16_t FrameSpeed = 8;
    //Public NPCFrameStyle(0 To maxNPCType) As Integer
    int16_t FrameStyle = 0;

    // Uses fish AI. Redigit's comment: 'Flags the NPC type as a cheep cheep
    bool IsFish : 1;
    //'Flags the NPC type if it is a coin
    bool IsACoin : 1;
    //'Flags the NPC type if it is a bonus
    bool IsABonus : 1;
    //'Flags the NPC type if it is a vine
    bool IsAVine : 1;
    //'Flags the NPC type if it is a shell
    bool IsAShell : 1;

    //NEW: does the NPC require the canonical activation zone?
    bool UseDefaultCam : 1;

    enum InactiveRender_t
    {
        SHADE = 0,   // shades NPC until it becomes active
        SHOW_ALWAYS, // always show the NPC (including animation)
        SHOW_STATIC, // always show the NPC (without animation)
        SKIP,        // skips NPC's render (assume it handles own intro animation from invisibility)
        SMOKE,       // hides the NPC and reveals it with a EFFID_SMOKE_S3 effect
    };

    //NEW: how should the NPC render when inactive?
    InactiveRender_t InactiveRender : 3;

    // Extended traits (from SMBX-38A / TheXTech custom npc-*.txt)
    bool NoHammer : 1;          // immune to hammers/axes
    bool NoShell : 1;           // immune to thrown shells
    bool NoLava : 1;            // immune to lava
    bool NoLeaf : 1;            // immune to leaf/tanooki tail
    bool NoBlockHit : 1;        // immune to damage from block hits
    bool SpinJump : 1;          // allows spin jump stomp (default true for backward compat)
    bool SpinJumpHurt : 1;      // hurts player on spin jump
    bool WaterJumpHurt : 1;     // hurts player on water jump
    bool YoshiHurt : 1;         // hurts yoshi-rider on stomp
    bool InstantKill : 1;       // instant kill on contact
    bool Immortal : 1;          // cannot be killed (respawns itself)
    bool GroundpoundBreak : 1;  // ground pound breaks this NPC
    bool MegaBreak : 1;         // mega form breaks this NPC
    bool TurboWeak : 1;         // turbo dash deals shell damage
    bool NoSectionWrap : 1;     // ignores section wrap
    bool NpcBlockSide : 1;      // block NPCs at side (separate from npcblock)
    bool Float : 1;             // floats on water surface
    bool NoPiercingDmg : 1;     // immune to piercing damage
    bool Pushable : 1;          // can be pushed by player
    bool Stackable : 1;         // stays still when on another Stackable NPC
    bool CanMeltBlock : 1;      // can melt ice blocks on contact
    bool WingsForever : 1;      // wings don't disappear when hurt
    // Transform-on-death targets (0 = no transform)
    int16_t FrozenTime = 0;     // freeze duration in frames (-1 = never)
    int16_t YoshiTransform = 0; // NPC ID after yoshi swallow
    int16_t FrozenTransform = 0;// NPC ID after frozen
    int16_t JumpTransform = 0;  // NPC ID after jump stomp

    constexpr NPCTraits_t() : IsFish(false), IsACoin(false), IsABonus(false), IsAVine(false), IsAShell(false), UseDefaultCam(false), InactiveRender(SHADE), NoHammer(false), NoShell(false), NoLava(false), NoLeaf(false), NoBlockHit(false), SpinJump(true), SpinJumpHurt(false), WaterJumpHurt(false), YoshiHurt(false), InstantKill(false), Immortal(false), GroundpoundBreak(false), MegaBreak(false), TurboWeak(false), NoSectionWrap(false), NpcBlockSide(false), Float(false), NoPiercingDmg(false), Pushable(false), Stackable(false), CanMeltBlock(false), WingsForever(false) {}
};

extern RangeArr<NPCTraits_t, 0, maxNPCType> NPCTraits;

inline const NPCTraits_t* NPC_t::operator->() const
{
    return &NPCTraits[Type];
}


// some read-only accessors
inline int16_t NPCHeight(int Type)
{
    return NPCTraits[Type].THeight;
}

inline int16_t NPCWidth(int Type)
{
    return NPCTraits[Type].TWidth;
}

inline int16_t NPCHeightGFX(int Type)
{
    return NPCTraits[Type].HeightGFX;
}

inline int16_t NPCWidthGFX(int Type)
{
    return NPCTraits[Type].WidthGFX;
}

inline int16_t NPCFrameOffsetX(int Type)
{
    return NPCTraits[Type].FrameOffsetX;
}

inline int16_t NPCFrameOffsetY(int Type)
{
    return NPCTraits[Type].FrameOffsetY;
}

inline bool NPCNoYoshi(int Type)
{
    return NPCTraits[Type].NoYoshi;
}

inline bool Block_t::tempBlockNoProjClipping() const
{
    if(tempBlockNpcType <= 0)
        return false;

    // cross-ref NPC temp block creation code in UpdateNPCs
    const NPCTraits_t& npc_tr = NPCTraits[tempBlockNpcType];
    return (npc_tr.CanWalkOn && !npc_tr.IsAHit1Block && !npc_tr.IsABlock);
}

inline vbint_t NPC_t::coinSwitchBlockType() const
{
    if(NPCTraits[this->DefaultType].IsACoin)
        return this->Damage;

    return 0;
}

#endif // #ifndef NPC_TRAITS_H
