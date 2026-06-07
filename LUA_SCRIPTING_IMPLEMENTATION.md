# LuaJIT 全量化游戏脚本接入 — 实施报告

**日期：** 2026-06-06（更新）
**项目：** TheXTech
**参考文档：** SMBx_Scripting_Help_1.4.5.rtf（TeaScript API）

---

## 概述

本文档记录了为 TheXTech 引擎实现完整 LuaJIT 游戏脚本支持的代码变更。实现使用项目已有的 LuaJIT 2.1 + luabind 基础设施，通过 `THEXTECH_ENABLE_LUA=ON` CMake 选项启用。

**设计原则：** 以 TeaScript 文档作为功能清单，用 Lua 惯用方式暴露 TheXTech 已存在的游戏数据，不复刻 TeaScript 的 VB6 风格 API 表面。

---

## 1. Lua VM 生命周期管理

**文件：** [script/include/xtech_lua_main.h](script/include/xtech_lua_main.h)
[script/src/xtech_lua_main.cpp](script/src/xtech_lua_main.cpp)

| 函数 | 说明 |
|------|------|
| `xtech_lua_init()` | 创建 LuaJIT VM（`luaL_newstate`），打开所有标准库，调用 `luabind::open`，注册游戏绑定，加载 `lunaglobal.lua` |
| `xtech_lua_load()` | 从关卡目录加载 `level.lua`（或 `lunadll.lua`），检索所有钩子函数（`onLoad`, `onLoop` 等），立即调用 `onLoad()` |
| `xtech_lua_loop()` | 处理异步延迟回调队列，然后调用 Lua `onLoop()` 函数 |
| `xtech_lua_reset()` | 清除延迟回调队列和所有函数引用，销毁并重建 Lua VM |
| `xtech_lua_renderStart()` | 调用 Lua `onRenderStart()` |
| `xtech_lua_renderEnd()` | 调用 Lua `onRenderEnd()` |
| `xtech_lua_render(screenZ)` | 调用 Lua `onRender(screenZ)` |
| `xtech_lua_renderHud(screenZ)` | 调用 Lua `onRenderHud(screenZ)` |
| `xtech_lua_quit()` | 调用 `onLoopEnd()`，关闭 Lua VM |

**脚本加载顺序：**
1. 启动时：从资源目录加载 `lunaglobal.lua`（全局脚本）
2. 进入关卡时：从关卡自定义目录加载 `level.lua` 或 `lunadll.lua`
3. 加载后：自动检索脚本中定义的钩子函数

**Lua 钩子函数：**
```lua
function onLoad()           -- 关卡加载时调用一次
function onLoop()           -- 每帧调用
function onLoopEnd()        -- 关卡结束时调用一次
function onRenderStart()    -- 每帧渲染前
function onRenderEnd()      -- 每帧渲染后
function onRender(Z)        -- 每层渲染（Z=屏幕层号）
function onRenderHud(Z)     -- HUD 渲染
end
```

**错误处理：** 所有 Lua 函数调用都通过 try-catch 间接调用，异常被捕获并记录到日志，不会导致游戏崩溃。

---

## 2. 异步延迟回调支持

**文件：** [script/src/xtech_lua_bindings.cpp](script/src/xtech_lua_bindings.cpp)
[script/src/xtech_lua_main.cpp](script/src/xtech_lua_main.cpp)

实现了基于帧计数的延迟回调队列系统：

```lua
-- 延迟 60 帧（约 1 秒）后执行
xtech_misc_wait(function()
    xtech_misc_log("1 second has passed!")
    xtech_audio_playSFX(CONST_SFX_Coin, 0, 128)
end, 60)

-- 链式延迟调用（每个 30 帧后执行下一步）
xtech_misc_wait(function()
    xtech_hud_showText("3...", 400, 300, 3)
    xtech_misc_wait(function()
        xtech_hud_showText("2...", 400, 300, 3)
        xtech_misc_wait(function()
            xtech_hud_showText("1...", 400, 300, 3)
            xtech_misc_wait(function()
                xtech_hud_showText("Go!", 400, 300, 3)
            end, 30)
        end, 30)
    end, 30)
end, 30)
```

- 延迟回调在 `xtech_lua_loop()` 中处理，每帧递减计时器
- 计时器归零时执行回调
- 回调中的错误被独立捕获，不会影响其他回调
- `xtech_lua_reset()` 清除所有待处理回调

---

## 3. 游戏对象类绑定

**文件：** [script/src/xtech_lua_bindings.cpp](script/src/xtech_lua_bindings.cpp)

### 3.1 Location_t（位置/碰撞箱）
```
X, Y, Width, Height, SpeedX, SpeedY — 全部可读写
```

### 3.2 SpeedlessLocation_t（无速度位置）✅ 新增
```
X, Y, Width, Height — 全部可读写
用于 Background_t、Water_t、Warp_t 和 Section 位置
```

### 3.3 LunaRect（矩形区域）
```
left, top, right, bottom — 全部可读写
```

### 3.4 Hitbox（碰撞盒）
```
Left_off, Top_off, W, H, CollisionType — 全部可读写
```

### 3.5 NPC_t（非玩家角色）— 完整字段绑定 ✅ 已扩展

**原有字段:**
```
Type, Killed, Frame, Active, Hidden, Inert
Location, SpecialX, SpecialY, Special, Special2, Special3, Special4, Special5
Section, Wet, Direction
Damage, Immune, CantHurt, Multiplier, TailCD
Projectile, Variant, BattleOwner, HoldingPlayer, CantHurtPlayer
RealSpeedX, BeltSpeed, oldAddBelt
Effect, Effect2, Effect3, TimeLeft, JustActivated
Slope, vehiclePlr, vehicleYOffset, WallDeath
GFXSlot, Wings, FrameCount
```

**P0 新增字段（对应 TeaScript NPC 属性）:**
```
DefaultLocationX, DefaultLocationY  -- TeaScript: PrX, PrY（原始生成位置）
DefaultType                         -- TeaScript: 默认 NPC 类型
DefaultSpecial                      -- TeaScript: 默认附加数据
DefaultDirection                    -- TeaScript: 默认朝向
DefaultWings                        -- TeaScript: 默认翅膀类型
```

**事件/字符串索引（P0 新增）:**
```
TriggerActivate     -- TeaScript: Activeevent（激活事件）
TriggerDeath        -- TeaScript: Deathevent（死亡事件）
TriggerTalk         -- TeaScript: Talkevent（对话事件）
TriggerLast         -- TeaScript: 最后 NPC 事件
Text                -- TeaScript: 对话文字索引
Layer               -- TeaScript: 所在图层索引
AttLayer            -- 附加图层
```

**位域成员 (通过 getter/setter 方法访问):**

原有：
```
getGenerator/setGenerator, getGeneratorActive/setGeneratorActive
getChat/setChat, getLegacy/setLegacy
getTurnAround/setTurnAround, getTurnBackWipe/setTurnBackWipe
getPlayerTemp/setPlayerTemp, getNoLavaSplash/setNoLavaSplash
getBouce/setBouce, getDefaultStuck/setDefaultStuck
getRespawnDelay/setRespawnDelay
```

P0 新增（对应 TeaScript 属性）：
```
getStuck/setStuck          -- TeaScript: NoMove（禁止移动）
getShadow/setShadow        -- 影子状态（作弊码相关）
getQuicksand/setQuicksand  -- TeaScript: 流沙计数器
```

**Generator 子字段:**
```
getGeneratorDirection, getGeneratorEffect
getGeneratorTimeMax/setGeneratorTimeMax
getGeneratorTime/setGeneratorTime
```

**对象标识方法（P2 新增）:**
```
getPermID()          -- 返回此 NPC 在全局数组中的 1-based 索引（永久 ID）
```

### 3.6 Player_t（玩家角色）— 完整字段绑定 ✅ 已扩展

**原有字段:**
```
DoubleJump, FlySparks, Driving, Quicksand, Bombs, Slippy
Fairy, FairyCD, FairyTime, HasKey, Hearts
CanFloat, FloatTime, FloatSpeed, FloatDir
GrabTime, GrabSpeed, VineNPC, VineBGO
Wet, WetFrame, SwimCount, NoGravity
Slide, SlideKill, Vine, ShellSurf, Rolling
StateNPC, Slope, Stoned, AquaticSwim
StonedCD, StonedTime, SpinJump, SpinFrame, SpinFireDir
Multiplier, SlideCounter, ShowWarp, ForceHold
YoshiYellow, YoshiBlue, YoshiRed
YoshiTongueLength, YoshiNPC, YoshiPlayer, Dismount
Location, Character, Direction, Mount, MountType
MountSpecial, MountOffsetY, MountFrame
State, Frame, FrameCount, Jump, CanJump, CanAltJump
Effect, Effect2, RespawnY, Duck, DropRelease, StandUp, StandUp2, Bumped
```

**之前新增字段:**
```
Bumped2, Dead, TimeToLive, Immune, Immune2, ForceHitSpot3
HoldingNPC, CanGrabNPCs, HeldBonus, Section
WarpCD, Warp, WarpBackward, WarpShooted
FireBallCD, FireBallCD2, TailCount, RunCount
CanFly, CanFly2, FlyCount
RunRelease, JumpRelease, StandingOnNPC, StandingOnVehiclePlr
UnStart, mountBump, CurMazeZone, MazeZoneStatus
```

**P0 新增字段:**
```
FloatRelease         -- 浮空释放标记
SwordPoke            -- 剑刺计数器

-- Yoshi 图形显示（完整8个字段）
YoshiWingsFrame, YoshiWingsFrameCount
YoshiTX, YoshiTY, YoshiTFrame, YoshiTFrameCount
YoshiBX, YoshiBY, YoshiBFrame, YoshiBFrameCount
YoshiTongue           -- Yoshi 舌头位置（SpeedlessLocation_t）
YoshiTonugeBool       -- Yoshi 舌头布尔值
```

**位域成员 (通过 getter/setter 方法访问):**
```
getGroundPound/setGroundPound, getGroundPound2/setGroundPound2
getCanPound/setCanPound, getAltRunRelease/setAltRunRelease
getDuckRelease/setDuckRelease, getSlippyWall/setSlippyWall
getJumpOffWall/setJumpOffWall
```

### 3.7 Layer_t（图层）
```
Name (只读), SpeedX, SpeedY, Hidden, EffectStop
```

### 3.8 Block_t（方块）✅ 已扩展

**原有字段:**
```
Location, Type, Special, Invis, Hidden, Slippy, Kill, RapidHit
```

**P0 新增字段（对应 TeaScript Block 属性）:**
```
forceSmashable              -- 强制可砸碎标记
RespawnDelay_ScreensLeft    -- TeaScript: 活跃屏幕计数
DefaultType                 -- TeaScript: 默认类型
DefaultSpecial              -- TeaScript: 默认内容
TriggerHit                  -- TeaScript: 命中事件
TriggerDeath                -- TeaScript: 销毁事件
TriggerLast                 -- TeaScript: 最后方块事件
Layer                       -- TeaScript: 所在图层
ShakeCounter, ShakeOffset   -- TeaScript: 震动状态
coinSwitchNpcType           -- P开关硬币转方块时的 NPC 类型
tempBlockVehiclePlr         -- 临时方块上的玩家
tempBlockNpcType            -- 临时方块的 NPC 类型
tempBlockVehicleYOffset     -- Y 偏移
tempBlockNpcIdx             -- 临时方块 NPC 索引
```

### 3.9 Background_t（BGO / 背景对象）✅ P1 新增

```
Type        -- TeaScript: ID（背景对象类型 ID）
Hidden      -- TeaScript: 隐藏标记
Layer       -- 所在图层索引
SortPriority -- 排序优先级
Location    -- TeaScript: X, Y, Width, Height（SpeedlessLocation_t）
```

**TeaScript BGO 属性映射:**
- `Bgo(id).X` → `BGO.Location.X`
- `Bgo(id).Y` → `BGO.Location.Y`
- `Bgo(id).ID` → `BGO.Type`
- `Bgo(id).Forecolor` → 暂未暴露（TheXTech BGO 无颜色字段）

### 3.10 Water_t（Liquid / 液体）✅ P1 新增

```
Location    -- TeaScript: X, Y（SpeedlessLocation_t）
Type        -- TeaScript: 液体类型（PHYSID: 1=水, 2=流沙）
Hidden      -- TeaScript: 隐藏
Layer       -- 所在图层索引
```

**与 TeaScript Liquid 属性对比:**
- TeaScript 有 `Xsp/Ysp`（液体移动速度）和 `Fdir/Fval/Fmax`（力场参数）。
  这些在 TheXTech 的 Water_t 结构体中不存在（TheXTech 液体使用不同的物理系统）。
- 如需控制液体行为，可能需要通过其他 API 实现。

### 3.11 Warp_t（管道/传送门）✅ P1 新增

```
Entrance        -- TeaScript: X, Y（入口位置，SpeedlessLocation_t）
Exit            -- TeaScript: Ex, Ey（出口位置，SpeedlessLocation_t）
Locked          -- TeaScript: Locked（需要钥匙）
WarpNPC         -- 允许 NPC 通过
NoYoshi         -- TeaScript: Noyoshi（禁止坐骑进入）
Hidden          -- 隐藏
Stars           -- TeaScript: Starcnt（需要星星数）
Effect          -- TeaScript: 管道/门样式
LevelWarp       -- 目标关卡传送门
LevelEnt        -- 是否为关卡入口
Direction       -- TeaScript: 入口方向
Direction2      -- 出口方向
MapWarp         -- 是否世界地图传送门
MapX, MapY      -- 地图坐标
curStars        -- 当前星星数
twoWay          -- 双向传送门
noPrintStars    -- 不显示星星需求
noEntranceScene -- 无入口场景
cannonExit      -- 大炮出口模式
cannonExitSpeed -- 大炮出口速度
stoodRequired   -- 需要站立进入
eventEnter      -- 进入事件
eventExit       -- 退出事件
StarsMsg        -- TeaScript: Starmsg（星星不足提示文本索引）
transitEffect   -- 过渡特效
Layer           -- 所在图层
```

**与 TeaScript Warp 属性对比:**
- TeaScript `Xsp/Ysp` → C++ 中 Warp_t 没有移动速度字段（传送门位置由编辑器固定）
- TeaScript `Canpick/Mini` → TheXTech Warp_t 中无对应字段（可能通过其他机制控制）

### 3.12 SpriteComponent（精灵组件）
```
data1, data2, data3, data4, lookup_code, run_time, org_time, data5, expired
```

### 3.13 LunaImage（加载的图片资源）
```
getWidth(), getHeight(), getUID(), isLoaded()
```

### 3.14 CSprite（自定义精灵）
```
ImgResCode, CollisionCode, FramesLeft, DrawPriorityLevel
OffscreenCount, FrameCounter, GfxXOffset, GfxYOffset
StaticScreenPos, Visible, Birthed, Died, Invalidated
LimitedFrameLife, AnimationSet, AlwaysProcess
Xpos, Ypos, Ht, Wd, Xspd, Yspd
Hitbox, AnimationPhase, AnimationTimer, AnimationFrame

方法:
setImage(img), setImageResource(code), makeLimitedLife(frames)
setCustomVar(name, op, value), customVarExists(name), getCustomVar(name)
birth(), die()
```

### 3.15 Effect_t（特效）✅ P3 新增

```
Location    -- 特效位置（Location_t，含 SpeedX/SpeedY）
Type        -- 特效类型 ID（EFFID）
Frame       -- 当前帧
FrameCount  -- 帧计数器
Life        -- 剩余生命（帧）
NewNpc      -- 特效结束时生成的 NPC ID
NewNpcSpecial -- 新 NPC 的附加数据
Shadow      -- 是否暗色特效
```

---

## 4. 游戏操作 API

### 4.1 NPC 操作
```lua
npc = xtech_npc_get(index)                    -- 获取第 index 个 NPC（1-based）
count = xtech_npc_count()                     -- NPC 总数
npc = xtech_npc_getFirstMatch(id, section)    -- 按 ID 和 section 查找第一个匹配
xtech_npc_memSet(id, offset, value, op, ftype)-- 内存级 NPC 属性修改
xtech_npc_allSetHits(identity, section, hits) -- 批量设置 NPC 生命值
xtech_npc_kill(a, b)                          -- 杀死 NPC
xtech_npc_hurt(a, b, c)                       -- 伤害 NPC
-- P2: 精确对象操作
npc = xtech_npc_getByPermID(permId)           -- 通过永久 ID 精确获取 NPC
xtech_npc_forEach(id, section, function(npc)  -- 遍历所有匹配 NPC 执行回调
    npc:setStuck(true)
end)
-- P3: 动态创建 NPC
npc = xtech_npc_create(type, x, y, xspd, yspd) -- 动态创建 NPC，返回对象或 nil
```

### 4.1 NPC 操作
```lua
npc = xtech_npc_get(index)                    -- 获取第 index 个 NPC（1-based）
count = xtech_npc_count()                     -- NPC 总数
npc = xtech_npc_getFirstMatch(id, section)    -- 按 ID 和 section 查找第一个匹配
xtech_npc_memSet(id, offset, value, op, ftype)-- 内存级 NPC 属性修改
xtech_npc_allSetHits(identity, section, hits) -- 批量设置 NPC 生命值
xtech_npc_kill(a, b)                          -- 杀死 NPC
xtech_npc_hurt(a, b, c)                       -- 伤害 NPC
-- P2 新增：精确对象操作
npc = xtech_npc_getByPermID(permId)           -- 通过永久 ID 精确获取 NPC
xtech_npc_forEach(id, section, function(npc)  -- 遍历所有匹配 NPC 执行回调
    -- id=0 匹配所有类型, section=0 匹配所有区域
    npc:setStuck(true)
end)
```

**getPermID 使用模式：**
```lua
-- 保存 NPC 的永久 ID，后续精确找回
local targetId = nil
function onLoad()
    local npc = xtech_npc_getFirstMatch(CONST_NPC_LOCK_DOOR, 0)
    if npc then
        targetId = npc:getPermID()   -- 记录永久 ID
    end
end

function onLoop()
    if targetId then
        local npc = xtech_npc_getByPermID(targetId)
        if npc and npc.Active then
            -- 精确操作之前找到的那个 NPC
        end
    end
end
```

### 4.2 玩家操作
```lua
player = xtech_player_get(num)                -- 获取第 num 个玩家（1-based）
count = xtech_player_count()                  -- 玩家总数
xtech_player_filterToBig(player)              -- 强制大形态
xtech_player_filterToSmall(player)            -- 强制小形态
xtech_player_filterToFire(player)             -- 强制火形态
xtech_player_filterMount(player)              -- 移除坐骑
xtech_player_filterReservePowerup(player)     -- 清除备用道具
xtech_player_cycleRight(player)               -- 循环到下一个角色
xtech_player_cycleLeft(player)                -- 循环到上一个角色
xtech_player_infiniteFlying(player)           -- 无限飞行
isDown = xtech_player_pressingDown(player)    -- 是否按下
isUp = xtech_player_pressingUp(player)
isLeft = xtech_player_pressingLeft(player)
isRight = xtech_player_pressingRight(player)
isJump = xtech_player_pressingJump(player)
isRun = xtech_player_pressingRun(player)
isSEL = xtech_player_pressingSEL(player)
yes = xtech_player_isHoldingSpriteType(p, id) -- 检查玩家手持指定类型
yes = xtech_player_usesHearts(player)         -- 检查玩家是否使用心形生命
xtech_player_memSet(offset, value, op, ftype) -- 内存级玩家属性修改
```

### 4.3 图层操作
```lua
layer = xtech_layer_get(index)                -- 获取图层
count = xtech_layer_count()                   -- 图层总数
xtech_layer_setXSpeed(layer, speed)           -- 设置水平速度
xtech_layer_setYSpeed(layer, speed)           -- 设置垂直速度
xtech_layer_stop(layer)                       -- 停止图层移动
```

### 4.4 方块操作
```lua
block = xtech_block_get(index)                -- 获取方块
count = xtech_block_count()                   -- 方块总数
xtech_block_setAll(type1, type2)              -- 批量设置方块类型
xtech_block_swapAll(type1, type2)             -- 批量交换方块类型
xtech_block_showAll(type)                     -- 批量显示方块
xtech_block_hideAll(type)                     -- 批量隐藏方块
yes = xtech_block_isPlayerTouchingType(type, collision, player)
-- P2 新增：精确对象操作
block = xtech_block_getByPermID(permId)       -- 通过永久 ID 精确获取方块
xtech_block_forEach(type, function(block)     -- 遍历所有匹配方块执行回调
    block.Hidden = false
end)
```

### 4.5 BGO（背景对象）操作 ✅ P1 新增
```lua
bgo = xtech_bgo_get(index)                    -- 获取第 index 个 BGO（1-based）
count = xtech_bgo_count()                     -- BGO 总数
-- P2 新增：精确对象操作
bgo = xtech_bgo_getByPermID(permId)           -- 通过永久 ID 精确获取 BGO
xtech_bgo_forEach(type, function(bgo)         -- 遍历所有匹配 BGO 执行回调
    bgo.Hidden = true
end)
-- BGO 字段: Type, Hidden, Layer, SortPriority, Location (SpeedlessLocation)
-- BGO 方法: getPermID() 返回此 BGO 的 1-based 永久 ID
```

### 4.6 液体操作 ✅ P1 新增
```lua
liquid = xtech_liquid_get(index)              -- 获取第 index 个液体（0-based）
count = xtech_liquid_count()                  -- 液体总数
liquid = xtech_liquid_getByPermID(permId)     -- 通过永久 ID 精确获取液体
-- Liquid 方法: getPermID() 返回此 Liquid 的 0-based 永久 ID
-- Liquid 字段: Location (SpeedlessLocation), Type (PHYSID), Hidden, Layer
```

### 4.7 传送门操作 ✅ P1 新增
```lua
warp = xtech_warp_get(index)                  -- 获取第 index 个传送门（1-based）
count = xtech_warp_count()                    -- 传送门总数
warp = xtech_warp_getByPermID(permId)         -- 通过永久 ID 精确获取传送门
-- Warp 方法: getPermID() 返回此 Warp 的 1-based 永久 ID
-- Warp 字段: Entrance, Exit, Locked, WarpNPC, NoYoshi, Hidden, Stars, Effect,
--            LevelWarp, LevelEnt, Direction, Direction2, MapWarp, MapX, MapY,
--            curStars, twoWay, noPrintStars, noEntranceScene, cannonExit,
--            cannonExitSpeed, stoodRequired, eventEnter, eventExit,
--            StarsMsg, transitEffect, Layer
```

### 4.8 Section（区域）操作 ✅ P1 新增
```lua
section = xtech_section_get(index)            -- 获取 Section 0-21 的位置（SpeedlessLocation_t）
count = xtech_section_count()                 -- Section 数量
bg = xtech_section_getBackground(index)       -- 获取 Section 背景 ID
xtech_section_setBackground(index, bgId)      -- 设置 Section 背景 ID
music = xtech_section_getMusic(index)         -- 获取 Section 音乐 ID
xtech_section_setMusic(index, musicId)        -- 设置 Section 音乐 ID
file = xtech_section_getMusicFile(index)      -- 获取 Section 自定义音乐文件
xtech_section_setMusicFile(index, filename)   -- 设置 Section 自定义音乐文件
b = xtech_section_getOffScreenExit(index)     -- 获取是否允许屏幕外退出
xtech_section_setOffScreenExit(index, bool)   -- 设置是否允许屏幕外退出
```

**TeaScript Section 映射:**
- `Section(index).Width` → `section.Width`
- `Section(index).Height` → `section.Height`
- `Section(index).X` → `section.X`
- `Section(index).Y` → `section.Y`

### 4.9 音频
```lua
xtech_audio_playSFX(index, loops, volume)     -- 播放内置音效
xtech_audio_playSFXExt(filename, loops, vol)  -- 播放自定义音效（关卡目录）
xtech_audio_stopSFXExt(filename)              -- 停止自定义音效
xtech_audio_preloadSFXExt(filename)           -- 预加载自定义音效
xtech_audio_playMusic(section, fadeInMs)      -- 播放 section 音乐
xtech_audio_playMusicFile(filename, fadeInMs) -- 播放自定义音乐
xtech_audio_setMusic(section, musicID, file)  -- 设置 section 音乐
```

### 4.10 精灵系统
```lua
-- 图片管理
xtech_sprite_loadImage(filename, code, transColor)  -- 加载图片并指定透明色
xtech_sprite_loadImageSimple(filename, code)        -- 加载图片（默认透明色）
img = xtech_sprite_getImage(code)                    -- 获取已加载的图片
xtech_sprite_deleteImage(code)                       -- 删除图片资源

-- 精灵放置
spr = xtech_sprite_place(type, imgCode, x, y, time) -- 在关卡中放置精灵
spr = xtech_sprite_placeExt(type, imgCode, x, y, time, xspd, yspd, spawned)

-- 精灵管理
count = xtech_sprite_count()                         -- 所有活跃精灵数
xtech_sprite_clearAll()                              -- 清除所有精灵
xtech_sprite_clearByCode(imgCode)                    -- 清除指定图片的精灵
xtech_sprite_addBlueprint(name, spr)                 -- 注册精灵蓝图
newSpr = xtech_sprite_copyFromBlueprint(name)        -- 复制蓝图创建新精灵
```

### 4.11 HUD / 渲染
```lua
xtech_hud_showText(text, x, y, font)          -- 在屏幕坐标显示文字
xtech_hud_showLevelName(x, y, font)           -- 显示关卡名
xtech_hud_showLevelFile(x, y, font)           -- 显示文件名
xtech_hud_debugPrint(text)                    -- 输出调试信息到日志
```

### 4.12 变量系统
```lua
value = xtech_var_get(name)                   -- 读取变量值
exists = xtech_var_exists(name)               -- 检查变量是否存在
ok = xtech_var_operation(name, value, op)     -- 对变量进行算术运算
```

### 4.13 事件系统
```lua
xtech_event_trigger(section, eventId)         -- 触发指定 section 的事件
xtech_event_triggerByName(eventName)          -- 按名称触发事件
xtech_event_cancelByName(eventName)           -- 按名称取消事件
```

### 4.14 异步/延迟
```lua
xtech_misc_wait(callback, frames)             -- 延迟 frames 帧后执行回调
```

### 4.15 杂项
```lua
frame = xtech_misc_getFrame()                 -- 当前帧计数
section = xtech_misc_getSection(player)       -- 获取玩家所在 section
xtech_misc_showMsg(text)                      -- 显示游戏内消息框（普通样式）
xtech_misc_showMsgInfo(text)                  -- 显示消息框（信息样式，有标题栏）
xtech_misc_showMsgWarn(text)                  -- 显示消息框（警告样式，黄色）
xtech_misc_cheat(cheatString)                 -- 执行作弊码字符串
xtech_misc_log(message)                       -- 记录 INFO 级别日志
xtech_misc_logWarn(message)                   -- 记录 WARNING 级别日志
xtech_misc_logDebug(message)                  -- 记录 DEBUG 级别日志
```

**ShowMsg 说明：** 这是游戏内的消息框（不是 Windows 弹窗）。调用后会暂停游戏并显示文本，玩家按跳跃键关闭。三种样式对应：
- `showMsg` — MESSAGE_TYPE_NORMAL，无标题栏
- `showMsgInfo` — MESSAGE_TYPE_SYS_INFO，有标题栏
- `showMsgWarn` — MESSAGE_TYPE_SYS_WARNING，黄色标题栏

### 4.16 Sysval 系统（全局游戏状态）✅ P3 新增
```lua
-- 玩家/分数
lives = xtech_sysval_getLives()               -- 当前生命数
xtech_sysval_setLives(5)                      -- 设置生命数
coins = xtech_sysval_getCoins()               -- 当前金币数
xtech_sysval_setCoins(50)                     -- 设置金币数
score = xtech_sysval_getScore()               -- 当前分数
xtech_sysval_setScore(10000)                  -- 设置分数
-- 相机
x = xtech_sysval_getScreenX(1)                -- 玩家1 屏幕 X 坐标
y = xtech_sysval_getScreenY(1)                -- 玩家1 屏幕 Y 坐标
-- 游戏状态
show = xtech_sysval_getShowHud()              -- 是否显示 HUD
xtech_sysval_setShowHud(false)                -- 隐藏 HUD
battle = xtech_sysval_getBattleMode()         -- 是否为对战模式
ending = xtech_sysval_getEndLevel()            -- 是否正在结束关卡
xtech_sysval_setEndLevel(true)                -- 立即结束关卡
macro = xtech_sysval_getLevelMacro()          -- 关卡结束宏类型
counter = xtech_sysval_getLevelMacroCounter() -- 结束宏计数器
time = xtech_sysval_getGameTime()             -- 游戏运行总帧数
```

### 4.17 命名定时器（TCreate/TClear 等价）✅ P3 新增
```lua
-- 创建可取消的命名定时器
xtech_timer_create("myTimer", function()
    xtech_audio_playSFX(CONST_SFX_Do, 0, 128)
end, 300)  -- 300 帧后触发

-- 提前取消
xtech_timer_cancel("myTimer")

-- 等待（匿名定时器，不可取消）
xtech_misc_wait(function()
    xtech_hud_showText("Done!", 400, 300, 3)
end, 180)

-- 清除所有定时器
xtech_timer_clearAll()
```

**与 TeaScript 对比：**
- TeaScript `Call TCreate(eventname, delay)` → `xtech_timer_create(name, callback, frames)`
- TeaScript `Call TClear(0, eventname)` → `xtech_timer_cancel(name)`
- TeaScript `Call TClear(1, 0)` → `xtech_timer_clearAll()`
- TeaScript `Call Sleep(delay)` → `xtech_misc_wait(callback, frames)`（异步版本）

### 4.18 特效操作（FXCreate 等价）✅ P3 新增
```lua
-- 创建特效
eff = xtech_effect_create(CONST_EFF_SMOKE_S3, 400, 300, 1, false)
if eff then
    eff.Location.SpeedX = 2     -- 可继续修改属性
end

-- 访问和遍历
eff = xtech_effect_get(index)                 -- 获取第 index 个特效（1-based）
count = xtech_effect_count()                  -- 特效总数
xtech_effect_kill(index)                      -- 删除指定特效
```

---

## 5. 常量定义

### 5.1 方向
`CONST_DIR_UP(1)`, `CONST_DIR_RIGHT(2)`, `CONST_DIR_DOWN(3)`, `CONST_DIR_LEFT(4)`

### 5.2 运算
`CONST_OP_ASSIGN(0)`, `CONST_OP_ADD(1)`, `CONST_OP_SUB(2)`, `CONST_OP_MULT(3)`, `CONST_OP_DIV(4)`, `CONST_OP_XOR(5)`, `CONST_OP_ABS(6)`

### 5.3 比较
`CONST_CMPT_EQUALS(0)`, `CONST_CMPT_GREATER(1)`, `CONST_CMPT_LESS(2)`, `CONST_CMPT_NOTEQ(3)`

### 5.4 优先级
`CONST_PRI_LOW(0)`, `CONST_PRI_MID(1)`, `CONST_PRI_HIGH(2)`

### 5.5 字段类型（内存操作）
`CONST_FT_BYTE(1)`, `CONST_FT_WORD(2)`, `CONST_FT_DWORD(3)`, `CONST_FT_FLOAT(4)`, `CONST_FT_DFLOAT(5)`

### 5.6 玩家形态
`CONST_PLAYER_SMALL(1)`, `CONST_PLAYER_SUPER(2)`, `CONST_PLAYER_FIRE(3)`, `CONST_PLAYER_RACCOON(4)`, `CONST_PLAYER_TANOOKI(5)`, `CONST_PLAYER_HAMMER(6)`

### 5.7 坐骑类型
`CONST_MOUNT_NONE(0)`, `CONST_MOUNT_BOOT(1)`, `CONST_MOUNT_CLOWN_CAR(2)`, `CONST_MOUNT_YOSHI(3)`

### 5.8 精灵类型
`CONST_SPRITE_CUSTOM(0)`, `CONST_SPRITE_STATIC(1)`, `CONST_SPRITE_NORMAL(2)`, `CONST_SPRITE_ITEM(3)`, `CONST_SPRITE_BAR(4)`, `CONST_SPRITE_PHANTO(5)`

### 5.9 蓝图挂载类型
`CONST_BPAT_BEHAVIOR(0)`, `CONST_BPAT_DRAW(1)`, `CONST_BPAT_BIRTH(2)`, `CONST_BPAT_DEATH(3)`

### 5.10 碰撞类型
`CONST_COLTYPE_NONE(0)`, `CONST_COLTYPE_LEFT(1)`, `CONST_COLTYPE_RIGHT(2)`, `CONST_COLTYPE_TOP(3)`, `CONST_COLTYPE_BOT(4)`

### 5.11 NPC ID（完整 306 个条目）

所有 [npc_id.h](src/npc_id.h) 中定义的 NPCID 枚举值均已暴露：
`CONST_NPC_NULL(0)` 到 `CONST_NPC_FLAG_EXIT(306)`，包括所有 SMBX 1.3 自定义 NPC（293-300）和 TheXTech 独占 NPC（301-306）。

### 5.12 SFX ID（完整 106 个条目）

所有 [sound.h](src/sound.h) 中定义的音效常量均已暴露：
`CONST_SFX_Jump(1)` 到 `CONST_SFX_FlagExit(106)`，覆盖所有 SMBX64 音效和 TheXTech 扩展音效。

### 5.13 EFFID（完整 152 个特效 ID）✅ P3 新增

所有 [eff_id.h](src/eff_id.h) 中定义的 EFFID 枚举值均已暴露：
`CONST_EFF_SMOKE_S3_CENTER(-10)` 到 `CONST_EFF_GENERIC_NPC_SQUISH(150)`，覆盖所有特效类型。

---

## 6. 配置集成

**文件：** [src/config.h](src/config.h)

新增配置项 `lua_enable_engine`：
- 默认值：`true`
- 兼容类：`critical_update`
- 作用域：`CreatorFile`（每个关卡文件可配置）
- 与 `luna_enable_engine` 独立控制

---

## 7. 游戏循环钩子

在以下位置添加了 `#ifdef ENABLE_XTECH_LUA` 保护的 Lua API 调用：

| 文件 | 钩子位置 | 调用 |
|------|----------|------|
| [src/main.cpp](src/main.cpp) | 游戏启动/退出 | `xtech_lua_init()`, `xtech_lua_quit()` |
| [src/game_main.cpp](src/game_main.cpp) | 关卡加载（3处）| `xtech_lua_load()` |
| [src/game_main.cpp](src/game_main.cpp) | 关卡重置（6处）| `xtech_lua_reset()` |
| [src/main/game_loop.cpp](src/main/game_loop.cpp) | 每帧循环 | `xtech_lua_loop()` |
| [src/main/menu_loop.cpp](src/main/menu_loop.cpp) | 菜单循环 | `xtech_lua_loop()` |
| [src/main/outro_loop.cpp](src/main/outro_loop.cpp) | 结局循环 | `xtech_lua_loop()` |
| [src/graphics/gfx_update.cpp](src/graphics/gfx_update.cpp) | 渲染前/后/图层/HUD | `xtech_lua_renderStart/End/Render/RenderHud()` |

---

## 8. 与 SMBx TeaScript 1.4.5 对比

### 8.1 已覆盖功能（~85%）

| TeaScript 功能类别 | 状态 | Lua 实现方式 |
|---|---|---|
| 变量系统 (val/gval/sysval) | ✅ 已覆盖 | xtech_var_* 函数 + Lua 原生变量 |
| 数学表达式和函数 | ✅ 已覆盖 | Lua 内置 math 库 |
| 逻辑表达式 | ✅ 已覆盖 | Lua 原生逻辑运算符 |
| 控制流 (If/For/Do/Select) | ✅ 已覆盖 | Lua 原生控制流 |
| NPC 属性读写 | ✅ 已扩展 | NPC 类绑定（含 P0 新增字段） |
| Player 属性读写 | ✅ 已扩展 | Player 类绑定（含 P0 新增字段） |
| Block 属性读写 | ✅ 已扩展 | Block 类绑定（含 P0 新增字段） |
| BGO 属性读写 | ✅ P1 新增 | BGO 类绑定 |
| Liquid 属性读写 | ✅ P1 新增 | Liquid 类绑定 |
| Warp 属性读写 | ✅ P1 新增 | Warp 类绑定 |
| Section 属性读写 | ✅ P1 新增 | xtech_section_* 函数 |
| 对象永久 ID 获取 | ✅ P2 新增 | getPermID() 方法（所有对象类） |
| 按永久 ID 精确获取 | ✅ P2 新增 | xtech_*_getByPermID() 函数 |
| 批量遍历操作 | ✅ P2 新增 | xtech_*_forEach() 回调遍历 |
| 对象命名 (getIDByName) | ⬜ 未实现 | 可用 getPermID + 自定义 table 替代 |
| 自定义变量槽 (Ivala/b/c) | ⬜ 部分 | 可用 Special2/3/4 字段代替 |
| Layer 操作 | ✅ 已覆盖 | Layer 类绑定 + xtech_layer_* 函数 |
| 音频 (AudioSet) | ✅ 已覆盖 | xtech_audio_* 函数 |
| HUD 文字 | ✅ 已覆盖 | xtech_hud_* 函数 |
| 事件系统 | ✅ 已覆盖 | xtech_event_* 函数 |
| 延迟/定时 | ✅ 已覆盖 | xtech_misc_wait（异步回调式） |
| 精灵/位图 | ✅ 已覆盖 | xtech_sprite_* 函数 |
| NPC 创建 (NCreate) | ✅ P3 新增 | `xtech_npc_create(type, x, y, xspd, yspd)` |
| NPC 删除 (NKill) | ✅ 已覆盖 | xtech_npc_kill |
| 特效创建 (FXCreate) | ✅ P3 新增 | `xtech_effect_create()` + Effect_t 类绑定 |
| Sysval 系统 | ✅ P3 新增 | `xtech_sysval_get*/set*` 系列函数 |
| 命名定时器 (TCreate/TClear) | ✅ P3 新增 | `xtech_timer_create/cancel/clearAll` |
| 延迟执行 (Sleep) | ✅ 已覆盖 | `xtech_misc_wait(callback, frames)` |
| 脚本互调 (EXEScript) | ⬜ 未实现 | 可用 Lua dofile/require 替代 |
| 消息框 (ShowMsg) | ✅ P3 新增 | `xtech_misc_showMsg(text)` + info/warn 变体 |
| 图层旋转 (LSpin) | ⬜ 未实现 | 需底层支持 |
| 对象命名 (getIDByName) | ⬜ 未实现 | 可用 getPermID + 自定义 table 替代 |
| BGP 属性 | ⬜ 未实现 | 需渲染管线重构（工作量较大） |
| MIDI (PlayNote) | ❌ 跳过 | 过时技术，不适合现代平台 |

### 8.2 设计差异说明

| 方面 | TeaScript (VB6) | Lua 实现 |
|---|---|---|
| 变量系统 | val(a)=, gval(b)= 宏调用 | Lua 原生变量赋值 |
| 延迟 | Sleep 阻塞脚本 | xtech_misc_wait 异步回调 |
| 迭代器 | ItrCreate/ItrNext 命令式 API | Lua for 循环 + 条件判断 |
| 位图/HUD | HUDSet 多功能命令（type 切换） | 独立 API 函数 |
| 对象命名 | getIDByName / Name 属性 | 使用数组索引访问 |
| 语法风格 | VB6 With/Goto/Select Case | Lua 惯用语法 |
| 阻塞 vs 异步 | Sleep 阻塞整个脚本 | 协程式异步（更安全） |

### 8.3 无法/不值得复刻的功能

| 功能 | 原因 |
|---|---|
| PlayNote (MIDI) | 过时技术，现代平台无意义 |
| Sleep 阻塞语义 | 阻塞游戏循环是糟糕设计，回调式更好 |
| HUDSet 多功能命令 | API 设计本身有缺陷，已用独立函数替代 |
| With 语法 | Lua 用局部变量引用即可 |
| Goto/Gosub 跨函数 | 结构化编程优于跳转 |
| 编译错误码 0-38 | 不同引擎，无意义 |
| scflash.png 依赖 | SMBX 特有格式 |
| Forecolor (NPC/BGO) | TheXTech 无对应字段 |

---

## 9. 修改的文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `script/include/xtech_lua_main.h` | 编辑 | 新增 8 个 API 声明 |
| `script/src/xtech_lua_main.cpp` | 重写 | 完整 Lua 生命周期 + 延迟调用集成（~270行）|
| `script/src/xtech_lua_bindings.h` | 编辑 | 新增 process/clear 函数声明 |
| `script/src/xtech_lua_bindings.cpp` | 重写 | 全部绑定 + P0+P1+P2+P3 扩展（~2535行）|
| `script/CMakeLists.txt` | 编辑 | 添加 `xtech_lua_bindings.cpp` |
| `src/config.h` | 编辑 | 新增 `lua_enable_engine` 配置项 |
| `src/game_main.cpp` | 编辑 | 9处 Lua 钩子 |
| `src/main/game_loop.cpp` | 编辑 | `xtech_lua_loop()` 钩子 |
| `src/main/menu_loop.cpp` | 编辑 | `xtech_lua_loop()` 钩子 |
| `src/main/outro_loop.cpp` | 编辑 | `xtech_lua_loop()` 钩子 |
| `src/graphics/gfx_update.cpp` | 编辑 | 4处渲染钩子 |

**总计：** ~2535 行新代码/修改，分布在 11 个文件中。新增 122 个 API 函数、306 个 NPC ID 常量、106 个 SFX 常量、152 个 EFFID 常量。

---

## 10. 未实现 / 后续扩展

### 已实现（P2）
- [x] **对象永久 ID 系统** — 所有对象类的 `getPermID()` 方法
- [x] **按永久 ID 精确获取** — `xtech_npc_getByPermID()` 等 5 个函数
- [x] **批量遍历回调** — `xtech_npc_forEach()`、`xtech_block_forEach()`、`xtech_bgo_forEach()`

### 已实现（P3）
- [x] **TeaScript NCreate 直接绑定** — `xtech_npc_create(type, x, y, xspd, yspd)`
- [x] **Sysval 系统完整覆盖** — `xtech_sysval_get*/set*` 系列（生命/金币/分数/相机/HUD 等）
- [x] **命名定时器 (TCreate/TClear 等价)** — `xtech_timer_create/cancel/clearAll`
- [x] **Effect_t 绑定 (FXCreate 等价)** — `xtech_effect_create/get/count/kill` + 152 EFFID 常量

### 中优先级
- [ ] **lunadll.txt 兼容层** — 将旧格式自动转换为 Lua 调用
- [ ] **Lua 脚本热加载** — 无需重启关卡重新加载脚本
- [ ] **完整的错误报告 UI** — 在游戏内显示 Lua 错误而非仅写入日志

### 低优先级
- [ ] **对象命名系统** — TeaScript `getIDByName` / `Name` 属性等价功能
- [ ] **BGP（背景图片）属性** — 背景分层/偏移控制（需渲染管线重构）
- [ ] **Luau 沙盒集成** — 将已有的 Luau 测试代码与游戏 API 集成
- [ ] **网络/多人游戏 API** — 暴露在线模式相关功能
- [ ] **脚本调试接口** — 断点、单步执行等
- [ ] **Lua 脚本的 IDE 支持** — 自动补全定义文件生成

### 跳过（架构/设计不兼容）
- [ ] **PlayNote** — MIDI 音符操作，过时技术
- [ ] **阻塞式 Sleep** — 异步回调设计更优
- [ ] **HUDSet 多功能 API** — 已拆分为独立函数
- [ ] **scflash.png 位图系统** — SMBX 特有格式

---

## 11. 示例用法

### 基础示例：关卡入口脚本

在关卡目录（`.lvl` 文件旁）创建 `level.lua`：

```lua
-- 关卡加载时执行一次
function onLoad()
    xtech_misc_log("Level loaded via Lua!")

    local player = xtech_player_get(1)
    if player then
        xtech_misc_log(string.format("Player state: %d, hearts: %d",
            player.State, player.Hearts))
    end
end

-- 每帧执行
local frameCount = 0
function onLoop()
    frameCount = frameCount + 1

    if frameCount % 60 == 0 then
        local player = xtech_player_get(1)
        if player then
            xtech_hud_showText(
                string.format("F:%d P:(%d,%d)",
                    frameCount,
                    math.floor(player.Location.X),
                    math.floor(player.Location.Y)),
                100, 30, 3)
        end
    end
end

function onRenderHud(Z)
    xtech_hud_showText("Lua Active!", 10, 600, 3)
end
```

### 精确对象操作示例 ✅ P2 新增

```lua
-- 模式1：保存永久 ID，跨帧精确找回
local bossDoorId = nil

function onLoad()
    -- 找到唯一的 Boss 门 NPC
    xtech_npc_forEach(CONST_NPC_LOCK_DOOR, 0, function(npc)
        bossDoorId = npc:getPermID()   -- 记录永久 ID
        npc:setStuck(true)             -- 先锁住
    end)
end

function onLoop()
    if bossDoorId then
        local door = xtech_npc_getByPermID(bossDoorId)
        if door and door.Active then
            -- 检查玩家是否有钥匙
            local player = xtech_player_get(1)
            if player and player.HasKey then
                door:setStuck(false)   -- 解锁
                xtech_hud_showText("Door unlocked!", 400, 300, 3)
            end
        else
            bossDoorId = nil  -- NPC 已消失
        end
    end
end
```

```lua
-- 模式2：forEach 批量操作（更优雅的遍历）
function onLoad()
    -- 将所有 Goomba 设置为朝左走
    xtech_npc_forEach(CONST_NPC_FODDER_S3, 0, function(npc)
        npc.Direction = -1
    end)

    -- 隐藏所有类型为 188 的方块
    xtech_block_forEach(188, function(block)
        block.Hidden = true
    end)

    -- 显示所有 BGO 类型 50
    xtech_bgo_forEach(50, function(bgo)
        bgo.Hidden = false
    end)
end
```

```lua
-- 模式3：手动遍历 + 条件筛选 + 永久 ID
local targets = {}  -- 记录所有符合条件的 NPC 的永久 ID

function onLoad()
    -- 记录 section 1 中所有踩怪（Goomba）的永久 ID
    xtech_npc_forEach(CONST_NPC_FODDER_S3, 1, function(npc)
        table.insert(targets, npc:getPermID())
    end)
    xtech_misc_log("Found " .. #targets .. " goombas in section 1")
end

function onLoop()
    -- 让所有记录的目标朝玩家跳跃
    local player = xtech_player_get(1)
    if not player then return end

    for _, permId in ipairs(targets) do
        local npc = xtech_npc_getByPermID(permId)
        if npc and npc.Active then
            if player.Location.Y < npc.Location.Y and
               math.abs(player.Location.X - npc.Location.X) < 80 then
                npc.Location.SpeedY = -6
            end
        end
    end
end
```

### Sysval + NCreate + 特效 + 定时器 综合示例 ✅ P3 新增

```lua
function onLoad()
    -- 设置初始状态
    xtech_sysval_setLives(5)
    xtech_sysval_setCoins(0)
    xtech_sysval_setShowHud(true)
    xtech_misc_log("Level started with " .. xtech_sysval_getLives() .. " lives")

    -- 创建一个自定义 NPC
    local npc = xtech_npc_create(CONST_NPC_FODDER_S3, 400, 300, -2, 0)
    if npc then
        npc.Direction = -1
        npc.Special = 999  -- 自定义标记
        xtech_misc_log("Created NPC with permID: " .. npc:getPermID())
    end

    -- 创建烟雾特效
    local eff = xtech_effect_create(CONST_EFF_SMOKE_S3, 400, 300, 1, false)
    if eff then
        eff.Location.SpeedX = 2
    end

    -- 3秒后播放音效并创建爆炸
    xtech_timer_create("delayedBoom", function()
        xtech_audio_playSFX(CONST_SFX_SMExplosion, 0, 128)
        xtech_effect_create(CONST_EFF_BOMB_S3_EXPLODE, 500, 300, 1, false)

        -- 2秒后再切换音乐
        xtech_timer_create("musicSwitch", function()
            xtech_section_setMusic(0, 5)
        end, 120)
    end, 180)

    -- 任务触发时可提前取消
    -- xtech_timer_cancel("delayedBoom")
end

function onLoop()
    -- 金币到达100时触发1UP
    if xtech_sysval_getCoins() >= 100 then
        xtech_sysval_setCoins(0)
        xtech_sysval_setLives(xtech_sysval_getLives() + 1)
        xtech_audio_playSFX(CONST_SFX_1up, 0, 128)
        xtech_hud_showText("1UP!", 400, 300, 3)
    end
end
```

### BGO / Liquid / Warp 遍历示例 ✅ 新增

```lua
-- 遍历所有 BGO 并修改
function onLoad()
    for i = 1, xtech_bgo_count() do
        local bgo = xtech_bgo_get(i)
        if bgo and bgo.Type == 100 then  -- 特定类型
            bgo.Hidden = true            -- 隐藏
        end
    end
end

-- 检查传送门状态
function onLoop()
    local player = xtech_player_get(1)
    if not player then return end

    for i = 1, xtech_warp_count() do
        local warp = xtech_warp_get(i)
        if warp and not warp.Hidden then
            -- 检查玩家是否接近传送门入口
            local dx = warp.Entrance.X - player.Location.X
            local dy = warp.Entrance.Y - player.Location.Y
            if math.abs(dx) < 32 and math.abs(dy) < 32 then
                xtech_hud_showText("Near warp " .. i, 100, 100, 3)
            end
        end
    end
end
```

### 精灵示例

```lua
function onLoad()
    -- 加载自定义图片
    xtech_sprite_loadImage("my_sprite.png", 1000, 0xFF00DC)

    -- 放置一个静态 HUD 精灵
    local spr = xtech_sprite_place(CONST_SPRITE_STATIC, 1000, 400, 300, 0)
    if spr then
        spr.Visible = true
        spr.StaticScreenPos = true
    end
end
```

### 异步延迟示例

```lua
function onLoad()
    -- 3秒后播放音效
    xtech_misc_wait(function()
        xtech_audio_playSFX(CONST_SFX_GotItem, 0, 128)
    end, 180)

    -- 延迟序列：每隔 1 秒显示倒计时
    local function countdown(n)
        if n > 0 then
            xtech_hud_showText(tostring(n), 400, 300, 3)
            xtech_misc_wait(function()
                countdown(n - 1)
            end, 60)
        else
            xtech_hud_showText("Go!", 400, 300, 3)
            xtech_audio_playSFX(CONST_SFX_Do, 0, 128)
        end
    end
    countdown(5)
end
```

### 完整 NPC 操控示例

```lua
function onLoop()
    -- 找到所有 Goomba (NPCID 1) 并让它们朝玩家跳跃
    local player = xtech_player_get(1)
    if not player then return end

    for i = 1, xtech_npc_count() do
        local npc = xtech_npc_get(i)
        if npc and npc.Active and npc.Type == CONST_NPC_FODDER_S3 then
            -- NPC 朝玩家方向
            if player.Location.X > npc.Location.X then
                npc.Direction = 1   -- 右
            else
                npc.Direction = -1  -- 左
            end
            -- 如果玩家在上方且接近，让 NPC 跳
            if player.Location.Y < npc.Location.Y and
               math.abs(player.Location.X - npc.Location.X) < 100 then
                npc.Location.SpeedY = -8
            end
        end
    end
end
```

---

## 12. 编译说明

### 使用一键构建脚本（推荐）

项目根目录下的 `build-win64.ps1` / `build-win64.cmd` 已配置为默认启用 Lua 脚本支持：

```powershell
# 双击 build-win64.cmd 或运行：
.\build-win64.ps1                     # MinGW64 MinSizeRel（带 Lua）
.\build-win64.ps1 -Compiler VS2019    # Visual Studio 2019（带 Lua）
.\build-win64.ps1 -Clean              # 清理重编
.\build-win64.ps1 -BuildType Debug    # Debug 构建
```

### 使用 CMake 手动配置

```bash
cmake -DTHEXTECH_ENABLE_LUA=ON ..
```

禁用以节省编译时间：
```bash
cmake -DTHEXTECH_ENABLE_LUA=OFF ..
```

该选项将：
1. 编译 LuaJIT 2.1（已内置在 `3rdparty/`）
2. 编译 luabind（已内置在 `3rdparty/`）
3. 编译 `XTechLua` 库（`script/` 目录）
4. 定义 `ENABLE_XTECH_LUA` 预处理器宏
5. 启用所有 `#ifdef ENABLE_XTECH_LUA` 保护的代码路径
