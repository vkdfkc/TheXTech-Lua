# LuaJIT 全量化游戏脚本接入 — 实施报告

**日期：** 2026-07-25（修订 — 加入 game.lua 存档系统 + 沙箱架构 + 世界地图支持）
**项目：** TheXTech
**参考文档：** SMBx_Scripting_Help_1.4.5.rtf（TeaScript API）

---

## 概述

本文档记录了为 TheXTech 引擎实现完整 LuaJIT 游戏脚本支持的代码变更。实现使用项目已有的 LuaJIT 2.1 + luabind 基础设施，通过 `THEXTECH_ENABLE_LUA=ON` CMake 选项启用。

**设计原则：** 以 TeaScript 文档作为功能清单，用 Lua 惯用方式暴露 TheXTech 已存在的游戏数据，不复刻 TeaScript 的 VB6 风格 API 表面。

---

## 1. Lua VM 生命周期管理

**文件：** [script/include/xtech_lua_main.h](script/include/xtech_lua_main.h)、[script/src/xtech_lua_main.cpp](script/src/xtech_lua_main.cpp)

### 1.1 架构概述

Lua VM 在游戏启动时创建一次，**不再随关卡进出销毁/重建**。通过关卡沙箱机制实现脚本隔离。

```
xtech_lua_init()        ← 游戏启动时，创建 VM + 注册绑定（仅一次）
xtech_lua_loadGame()    ← 加载 game.lua（仅一次，首次调用时）
xtech_lua_loadLevel()   ← 沙箱加载关卡脚本（每次进入关卡）
xtech_lua_unloadLevel() ← 卸关卡沙箱 + 清理函数引用（退出关卡）
xtech_lua_reset()       ← 同 unloadLevel（不再 lua_close）
xtech_lua_quit()        ← 游戏退出时，最终销毁 VM
```

### 1.2 API 函数

| 函数 | 说明 |
|------|------|
| `xtech_lua_init()` | 创建 LuaJIT VM，打开标准库，注册所有游戏绑定 |
| `xtech_lua_loadGame()` | 从 Episode 目录加载 `game.lua`（仅一次，自动在首次需要时调用） |
| `xtech_lua_loadLevel()` | 从关卡目录沙箱加载 `level.lua`（或 `lunadll.lua`），检索钩子函数 |
| `xtech_lua_unloadLevel()` | 清理关卡函数引用和沙箱（**不销毁 VM**） |
| `xtech_lua_reset()` | 同 `unloadLevel`（兼容旧调用点） |
| `xtech_lua_load()` | 兼容函数 = `loadGame` + `loadLevel` |
| `xtech_lua_loop()` | 处理异步延迟回调，调用 `onLoop` |
| `xtech_lua_worldMapRender()` | 调用 `OnWorldMapRender`（世界地图渲染钩子） |
| `xtech_lua_gameSave(dataPath)` | 调用 `OnGameSave` → 序列化 table → 写入 `save{id}.luadata` |
| `xtech_lua_gameLoad(jsonStr)` | 读取 JSON → 反序列化 → 调用 `OnGameLoad(table)` |
| `xtech_lua_getFunc(name)` | 查找 Lua 函数：**沙箱优先**，回退到 `_G`（事件系统用） |
| `xtech_lua_quit()` | 卸载所有状态，关闭 Lua VM |

### 1.3 脚本加载顺序

1. **启动时**：`xtech_lua_init()` 创建 VM
2. **首次需要时**：`xtech_lua_loadGame()` 从 Episode 目录加载 `game.lua`
3. **进入关卡时**：`xtech_lua_loadLevel()` 从关卡自定义目录沙箱加载 `level.lua` / `lunadll.lua`
4. **退出关卡时**：`xtech_lua_unloadLevel()` 清理关卡状态（保留 game.lua 的全局状态）

### 1.4 关卡沙箱机制

关卡脚本在**沙箱环境**中执行，实现脚本隔离：

```
Sandbox = {}
Sandbox_mt = {
    __index    = _G,              -- 读不到的去全局找（能读 CustomData、xtech_* API）
    __newindex = sandbox_proxy    -- 写：CustomData → _G，其余 → Sandbox（不污染全局）
}

level.lua chunk 以 Sandbox 为环境执行
```

**作用**：
- 关卡脚本可以访问所有全局 API 和 `game.lua` 定义的变量
- 关卡脚本定义的全局变量不会污染 `_G`（限在沙箱内）
- `CustomData` 是唯一可跨关卡写入的全局 table

**事件系统兼容**：`xtech_lua_getFunc(name)` 先查沙箱，未找到再查 `_G`，确保 `onLevelComplete` 等事件钩子在沙箱环境中正常工作。

### 1.5 全局脚本 game.lua

**路径**：Episode 目录下（如 `worlds/vk's world2/game.lua`）

**触发机制**：`xtech_lua_loadGame()` 在首次需要时自动加载（由 `loadLevel`、`gameSave`、`gameLoad`、`worldMapRender` 等入口点触发）。

### 1.6 所有 Lua 钩子函数

```lua
-- === game.lua（全局钩子） ===
function OnGameSave()                    -- 存档时触发，返回要持久化的 table
function OnGameLoad(data)                -- 读档时触发，参数为上次保存的 table
function OnWorldMapRender()              -- 世界地图每帧渲染

-- === level.lua（关卡钩子） ===
function onLoad()                        -- 关卡加载时调用一次
function onLoop()                        -- 每帧调用
function onLoopEnd()                     -- 关卡结束时调用一次
function onRenderStart()                 -- 每帧渲染前
function onRenderEnd()                   -- 每帧渲染后
function onRender(Z)                     -- 每层渲染（Z=屏幕层号）
function onRenderHud(Z, numScreens)      -- HUD 渲染
function onLevelComplete()               -- 关卡通关时触发

-- === 事件钩子（关卡脚本中定义，见第 8 章） ===
function onNPCDeath(permid, npcId, killer)
function onNPCHurt(permid, npcId, hitter, hitType)
-- ... 等 17 个系统事件
end
```

**错误处理：** 所有 Lua 函数调用都通过 try-catch 间接调用，异常被捕获并记录到日志，不会导致游戏崩溃。

---

## 1.7 CustomData 持久化系统

### 概述

`CustomData` 是一个全局 Lua table，由 `game.lua` 管理，用于跨关卡自定义数据持久化。

- **关卡脚本**：直接读写 `CustomData[FileName]`，无需关心存档机制
- **存档时**：C++ 调用 `OnGameSave()` → Lua 返回整个 `CustomData` table → 序列化为 JSON → 写入 `save{id}.luadata`
- **读档时**：从 `save{id}.luadata` 读取 JSON → 反序列化为 table → 调用 `OnGameLoad(table)`
- **新游戏时**：`OnGameLoad({})` 传入空 table

### game.lua 模板

```lua
-- game.lua（放在 Episode 目录下，如 worlds/vk's world2/game.lua）

CustomData = {}

-- 读档 / 新游戏时触发
function OnGameLoad(data)
    CustomData = data

    -- 预加载世界地图需要的图像 (filename, code, transColor)
    xtech_sprite_loadImage("graphics/star_full.png", 100, 0)
    xtech_sprite_loadImage("graphics/star_empty.png", 101, 0)
end

-- 存档时触发：返回需要持久化的 table
function OnGameSave()
    return CustomData
end
```

### 关卡脚本中使用 CustomData

```lua
-- level.lua

function onLoad()
    -- 读取本关的持久化数据
    local myData = CustomData[FileName] or {}
    if myData.starCollected then
        -- 大金币已收集，隐藏对应的 NPC
        xtech_npc_forEach(CONST_NPC_STAR_COLLECT, 0, function(npc)
            npc.Killed = 9
        end)
    end
end

function onLevelComplete()
    -- 保存本关的完成状态
    CustomData[FileName] = {
        starCollected = true,
        score = xtech_sysval_getScore(),
        completedAt = xtech_sysval_getGameTime()
    }
end
```

### 序列化支持的类型

number、string、boolean、嵌套 table、nil（序列化为 `null`）。存储格式为独立 JSON 文件 `save{id}.luadata`，与 `save{id}.savx` 同级。

---

## 1.8 世界地图 Lua 支持

### 钩子

`OnWorldMapRender()` — 世界地图每帧渲染时调用，定义在 `game.lua` 中。

### API

```lua
-- 获取当前玩家所在的关卡文件名（WorldPlayer[1].LevelIndex 对应的 WorldLevel[].FileName）
name = xtech_worldmap_getCurrentLevel()      -- string 或 "" (nil if not on a level)

-- 获取关卡在地图上的坐标（世界坐标，即 .lvl 文件中定义的位置）
pos = xtech_worldmap_getLevelScreenPos(name)  -- table {x=?, y=?} 或 nil
```

### worldmap_getLevelScreenPos 说明

返回值是一个 table，包含 `x` 和 `y` 两个字段（世界地图上的像素坐标）。注意这是世界坐标而非屏幕坐标——需要配合相机 API 转换为屏幕坐标用于绘制 HUD。

### 大地图 HUD 渲染示例

```lua
-- game.lua

local imgsLoaded = false

function OnGameLoad(data)
    CustomData = data
    if not imgsLoaded then
        xtech_sprite_loadImage("graphics/star_full.png", 100, 0)
        xtech_sprite_loadImage("graphics/star_empty.png", 101, 0)
        imgsLoaded = true
    end
end

function OnWorldMapRender()
    local levelName = xtech_worldmap_getCurrentLevel()
    if not levelName or levelName == "" then return end

    local pos = xtech_worldmap_getLevelScreenPos(levelName)
    if not pos then return end

    local data = CustomData[levelName]
    -- xtech_hud_showImage(imgCode, x, y, sx, sy, sw, sh)
    if data and data.starCollected then
        xtech_hud_showImage(100, pos.x, pos.y, 0, 0, 32, 32)
    else
        xtech_hud_showImage(101, pos.x, pos.y, 0, 0, 32, 32)
    end
end

function OnGameSave()
    return CustomData
end
```

---

## 2. 异步延迟回调支持

**文件：** [script/src/xtech_lua_bindings.cpp](script/src/xtech_lua_bindings.cpp)、[script/src/xtech_lua_main.cpp](script/src/xtech_lua_main.cpp)

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

```lua
-- 字段（全部可读写）
X, Y, Width, Height, SpeedX, SpeedY

-- 方法
luaX()   -- 返回高精度 X 坐标的整数部分（用于 Lua 整数运算）
luaY()   -- 返回高精度 Y 坐标的整数部分（用于 Lua 整数运算）
```

> `luaX()`/`luaY()` 将内部的 64-bit 定点数右移 32 位转换为 Lua 整数。适用于所有带 Location 的对象。

### 3.2 SpeedlessLocation_t（无速度位置）

```lua
-- 字段（全部可读写）
X, Y, Width, Height

-- 方法
luaX(), luaY()
```

用于 Background_t、Water_t、Warp_t 和 Section 的位置。

### 3.3 LunaRect（矩形区域）

```lua
left, top, right, bottom   -- 全部可读写
```

### 3.4 Hitbox（碰撞盒）

```lua
Left_off, Top_off, W, H, CollisionType   -- 全部可读写
```

### 3.5 LunaImage（加载的图片资源）

```lua
-- 方法
getWidth()     -- 图片宽度（像素）
getHeight()    -- 图片高度（像素）
getUID()       -- 唯一标识符
isLoaded()     -- 是否已成功加载
```

### 3.6 NPC_t（非玩家角色）

**直接读写字段:**

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
DefaultLocationX, DefaultLocationY   -- TeaScript: PrX, PrY（原始生成位置）
DefaultType                          -- TeaScript: 默认 NPC 类型
DefaultSpecial                       -- TeaScript: 默认附加数据
DefaultDirection                     -- TeaScript: 默认朝向
DefaultWings                         -- TeaScript: 默认翅膀类型
```

**事件/字符串索引：**

```
TriggerActivate     -- TeaScript: Activeevent（激活事件）
TriggerDeath        -- TeaScript: Deathevent（死亡事件）
TriggerTalk         -- TeaScript: Talkevent（对话事件）
TriggerLast         -- TeaScript: 最后 NPC 事件
Text                -- TeaScript: 对话文字索引
Layer               -- 所在图层索引
AttLayer            -- 附加图层
```

**P5 新增字段（精灵图帧偏移）：**

```
extx, exty          -- 精灵图帧偏移（以 GFX 宽高为单位），渲染时自动应用
```

> `extx` 偏移 `GFXSlot` 的计算结果：`src_x = (GFXSlot + extx) * frameWidth`，`exty` 偏移 `Frame`：`src_y = (Frame + exty) * frameHeight`。默认均为 0。

**位域成员（通过 getter/setter 方法访问）：**

```
getGenerator/setGenerator, getGeneratorActive/setGeneratorActive
getChat/setChat, getLegacy/setLegacy
getTurnAround/setTurnAround, getTurnBackWipe/setTurnBackWipe
getPlayerTemp/setPlayerTemp, getNoLavaSplash/setNoLavaSplash
getBouce/setBouce, getDefaultStuck/setDefaultStuck
getRespawnDelay/setRespawnDelay
getStuck/setStuck           -- TeaScript: NoMove（禁止移动）
getShadow/setShadow         -- 影子状态（作弊码相关）
getQuicksand/setQuicksand   -- TeaScript: 流沙计数器
```

**Generator 子字段（通过 getter/setter 方法访问）：**

```
getGeneratorDirection, getGeneratorEffect
getGeneratorTimeMax/setGeneratorTimeMax
getGeneratorTime/setGeneratorTime
```

**对象方法：**

```
getPermID()          -- 返回此 NPC 在全局数组中的 1-based 索引（永久 ID）
getName()            -- 返回此 NPC 的名称（字符串，空字符串表示未命名）
setName("name")      -- 设置此 NPC 的名称（写入字符串库）
luaX(), luaY()       -- 高精度坐标的整数部分（见 3.1 说明）
```

> 名称存储在引擎的字符串库中，支持通过 `xtech_npc_getByName("name")` 精确查找。
> 也可以在 `.lvlx` 关卡文件中通过 `NA:"name"` 字段直接预设名称（见第 8.3 节）。

### 3.7 Player_t（玩家角色）

**直接读写字段：**

```
DoubleJump, FlySparks, Driving, Quicksand, Bombs, Slippy
Fairy, FairyCD, FairyTime, HasKey, Hearts
CanFloat, FloatRelease, FloatTime, FloatSpeed, FloatDir
SwordPoke, GrabTime, GrabSpeed, VineNPC, VineBGO
Wet, WetFrame, SwimCount, NoGravity
Slide, SlideKill, Vine, ShellSurf, Rolling
StateNPC, Slope, Stoned, AquaticSwim
StonedCD, StonedTime, SpinJump, SpinFrame, SpinFireDir
Multiplier, SlideCounter, ShowWarp, ForceHold
YoshiYellow, YoshiBlue, YoshiRed
YoshiWingsFrame, YoshiWingsFrameCount
YoshiTX, YoshiTY, YoshiTFrame, YoshiTFrameCount
YoshiBX, YoshiBY, YoshiBFrame, YoshiBFrameCount
YoshiTongue, YoshiTonugeBool, YoshiTongueLength
YoshiNPC, YoshiPlayer, Dismount
Location, Character, Direction, Mount, MountType
MountSpecial, MountOffsetY, MountFrame
State, Frame, FrameCount, Jump, CanJump, CanAltJump
Effect, Effect2, RespawnY, Duck, DropRelease, StandUp, StandUp2, Bumped
Bumped2, Dead, TimeToLive, Immune, Immune2, ForceHitSpot3
HoldingNPC, CanGrabNPCs, HeldBonus, Section
WarpCD, Warp, WarpBackward, WarpShooted
FireBallCD, FireBallCD2, TailCount, RunCount
CanFly, CanFly2, FlyCount
RunRelease, JumpRelease, StandingOnNPC, StandingOnVehiclePlr
UnStart, mountBump, CurMazeZone, MazeZoneStatus
```

**位域成员（通过 getter/setter 方法访问）：**

```
getGroundPound/setGroundPound, getGroundPound2/setGroundPound2
getCanPound/setCanPound, getAltRunRelease/setAltRunRelease
getDuckRelease/setDuckRelease, getSlippyWall/setSlippyWall
getJumpOffWall/setJumpOffWall
```

**对象方法：**

```
luaX(), luaY()       -- 高精度坐标的整数部分
```

### 3.8 Layer_t（图层）

```lua
Name (只读), SpeedX, SpeedY, Hidden, EffectStop
```

### 3.9 Block_t（方块）

**原有字段:**

```
Location, Type, Special, Invis, Hidden, Slippy, Kill, RapidHit
```

**P0 新增字段（对应 TeaScript Block 属性）：**

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

**P5 新增字段（精灵图帧偏移）：**

```
extx, exty       -- 精灵图帧偏移（以 GFX 宽高为单位），渲染时自动应用
```

> `extx` 偏移 `GFXSlot` 的计算结果：`src_x = (GFXSlot + extx) * frameWidth`，`exty` 偏移 `Frame`：`src_y = (Frame + exty) * frameHeight`。默认均为 0。

**对象方法：**

```
getPermID()          -- 返回 1-based 永久 ID
getName()            -- 返回名称字符串
setName("name")      -- 设置名称
luaX(), luaY()       -- 高精度坐标的整数部分
```

### 3.10 Background_t（BGO / 背景对象）

```lua
Type        -- TeaScript: ID（背景对象类型 ID）
Hidden      -- TeaScript: 隐藏标记
Layer       -- 所在图层索引
SortPriority -- 排序优先级
Location    -- X, Y, Width, Height（SpeedlessLocation_t）
extx, exty  -- P5: 精灵图帧偏移（以 GFX 宽高为单位），渲染时自动应用

-- 方法
getPermID()   -- 返回 1-based 永久 ID
```

**TeaScript BGO 属性映射:**
- `Bgo(id).X` → `BGO.Location.X`
- `Bgo(id).Y` → `BGO.Location.Y`
- `Bgo(id).ID` → `BGO.Type`

### 3.11 Water_t（Liquid / 液体）

```lua
Location    -- X, Y（SpeedlessLocation_t）
Type        -- 液体类型（PHYSID: 1=水, 2=流沙）
Hidden      -- 隐藏
Layer       -- 所在图层索引

-- 方法
getPermID()    -- 返回 0-based 永久 ID
getName()      -- 返回名称字符串
setName("name") -- 设置名称
```

### 3.12 Warp_t（管道/传送门）

```lua
Entrance        -- 入口位置（SpeedlessLocation_t）
Exit            -- 出口位置（SpeedlessLocation_t）
Locked          -- 需要钥匙
WarpNPC         -- 允许 NPC 通过
NoYoshi         -- 禁止坐骑进入
Hidden          -- 隐藏
Stars           -- 需要星星数
Effect          -- 管道/门样式
LevelWarp       -- 目标关卡传送门
LevelEnt        -- 是否为关卡入口
Direction       -- 入口方向
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
StarsMsg        -- 星星不足提示文本索引
transitEffect   -- 过渡特效
Layer           -- 所在图层

-- 方法
getPermID()   -- 返回 1-based 永久 ID
```

### 3.13 Effect_t（特效）

```lua
Location    -- 特效位置（Location_t，含 SpeedX/SpeedY）
Type        -- 特效类型 ID（EFFID）
Frame       -- 当前帧
FrameCount  -- 帧计数器
Life        -- 剩余生命（帧）
NewNpc      -- 特效结束时生成的 NPC ID
NewNpcSpecial -- 新 NPC 的附加数据
Shadow      -- 是否暗色特效
```

### 3.14 SpriteComponent（精灵组件）

```lua
data1, data2, data3, data4, lookup_code, run_time, org_time, data5, expired
```

**BasicAnimate 动画参数（`func = SpriteFunc::BasicAnimate`）：**

| 字段 | 用途 | 说明 |
|---|---|---|
| `data1` | 单帧高度 | 每个动画帧的高度（像素），=0 时取 1 |
| `data2` | 帧间隔 | 每帧持续的 tick 数 |
| `data3` | 单帧宽度 | 每个动画帧的宽度（像素），=0 时使用图像全宽（兼容竖排） |

> **网格模式：** 当 `data3 > 0` 且不等于图像全宽时，将图像按 `(width/data3) × (height/data1)` 的网格切分，
> 帧序号按从左到右、从上到下排列。`data3=0` 退化为竖排模式（兼容旧行为）。

### 3.15 CSprite（自定义精灵）

```lua
-- 字段
ImgResCode, CollisionCode, FramesLeft, DrawPriorityLevel
OffscreenCount, FrameCounter, GfxXOffset, GfxYOffset
StaticScreenPos, Visible, Birthed, Died, Invalidated
LimitedFrameLife, AnimationSet, AlwaysProcess
Xpos, Ypos, Ht, Wd, Xspd, Yspd
Hitbox, AnimationPhase, AnimationTimer, AnimationFrame

-- 方法
setImage(img)              -- 设置图片（LunaImage 对象）
setImageResource(code)     -- 按资源码设置图片
makeLimitedLife(frames)    -- 设置为有限生命周期
setCustomVar(name, op, value) -- 设置自定义变量
customVarExists(name)      -- 检查自定义变量是否存在
getCustomVar(name)         -- 获取自定义变量值
birth()                    -- 标记精灵为已出生
die()                      -- 标记精灵为已死亡
getExtX()                  -- 当前帧在精灵图网格中的列号（0-based）
getExtY()                  -- 当前帧在精灵图网格中的行号（0-based）
getFrameCols()             -- 精灵图网格的列数
```

---

## 4. 游戏操作 API

### 4.1 NPC 操作

```lua
npc = xtech_npc_get(index)                        -- 获取第 index 个 NPC（1-based）
count = xtech_npc_count()                         -- NPC 总数
npc = xtech_npc_getFirstMatch(id, section)        -- 按 ID 和 section 查找第一个匹配（id=0 匹配所有, section=0 匹配所有区域）
xtech_npc_memSet(id, offset, value, op, ftype)    -- 内存级 NPC 属性修改
xtech_npc_allSetHits(identity, section, hits)     -- 批量设置 NPC 生命值
xtech_npc_kill(a, b)                              -- 杀死 NPC
xtech_npc_hurt(a, b, c)                           -- 伤害 NPC
npc = xtech_npc_getByPermID(permId)               -- 通过永久 ID 精确获取 NPC
npc = xtech_npc_getByName("name")                 -- 通过名称精确查找 NPC（需预先 setName）
xtech_npc_forEach(id, section, function(npc)      -- 遍历所有匹配 NPC 执行回调（id=0 匹配所有, section=0 匹配所有区域）
    npc:setStuck(true)
end)
npc = xtech_npc_create(type, x, y, xspd, yspd)    -- P3: 动态创建 NPC，返回对象或 nil
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
player = xtech_player_get(num)                    -- 获取第 num 个玩家（1-based）
count = xtech_player_count()                      -- 玩家总数
xtech_player_filterToBig(player)                  -- 强制大形态
xtech_player_filterToSmall(player)                -- 强制小形态
xtech_player_filterToFire(player)                 -- 强制火形态
xtech_player_filterMount(player)                  -- 移除坐骑
xtech_player_filterReservePowerup(player)         -- 清除备用道具
xtech_player_cycleRight(player)                   -- 循环到下一个角色
xtech_player_cycleLeft(player)                    -- 循环到上一个角色
xtech_player_infiniteFlying(player)               -- 无限飞行
isDown = xtech_player_pressingDown(player)        -- 是否按下
isUp = xtech_player_pressingUp(player)
isLeft = xtech_player_pressingLeft(player)
isRight = xtech_player_pressingRight(player)
isJump = xtech_player_pressingJump(player)
isRun = xtech_player_pressingRun(player)
isSEL = xtech_player_pressingSEL(player)
yes = xtech_player_isHoldingSpriteType(p, id)     -- 检查玩家手持指定类型
yes = xtech_player_usesHearts(player)             -- 检查玩家是否使用心形生命
xtech_player_dismount(player)                     -- P6: 坐骑静默脱落为NPC（无跳跃/无音效）
xtech_player_memSet(offset, value, op, ftype)     -- 内存级玩家属性修改
```

### 4.3 图层操作

```lua
layer = xtech_layer_get(index)                    -- 获取图层
count = xtech_layer_count()                       -- 图层总数
xtech_layer_setXSpeed(layer, speed)               -- 设置水平速度
xtech_layer_setYSpeed(layer, speed)               -- 设置垂直速度
xtech_layer_stop(layer)                           -- 停止图层移动
```

### 4.4 方块操作

```lua
block = xtech_block_get(index)                    -- 获取方块
count = xtech_block_count()                       -- 方块总数
xtech_block_setAll(type1, type2)                  -- 批量设置方块类型
xtech_block_swapAll(type1, type2)                 -- 批量交换方块类型
xtech_block_showAll(type)                         -- 批量显示方块
xtech_block_hideAll(type)                         -- 批量隐藏方块
yes = xtech_block_isPlayerTouchingType(type, collision, player)
block = xtech_block_getByPermID(permId)           -- 通过永久 ID 精确获取方块
block = xtech_block_getByName("name")             -- 通过名称精确查找方块
xtech_block_forEach(type, function(block)         -- 遍历所有匹配方块执行回调（type=0 匹配所有）
    block.Hidden = false
end)
```

### 4.5 BGO（背景对象）操作

```lua
bgo = xtech_bgo_get(index)                        -- 获取第 index 个 BGO（1-based）
count = xtech_bgo_count()                         -- BGO 总数
bgo = xtech_bgo_getByPermID(permId)               -- 通过永久 ID 精确获取 BGO
xtech_bgo_forEach(type, function(bgo)             -- 遍历所有匹配 BGO 执行回调（type=0 匹配所有）
    bgo.Hidden = true
end)
-- BGO 字段: Type, Hidden, Layer, SortPriority, Location (SpeedlessLocation_t), extx, exty
-- BGO 方法: getPermID() 返回此 BGO 的 1-based 永久 ID
```

### 4.6 液体操作

```lua
liquid = xtech_liquid_get(index)                  -- 获取第 index 个液体（0-based）
count = xtech_liquid_count()                      -- 液体总数
liquid = xtech_liquid_getByPermID(permId)         -- 通过永久 ID 精确获取液体
liquid = xtech_liquid_getByName("name")           -- 通过名称精确查找液体
-- Liquid 方法: getPermID() 返回此 Liquid 的 0-based 永久 ID; getName()/setName()
-- Liquid 字段: Location (SpeedlessLocation_t), Type (PHYSID), Hidden, Layer
```

### 4.7 传送门操作

```lua
warp = xtech_warp_get(index)                      -- 获取第 index 个传送门（1-based）
count = xtech_warp_count()                        -- 传送门总数
warp = xtech_warp_getByPermID(permId)             -- 通过永久 ID 精确获取传送门
-- Warp 方法: getPermID() 返回此 Warp 的 1-based 永久 ID
-- Warp 字段: Entrance, Exit, Locked, WarpNPC, NoYoshi, Hidden, Stars, Effect,
--            LevelWarp, LevelEnt, Direction, Direction2, MapWarp, MapX, MapY,
--            curStars, twoWay, noPrintStars, noEntranceScene, cannonExit,
--            cannonExitSpeed, stoodRequired, eventEnter, eventExit,
--            StarsMsg, transitEffect, Layer
```

### 4.8 Section（区域）操作

```lua
section = xtech_section_get(index)                -- 获取 Section 的位置（SpeedlessLocation_t，index 0-based）
count = xtech_section_count()                     -- Section 数量
bg = xtech_section_getBackground(index)           -- 获取 Section 背景 ID
xtech_section_setBackground(index, bgId)          -- 设置 Section 背景 ID
music = xtech_section_getMusic(index)             -- 获取 Section 音乐 ID
xtech_section_setMusic(index, musicId)            -- 设置 Section 音乐 ID
file = xtech_section_getMusicFile(index)          -- 获取 Section 自定义音乐文件
xtech_section_setMusicFile(index, filename)       -- 设置 Section 自定义音乐文件
b = xtech_section_getOffScreenExit(index)         -- 获取是否允许屏幕外退出
xtech_section_setOffScreenExit(index, bool)       -- 设置是否允许屏幕外退出
```

**TeaScript Section 映射:**
- `Section(index).Width` → `section.Width`
- `Section(index).Height` → `section.Height`
- `Section(index).X` → `section.X`
- `Section(index).Y` → `section.Y`

### 4.9 音频

```lua
xtech_audio_playSFX(index, loops, volume)         -- 播放内置音效
xtech_audio_playSFXExt(filename, loops, vol)      -- 播放自定义音效（关卡目录）
xtech_audio_stopSFXExt(filename)                  -- 停止自定义音效
xtech_audio_preloadSFXExt(filename)               -- 预加载自定义音效
xtech_audio_playMusic(section, fadeInMs)          -- 播放 section 音乐
xtech_audio_playMusicFile(filename, fadeInMs)     -- 播放自定义音乐
xtech_audio_setMusic(section, musicID, file)      -- 设置 section 音乐
```

### 4.10 精灵系统

```lua
-- 图片管理
xtech_sprite_loadImage(filename, code, transColor)   -- 加载图片并指定透明色
xtech_sprite_loadImageSimple(filename, code)         -- 加载图片（默认透明色）
img = xtech_sprite_getImage(code)                    -- 获取已加载的 LunaImage
xtech_sprite_deleteImage(code)                       -- 删除图片资源

-- 精灵放置
spr = xtech_sprite_place(type, imgCode, x, y, time)
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
xtech_hud_showText(text, x, y, font)              -- 在屏幕坐标显示文字（直接渲染，需每帧调用）
xtech_hud_showLevelName(x, y, font)               -- 显示关卡名
xtech_hud_showLevelFile(x, y, font)               -- 显示文件名
xtech_hud_showImage(imgCode, x, y, sx, sy, sw, sh) -- 在 HUD 平面绘制图像（屏幕固定，支持裁切）
xtech_hud_showNPC(npcId, x, y, w, h)              -- 在 HUD 平面绘制指定 NPC 的图像
xtech_hud_debugPrint(text)                        -- 输出调试信息到日志
```

**`xtech_hud_showImage` 说明：**

直接将图像绘制到 HUD 平面，不随镜头移动。比 `xtech_sprite_place(type=1)` 更轻量，无需 sprite 管线：

```lua
xtech_sprite_loadImage("hud.png", 100)
-- 完整绘制
xtech_hud_showImage(100, 10, 10, 0, 0, 0, 0)
-- 裁切绘制：从 (0,32) 取 32×16 区域画到 (200,100)
xtech_hud_showImage(100, 200, 100, 0, 32, 32, 16)
-- sw=0 / sh=0 自动取图像完整尺寸
xtech_hud_showImage(100, 400, 300, 64, 0, 0, 0)
```

注意内部使用 `RenderBitmapOp`（pool 分配，单帧生命周期），坐标经过 `TranslateScreenCoords` 做跨分辨率适配，与 `StaticDraw` 共享相同的渲染管线。

**`xtech_hud_showNPC` 说明：**

在 HUD 平面绘制指定 NPC ID 的精灵图图像。用于 HUD 中显示 NPC 图标（如道具预览）。

**ShowOnScreenHUD 与 ShowInterface：**

```
PLANE_LVL_HUD:
  if(ShowOnScreenHUD) {
      onRenderHud(Z, numScreens)      ← 受 ShowOnScreenHUD 控制
      if(ShowInterface) {
          DrawInterface(Z)            ← 受 ShowInterface 控制（仅内建游戏界面）
      }
  }
```

- `setShowHud(false)` — 隐藏整个 HUD 平面（包括 `onRenderHud` 回调）
- `setShowInterface(false)` — 仅隐藏内建游戏界面（生命/金币/分数等），**保留** `onRenderHud` 回调
- 自定义 HUD 应使用 `setShowInterface(false)` 以保留 `onRenderHud` 回调

> **注意：** 坐标参数 `x, y` 使用 Lua number（double），内部转为整数。font 取值范围 1-5。

### 4.12 变量系统

```lua
value = xtech_var_get(name)                       -- 读取变量值
exists = xtech_var_exists(name)                   -- 检查变量是否存在
ok = xtech_var_operation(name, value, op)         -- 对变量进行算术运算
```

### 4.13 事件系统

```lua
xtech_event_trigger(section, eventId)             -- 触发指定 section 的事件
xtech_event_triggerByName(eventName)              -- 按名称触发事件
xtech_event_cancelByName(eventName)               -- 按名称取消事件
xtech_event_subscribe(eventName, callback)         -- P4: 订阅系统事件（按名称注册 Lua 回调）
```

### 4.14 异步 / 延迟 / 定时器

```lua
xtech_misc_wait(callback, frames)                 -- 延迟 frames 帧后执行回调（匿名，不可取消）
xtech_timer_create(name, callback, frames)        -- P3: 创建命名定时器（可取消）
xtech_timer_createEvent(eventName, frames)         -- P3: 延迟触发命名游戏事件（TeaScript TCreate 等价）
xtech_timer_cancel(name)                          -- P3: 取消命名定时器
xtech_timer_clearAll()                            -- P3: 清除所有定时器（包括匿名）
```

**与 TeaScript 对比：**

| TeaScript | Lua |
|---|---|
| `Call TCreate(eventname, delay)` | `xtech_timer_createEvent(eventName, frames)` |
| `Call TClear(0, eventname)` | `xtech_timer_cancel(name)` |
| `Call TClear(1, 0)` | `xtech_timer_clearAll()` |
| `Call Sleep(delay)` | `xtech_misc_wait(callback, frames)`（异步版本） |

### 4.15 杂项

```lua
frame = xtech_misc_getFrame()                     -- 当前帧计数
section = xtech_misc_getSection(player)           -- 获取玩家所在 section
xtech_misc_showMsg(text)                          -- 显示游戏内消息框（普通样式）
xtech_misc_showMsgInfo(text)                      -- 显示消息框（信息样式，有标题栏）
xtech_misc_showMsgWarn(text)                      -- 显示消息框（警告样式，黄色）
xtech_misc_cheat(cheatString)                     -- 执行作弊码字符串
xtech_misc_log(message)                           -- 记录 INFO 级别日志
xtech_misc_logWarn(message)                       -- 记录 WARNING 级别日志
xtech_misc_logDebug(message)                      -- 记录 DEBUG 级别日志
width = xtech_getStrWidth(text, font)             -- 计算文字宽度（像素），font 缺省为 3
width = xtech_getStrWidth(text)                   -- 同上，font 固定为 3
```

**ShowMsg 说明：** 这是游戏内的消息框（不是 Windows 弹窗）。调用后会暂停游戏并显示文本，玩家按跳跃键关闭。三种样式：
- `showMsg` — MESSAGE_TYPE_NORMAL，无标题栏
- `showMsgInfo` — MESSAGE_TYPE_SYS_INFO，有标题栏
- `showMsgWarn` — MESSAGE_TYPE_SYS_WARNING，黄色标题栏

### 4.16 Sysval 系统（全局游戏状态）

```lua
-- 玩家/分数
lives = xtech_sysval_getLives()                   -- 当前生命数
xtech_sysval_setLives(5)                          -- 设置生命数
coins = xtech_sysval_getCoins()                   -- 当前金币数
xtech_sysval_setCoins(50)                         -- 设置金币数
score = xtech_sysval_getScore()                   -- 当前分数
xtech_sysval_setScore(10000)                      -- 设置分数

-- 相机 / 屏幕尺寸（screen 参数为 1-based: 1=左, 2=中, 3=右）
x = xtech_sysval_getScreenX(screen)               -- 屏幕的 X 坐标（世界坐标）
y = xtech_sysval_getScreenY(screen)               -- 屏幕的 Y 坐标（世界坐标）
w = xtech_sysval_getScreenWidth(screen)           -- 屏幕宽度（像素）
h = xtech_sysval_getScreenHeight(screen)          -- 屏幕高度（像素）
top = xtech_sysval_getScreenTop(screen)           -- HUD 顶部偏移（>600 时居中到 600 区域）
cx = xtech_sysval_getScreenCenterX(screen)        -- 屏幕水平中心点（Width / 2）

-- 游戏状态
show = xtech_sysval_getShowHud()                  -- 是否显示 HUD 平面
xtech_sysval_setShowHud(false)                    -- 隐藏 HUD 平面（禁用 onRenderHud）
showIf = xtech_sysval_getShowInterface()          -- 是否显示内建游戏界面
xtech_sysval_setShowInterface(false)              -- 仅隐藏内建界面（保留 onRenderHud）
battle = xtech_sysval_getBattleMode()             -- 是否为对战模式（只读）
ending = xtech_sysval_getEndLevel()               -- 是否正在结束关卡
xtech_sysval_setEndLevel(true)                    -- 立即结束关卡
macro = xtech_sysval_getLevelMacro()              -- 关卡结束宏类型
xtech_sysval_setLevelMacro(v)                     -- 设置关卡结束宏类型（0 可取消硬编码动画）
counter = xtech_sysval_getLevelMacroCounter()     -- 结束宏计数器
time = xtech_sysval_getGameTime()                 -- 游戏运行总帧数
name = xtech_sysval_getLevelName()                -- 当前关卡名称（LevelName 或 FileName）

-- 检查点
ckptCount = xtech_sysval_getCheckpointCount()     -- 当前已收集的检查点数量（0=全新开始）
ckptId = xtech_sysval_getCheckpointId(n)          -- 第 n 个检查点的 ID（1-based，NPC.Special 值）

-- 关卡退出路径
beatCode = xtech_sysval_getLevelBeatCode()        -- 当前关卡退出路径（决定世界地图解锁）
xtech_sysval_setLevelBeatCode(v)                  -- 设置关卡退出路径（配合 setEndLevel 使用）
```

### 4.17 特效操作

```lua
eff = xtech_effect_create(effId, x, y, direction, shadow)  -- 创建特效，返回对象或 nil
eff = xtech_effect_get(index)                     -- 获取第 index 个特效（1-based）
count = xtech_effect_count()                      -- 特效总数
xtech_effect_kill(index)                          -- 删除指定特效
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

`CONST_FT_BYTE`, `CONST_FT_WORD`, `CONST_FT_DWORD`, `CONST_FT_FLOAT`, `CONST_FT_DFLOAT`

> 常量值直接取自 `FIELDTYPE` 枚举（定义在 `globals.h`），与 `StrToFieldtype` 中 "b"/"s"/"dw"/"f"/"df" 映射一致。

### 5.5a 物理常量

`CONST_PLAYER_RUN_SPEED` — 玩家最大跑步速度（像素/帧），对应 `Physics.PlayerRunSpeed`。

> 用于 P-meter / 速度进度条等 Lua 逻辑。与 `player.Location.SpeedX` 比较判断玩家是否达到满速。

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

### 5.11 LevelMacro（关卡结束宏类型）

| 常量 | 值 | 说明 |
|---|---|---|
| `CONST_LEVELMACRO_OFF` | 0 | 无宏（设为 0 可取消硬编码过关动画） |
| `CONST_LEVELMACRO_CARD_ROULETTE` | 1 | 卡牌轮盘（NPC 11） |
| `CONST_LEVELMACRO_QUESTION_SPHERE` | 2 | 问号球（NPC 16） |
| `CONST_LEVELMACRO_KEYHOLE` | 3 | 钥匙孔（NPC 41） |
| `CONST_LEVELMACRO_CRYSTAL_BALL` | 4 | 水晶球（NPC 41 S2） |
| `CONST_LEVELMACRO_GAME_COMPLETE` | 5 | 通关（最终 Boss） |
| `CONST_LEVELMACRO_STAR` | 6 | 星星（NPC 97） |
| `CONST_LEVELMACRO_GOAL_TAPE` | 7 | 终点带（NPC 197） |
| `CONST_LEVELMACRO_FLAG` | 8 | 旗杆（NPC 306） |

### 5.12 LevelBeatCode（关卡退出路径）

| 常量 | 值 | 说明 |
|---|---|---|
| `CONST_BEATCODE_NONE` | 0 | 无 |
| `CONST_BEATCODE_CARD_ROULETTE` | 1 | 卡牌轮盘退出 |
| `CONST_BEATCODE_QUESTION_SPHERE` | 2 | 问号球退出 |
| `CONST_BEATCODE_OFFSCREEN` | 3 | 屏幕外退出 |
| `CONST_BEATCODE_KEYHOLE` | 4 | 钥匙孔退出 |
| `CONST_BEATCODE_CRYSTAL_BALL` | 5 | 水晶球退出 |
| `CONST_BEATCODE_WARP` | 6 | 传送门退出 |
| `CONST_BEATCODE_STAR` | 7 | 星星退出 |
| `CONST_BEATCODE_GOAL_TAPE` | 8 | 终点带退出 |
| `CONST_BEATCODE_FLAG` | 9 | 旗杆退出 |
| `CONST_BEATCODE_ALT_FLAG` | 10 | 秘密旗杆退出 |
| `CONST_BEATCODE_RESERVED_1` | 11 | 脚本专用，自定义退出路径 1 |
| `CONST_BEATCODE_RESERVED_2` | 12 | 脚本专用，自定义退出路径 2 |
| `CONST_BEATCODE_RESERVED_3` | 13 | 脚本专用，自定义退出路径 3 |
| `CONST_BEATCODE_RESERVED_4` | 14 | 脚本专用，自定义退出路径 4 |
| `CONST_BEATCODE_GAME_COMPLETE` | 15 | 游戏通关（标记存档完成） |

> **注意：** `LevelMacro` 和 `LevelBeatCode` 是两个独立的枚举系统，数值并不对应。例如旗杆退出时 `LevelMacro=8` 但 `BeatCode=9`。设置时应使用对应常量，不要混用。

### 5.13 NPC ID 常量（306 个）

所有 [npc_id.h](src/npc_id.h) 中定义的 NPCID 枚举值均暴露为 `CONST_NPC_<NAME>` 格式：

`CONST_NPC_NULL(0)` → `CONST_NPC_FLAG_EXIT(306)`，包括 SMBX 1.3 自定义 NPC（293-300: `CONST_NPC_RESERVED_293` 至 `CONST_NPC_RESERVED_300`）和 TheXTech 独占 NPC（301-306: `CONST_NPC_INVINCIBILITY_POWER`, `CONST_NPC_AQUATIC_POWER`, `CONST_NPC_POLAR_POWER`, `CONST_NPC_CYCLONE_POWER`, `CONST_NPC_SHELL_POWER`, `CONST_NPC_FLAG_EXIT`）。

### 5.14 SFX ID 常量（106 个）

所有 [sound.h](src/sound.h) 中定义的音效常量均暴露为 `CONST_SFX_<NAME>` 格式：

`CONST_SFX_Jump(1)` → `CONST_SFX_FlagExit(106)`，覆盖所有 SMBX64 音效和 TheXTech 扩展音效。

### 5.15 EFFID 常量（152 个）

所有 [eff_id.h](src/eff_id.h) 中定义的 EFFID 枚举值均暴露为 `CONST_EFF_<NAME>` 格式：

`CONST_EFF_SMOKE_S3_CENTER` → `CONST_EFF_GENERIC_NPC_SQUISH`，覆盖所有特效类型（包括负值编号和 100+ 编号）。

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

## 8. 系统事件钩子（P4 — 已实现 17/25）

### 8.1 概述

系统事件钩子允许 Lua 脚本通过定义特定名称的全局函数来响应游戏运行时事件。脚本中定义函数即表示订阅该事件，不定义则跳过。

此外，通过 `xtech_event_subscribe(name, callback)` 可以在运行时动态订阅事件。

```lua
-- 方式 1：定义全局函数（静态订阅）
function onNPCDeath(permid, npcId, killerPlayerId)
    if npcId == CONST_NPC_MINIBOSS then
        xtech_event_triggerByName("bossKilled")
        xtech_misc_showMsg("Boss defeated!")
    end
end

-- 方式 2：运行时动态订阅
xtech_event_subscribe("onNPCDeath", function(permid, npcId, killerPlayerId)
    xtech_misc_log(string.format("NPC %d killed by player %d", permid, killerPlayerId))
end)
```

### 8.2 事件回调函数签名规范

所有事件回调返回 `nil`（无返回值）。参数类型：整数为 `int`，浮点为 `num_t`。

---

### NPC 事件（7 个）

#### onNPCUpdate(permid, npcId)
**触发时机：** 每个活跃 NPC 每帧更新时调用。在 NPC 运动/AI 逻辑**之前**触发。
**参数：** `permid` (int), `npcId` (int)
**性能：** 🔴 高频调用（每帧 × NPC 数量）。建议仅在必要时定义，并在内部尽早 return。

#### onNPCDeath(permid, npcId, killerPlayerId)
**触发时机：** NPC 被杀死**之后**（`KillNPC()` 返回前）。
**参数：** `permid` (int), `npcId` (int), `killerPlayerId` (int) — 杀死者玩家编号（1/2），环境杀死为 0

#### onNPCHurt(permid, npcId, hitterId, hitType)
**触发时机：** NPC 被攻击**之后**（`NPCHit()` 处理完成时）。
**参数：** `permid` (int), `npcId` (int), `hitterId` (int), `hitType` (int) — 1=踩踏, 2=火球, 3=尾巴, 4=旋转跳等

#### onNPCActivate(permid, npcId)
**触发时机：** NPC 进入激活状态（`Active` 设为 `true`）。
**参数：** `permid` (int), `npcId` (int)

#### onNPCTalk(permid, npcId, playerId)
**触发时机：** 玩家与 NPC 对话时（对话逻辑执行**之后**）。
**参数：** `permid` (int), `npcId` (int), `playerId` (int)

#### onNPCTouch(permid, npcId, playerId, side)
**触发时机：** 玩家接触到 NPC 的碰撞箱时。
**参数：** `permid` (int), `npcId` (int), `playerId` (int), `side` (int) — 0=无接触, 1=上方, 2=下方, 3=左侧, 4=右侧

#### onNPCGrab(permid, npcId, playerId, fromTop)
**触发时机：** 玩家抓起 NPC 时。
**参数：** `permid` (int), `npcId` (int), `playerId` (int), `fromTop` (bool)

---

### 玩家事件（7 个）

#### onPlayerHurt(playerId, damage, cause)
**触发时机：** 玩家受伤**之后**（`PlayerHurt()` 处理完成时）。
**参数：** `playerId` (int), `damage` (int), `cause` (int) — 伤害来源 NPC ID，0=环境伤害

#### onPlayerDeath(playerId, cause)
**触发时机：** 玩家死亡动画开始时（`PlayerDead()` 调用时）。
**参数：** `playerId` (int), `cause` (int)

#### onPlayerPowerUp(playerId, oldState, newState)
**触发时机：** 玩家形态改变**之后**。
**参数：** `playerId` (int), `oldState` (int), `newState` (int) — 使用 `CONST_PLAYER_*` 常量

#### onPlayerMount(playerId, mountType)
**触发时机：** 玩家骑上坐骑时。
**参数：** `playerId` (int), `mountType` (int) — 使用 `CONST_MOUNT_*` 常量

#### onPlayerDismount(playerId, mountType)
**触发时机：** 玩家从坐骑下来时。
**参数：** `playerId` (int), `mountType` (int)

#### onPlayerSwitchChar(playerId, oldChar, newChar)
**触发时机：** 玩家切换角色时。
**参数：** `playerId` (int), `oldChar` (int), `newChar` (int) — 0=Mario, 1=Luigi, 2=Peach, 3=Toad, 4=Link

#### onPlayerRespawn(playerId)
**触发时机：** 玩家重生（失去一条命后重新开始）。
**参数：** `playerId` (int)

---

### 方块事件（2 个）

#### onBlockHit(blockId, blockType, hitterId, hitStyle)
**触发时机：** 方块被撞击**之后**。
**参数：** `blockId` (int), `blockType` (int), `hitterId` (int) — NPC 撞击时为负值, `hitStyle` (int)

#### onBlockDestroy(blockId, blockType, destroyerId)
**触发时机：** 方块被摧毁**之后**。
**参数：** `blockId` (int), `blockType` (int), `destroyerId` (int)

---

### 关卡 / 游戏事件（7 个）

#### onLevelLoad()
**触发时机：** 关卡加载完成，所有对象初始化之后。（等价于全局钩子 `onLoad`）
**参数：** 无

#### onLevelComplete()
**触发时机：** 关卡通关（接触到终点/球/星星/磁带时 `EndLevel = true`）。
**参数：** 无

#### onPostMacro()
**触发时机：** `UpdateMacro()` 完成后，`LevelMacro` 回到 `LEVELMACRO_OFF` 时。
**参数：** 无
**使用场景：** 在硬编码过关动画结束后重设 `LevelBeatCode`。因为 `UpdateMacro()` 会根据 `LevelMacro` 自动覆盖 `LevelBeatCode`，`onLevelComplete` 中设置的值会被覆盖。`onPostMacro` 在 `UpdateMacro` 结束后触发，此时重设 `LevelBeatCode` 可确保自定义退出路径生效。

```lua
function onLevelComplete()
    xtech_sysval_setLevelMacro(CONST_LEVELMACRO_OFF)  -- 杀死硬编码动画
end
function onPostMacro()
    -- UpdateMacro 已经完成了，此时安全重设 BeatCode
    xtech_sysval_setLevelBeatCode(CONST_BEATCODE_ALT_FLAG)
    xtech_sysval_setEndLevel(true)
end
```

#### onLevelExit()
**触发时机：** 关卡退出时（返回世界地图/菜单）。（等价于全局钩子 `onLoopEnd`）
**参数：** 无

#### onGameOver()
**触发时机：** 所有玩家生命用尽，Game Over 画面显示时。
**参数：** 无

#### onPause()
**触发时机：** 游戏暂停时。
**参数：** 无

#### onUnpause()
**触发时机：** 游戏从暂停恢复时。
**参数：** 无

---

### 关卡/传送门事件（2 个）

#### onWarpEnter(playerId, warpId)
**触发时机：** 玩家进入传送门/管道时。
**参数：** `playerId` (int), `warpId` (int) — 传送门的永久索引

#### onWarpExit(playerId, warpId)
**触发时机：** 玩家从传送门/管道出来时。
**参数：** `playerId` (int), `warpId` (int)

---

### 实现状态汇总

| 类别 | 事件 | 频率 | 状态 |
|------|------|------|------|
| NPC | `onNPCUpdate` | 🔴 极高 | ✅ 已实现 |
| NPC | `onNPCDeath` | 🟢 低 | ✅ 已实现 |
| NPC | `onNPCHurt` | 🟡 中 | ✅ 已实现 |
| NPC | `onNPCActivate` | 🟡 中 | ✅ 已实现 |
| NPC | `onNPCTalk` | 🟢 低 | ✅ 已实现 |
| NPC | `onNPCTouch` | 🟡 中 | ⬜ 计划中 |
| NPC | `onNPCGrab` | 🟢 低 | ✅ 已实现 |
| 玩家 | `onPlayerHurt` | 🟡 中 | ✅ 已实现 |
| 玩家 | `onPlayerDeath` | 🟢 低 | ✅ 已实现 |
| 玩家 | `onPlayerPowerUp` | 🟢 低 | ⬜ 计划中 |
| 玩家 | `onPlayerMount` | 🟢 低 | ⬜ 计划中 |
| 玩家 | `onPlayerDismount` | 🟢 低 | ✅ 已实现 |
| 玩家 | `onPlayerSwitchChar` | 🟢 低 | ⬜ 计划中 |
| 玩家 | `onPlayerRespawn` | 🟢 低 | ✅ 已实现 |
| 方块 | `onBlockHit` | 🟡 中 | ✅ 已实现 |
| 方块 | `onBlockDestroy` | 🟢 低 | ✅ 已实现 |
| 关卡 | `onLevelComplete` | 🟢 低 | ✅ 已实现 |
| 关卡 | `onPostMacro` | 🟢 低 | ✅ 已实现 |
| 关卡 | `onLevelExit` | 🟢 低 | ✅ 已实现 |
| 关卡 | `onGameOver` | 🟢 低 | ✅ 已实现 |
| 关卡 | `onPause` | 🟢 低 | ✅ 已实现 |
| 关卡 | `onUnpause` | 🟢 低 | ⬜ 计划中 |
| 传送 | `onWarpEnter` | 🟡 中 | ⬜ 计划中 |
| 传送 | `onWarpExit` | 🟡 中 | ⬜ 计划中 |

**已完成：17 个事件 / 计划中：7 个事件 / 总计：25 个（含 onLevelLoad 则为 25 个系统事件）**

---

### 8.3 .lvlx 关卡文件对象命名（P4）

对象名称可以直接在 `.lvlx` 关卡文件中预设，无需通过 Lua 脚本设置。使用 `NA`（Name）字段：

```
BLOCK
ID:54;X:-160032;Y:-160576;W:32;H:32;NA:"exit_door";
BLOCK_END

NPC
ID:1;X:400;Y:300;NA:"boss_guard";
NPC_END
```

加载关卡时，`NA` 字段的值自动写入运行时 `Name` 属性，Lua 脚本可直接查询：

```lua
function onLoad()
    local door = xtech_block_getByName("exit_door")
    if door then door.Hidden = false end
end
```

**实现涉及的文件：**

| 文件 | 改动 |
|------|------|
| `3rdparty/PGE_File_Formats/lvl_filedata.h` | LevelBlock + LevelNPC 加 `PGESTRING name` |
| `3rdparty/PGE_File_Formats/src/pgex/file_rw_lvlx.cpp` | 读写 `NA` 字段 |
| `src/main/level_file.cpp` | 加载时 `SetS(obj.Name, fileData.name)` |
| `src/globals.h` | NPC_t / Block_t / Water_t 加 `stringindex_t Name` |

---

## 9. 与 SMBx TeaScript 1.4.5 对比

### 9.1 已覆盖功能（~85%）

| TeaScript 功能类别 | 状态 | Lua 实现方式 |
|---|---|---|
| 变量系统 (val/gval/sysval) | ✅ | `xtech_var_*` 函数 + Lua 原生变量 |
| 数学表达式和函数 | ✅ | Lua 内置 math 库 |
| 逻辑表达式 | ✅ | Lua 原生逻辑运算符 |
| 控制流 (If/For/Do/Select) | ✅ | Lua 原生控制流 |
| NPC 属性读写 | ✅ | NPC 类绑定（完整字段 + 位域 getter/setter） |
| Player 属性读写 | ✅ | Player 类绑定（完整字段 + 位域 getter/setter） |
| Block 属性读写 | ✅ | Block 类绑定（完整字段 + 名称方法） |
| BGO 属性读写 | ✅ | BGO 类绑定 |
| Liquid 属性读写 | ✅ | Liquid 类绑定 |
| Warp 属性读写 | ✅ | Warp 类绑定 |
| Section 属性读写 | ✅ | `xtech_section_*` 函数 |
| 对象永久 ID 获取 | ✅ | `getPermID()` 方法（所有对象类） |
| 按永久 ID 精确获取 | ✅ | `xtech_*_getByPermID()` 函数（5 个对象类型） |
| 批量遍历操作 | ✅ | `xtech_*_forEach()` 回调遍历（NPC/Block/BGO） |
| 对象命名 + 按名查找 | ✅ | `getName`/`setName`/`getByName`（NPC/Block/Liquid）+ `.lvlx` `NA` 字段 |
| 自定义变量槽 (Ivala/b/c) | ⬜ | 可用 Special2/3/4 字段代替 |
| Layer 操作 | ✅ | Layer 类绑定 + `xtech_layer_*` 函数 |
| 音频 (AudioSet) | ✅ | `xtech_audio_*` 函数 |
| HUD 文字 | ✅ | `xtech_hud_*` 函数（直接渲染，无池泄漏） |
| HUD 图像 | ✅ | `xtech_hud_showImage` — 屏幕固定图像，支持裁切 |
| HUD NPC 图 | ✅ | `xtech_hud_showNPC` — 在 HUD 中绘制 NPC 精灵图 |
| 屏幕尺寸 | ✅ | `xtech_sysval_getScreenWidth/Height/Top/CenterX` |
| ShowInterface | ✅ | `xtech_sysval_get/setShowInterface` |
| 检查点查询 | ✅ | `xtech_sysval_getCheckpointCount/Id` |
| 过关宏控制 | ✅ | `xtech_sysval_setLevelMacro` + `setLevelBeatCode` |
| 过关路径常量 | ✅ | 9 个 `CONST_LEVELMACRO_*` + 15 个 `CONST_BEATCODE_*` |
| 事件系统 | ✅ | `xtech_event_*` 函数 + `xtech_event_subscribe` 动态订阅 |
| 延迟/定时 | ✅ | `xtech_misc_wait` + `xtech_timer_create/cancel/clearAll` |
| TeaScript TCreate | ✅ | `xtech_timer_createEvent(eventName, frames)` |
| 精灵/位图 | ✅ | `xtech_sprite_*` 函数 |
| NPC 创建 (NCreate) | ✅ | `xtech_npc_create(type, x, y, xspd, yspd)` |
| NPC 删除 (NKill) | ✅ | `xtech_npc_kill` |
| 特效创建 (FXCreate) | ✅ | `xtech_effect_create()` + Effect_t 类绑定 |
| Sysval 系统 | ✅ | `xtech_sysval_get*/set*` 系列（完整覆盖） |
| 消息框 (ShowMsg) | ✅ | `xtech_misc_showMsg` + info/warn 变体 |
| 脚本互调 (EXEScript) | ⬜ | 可用 Lua `dofile`/`require` 替代 |
| 图层旋转 (LSpin) | ⬜ | 需底层 C++ 支持 |
| BGP 属性 | ⬜ | 需渲染管线重构（工作量较大） |
| MIDI (PlayNote) | ❌ | 过时技术，不适合现代平台 |

### 9.2 设计差异说明

| 方面 | TeaScript (VB6) | Lua 实现 |
|---|---|---|
| 变量系统 | `val(a)=`, `gval(b)=` 宏调用 | Lua 原生变量赋值 |
| 延迟 | `Sleep` 阻塞脚本 | `xtech_misc_wait` 异步回调 |
| 迭代器 | `ItrCreate`/`ItrNext` 命令式 API | Lua `for` 循环 + 条件判断 |
| 位图/HUD | `HUDSet` 多功能命令（type 切换） | 独立 API 函数 |
| 对象命名 | `getName`/`setName`/`getByName` + `.lvlx` `NA` 字段 | Lua 方法 + 文件格式原生支持 |
| 语法风格 | VB6 `With`/`Goto`/`Select Case` | Lua 惯用语法 |
| 阻塞 vs 异步 | `Sleep` 阻塞整个脚本 | 协程式异步（更安全） |

### 9.3 无法/不值得复刻的功能

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

## 10. 修改的文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `script/include/xtech_lua_main.h` | 编辑 | 新增 API 声明 |
| `script/src/xtech_lua_main.cpp` | 编辑 | 完整 Lua 生命周期 + 延迟调用 + 独立渲染管理 |
| `script/src/xtech_lua_bindings.h` | 编辑 | 新增 process/clear 函数声明 |
| `script/src/xtech_lua_bindings.cpp` | 编辑 | 全部绑定（~2550 行，包含 P0+P1+P2+P3+P4+P5+P6 扩展） |
| `script/src/xtech_lua_events.cpp` | 新增 | 系统事件钩子系统 |
| `src/script/luna/luna.cpp` | 编辑 | `lunaRenderEnd` 添加池回收（ClearQueue） |
| `src/script/luna/renderop_string.cpp` | 编辑 | `Draw` 添加字体有效性防御检查 |
| `src/graphics/gfx_print.cpp` | 编辑 | `SuperPrint`/`SuperTextPixLen` 添加调试日志 |
| `src/fontman/font_manager.cpp` | 编辑 | `printText` 添加调试日志 |
| `src/globals.h` | 编辑 | 新增 `ShowInterface` 全局变量 |
| `src/globals.cpp` | 编辑 | `ShowInterface` 初始化 |
| `src/graphics/gfx_update.cpp` | 编辑 | `DrawInterface` 受 `ShowInterface` 控制 + 4 处渲染钩子 |
| `script/CMakeLists.txt` | 编辑 | 添加 `xtech_lua_bindings.cpp` |
| `src/config.h` | 编辑 | 新增 `lua_enable_engine` 配置项 |
| `src/game_main.cpp` | 编辑 | 9 处 Lua 钩子 |
| `src/main/game_loop.cpp` | 编辑 | `xtech_lua_loop()` 钩子 |
| `src/main/menu_loop.cpp` | 编辑 | `xtech_lua_loop()` 钩子 |
| `src/main/outro_loop.cpp` | 编辑 | `xtech_lua_loop()` 钩子 |
| `src/main/game_save.cpp` | 编辑 | `SaveGame`/`LoadGame` 中 `OnGameSave`/`OnGameLoad` 钩子 |
| `src/main/world_loop.cpp` | 编辑 | `OnWorldMapRender` 钩子 |
| `script/src/xtech_lua_data.cpp` | **新增** | table↔JSON 递归序列化/反序列化 |
| `script/include/xtech_lua_data.h` | **新增** | 序列化 API 声明 |

**总计：** ~3100 行新代码/修改，分布在 17+ 个文件中。新增 140+ 个 API 函数、306 个 NPC ID 常量、106 个 SFX 常量、152 个 EFFID 常量、9 个 LEVELMACRO 常量、15 个 BEATCODE 常量。实现了完整的 Lua VM 常驻生命周期、关卡沙箱隔离、自定义数据持久化、世界地图 Lua 支持。

---

## 11. 未实现 / 后续扩展

### 计划实现（P4 剩余 — 7 个事件）
- [ ] **onNPCTouch** — TheXTech 中无直接触发点，需由事件系统驱动
- [ ] **onPlayerPowerUp** / **onPlayerMount** / **onPlayerSwitchChar**
- [ ] **onUnpause**
- [ ] **onWarpEnter** / **onWarpExit**

### 中优先级
- [ ] **lunadll.txt 兼容层** — 将旧格式自动转换为 Lua 调用
- [ ] **Lua 脚本热加载** — 无需重启关卡重新加载脚本
- [ ] **完整的错误报告 UI** — 在游戏内显示 Lua 错误而非仅写入日志

### 低优先级
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

## 12. 示例用法

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

function onRenderHud(Z, numScreens)
    xtech_hud_showText("Lua Active!", 10, 600, 3)
end
```

### 精确对象操作示例

```lua
-- 模式1：保存永久 ID，跨帧精确找回
local bossDoorId = nil

function onLoad()
    xtech_npc_forEach(CONST_NPC_LOCK_DOOR, 0, function(npc)
        bossDoorId = npc:getPermID()
        npc:setStuck(true)
    end)
end

function onLoop()
    if bossDoorId then
        local door = xtech_npc_getByPermID(bossDoorId)
        if door and door.Active then
            local player = xtech_player_get(1)
            if player and player.HasKey then
                door:setStuck(false)
                xtech_hud_showText("Door unlocked!", 400, 300, 3)
            end
        else
            bossDoorId = nil
        end
    end
end
```

```lua
-- 模式2：forEach 批量操作
function onLoad()
    xtech_npc_forEach(CONST_NPC_FODDER_S3, 0, function(npc)
        npc.Direction = -1
    end)

    xtech_block_forEach(188, function(block)
        block.Hidden = true
    end)

    xtech_bgo_forEach(50, function(bgo)
        bgo.Hidden = false
    end)
end
```

```lua
-- 模式3：手动遍历 + 条件筛选 + 永久 ID
local targets = {}

function onLoad()
    xtech_npc_forEach(CONST_NPC_FODDER_S3, 1, function(npc)
        table.insert(targets, npc:getPermID())
    end)
    xtech_misc_log("Found " .. #targets .. " goombas in section 1")
end

function onLoop()
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

### Sysval + NCreate + 特效 + 定时器 综合示例

```lua
function onLoad()
    xtech_sysval_setLives(5)
    xtech_sysval_setCoins(0)
    xtech_sysval_setShowHud(true)
    xtech_misc_log("Level started with " .. xtech_sysval_getLives() .. " lives")

    local npc = xtech_npc_create(CONST_NPC_FODDER_S3, 400, 300, -2, 0)
    if npc then
        npc.Direction = -1
        npc.Special = 999
        xtech_misc_log("Created NPC with permID: " .. npc:getPermID())
    end

    local eff = xtech_effect_create(CONST_EFF_SMOKE_S3, 400, 300, 1, false)
    if eff then
        eff.Location.SpeedX = 2
    end

    xtech_timer_create("delayedBoom", function()
        xtech_audio_playSFX(CONST_SFX_SMExplosion, 0, 128)
        xtech_effect_create(CONST_EFF_BOMB_S3_EXPLODE, 500, 300, 1, false)

        xtech_timer_create("musicSwitch", function()
            xtech_section_setMusic(0, 5)
        end, 120)
    end, 180)
end

function onLoop()
    if xtech_sysval_getCoins() >= 100 then
        xtech_sysval_setCoins(0)
        xtech_sysval_setLives(xtech_sysval_getLives() + 1)
        xtech_audio_playSFX(CONST_SFX_1up, 0, 128)
        xtech_hud_showText("1UP!", 400, 300, 3)
    end
end
```

### 按名称查找对象示例

```lua
function onLoad()
    -- 通过 .lvlx 中预设的 NA 字段查找
    local door = xtech_block_getByName("exit_door")
    if door then
        door.Hidden = true  -- 先隐藏门
    end

    local boss = xtech_npc_getByName("boss_guard")
    if boss then
        boss.Immune = false  -- 取消无敌
    end
end
```

### HUD 图像渲染示例

```lua
local hudImg, iconImg
function onLoad()
    hudImg  = xtech_sprite_loadImage("hud_sheet.png", 200)
    iconImg = xtech_sprite_loadImage("icons.png", 201)
end

function onRenderHud(Z, numScreens)
    local top  = xtech_sysval_getScreenTop(Z + 1)
    local cx   = xtech_sysval_getScreenCenterX(Z + 1)
    local sw   = xtech_sysval_getScreenWidth(Z + 1)

    -- 完整绘制
    xtech_hud_showImage(iconImg, 10, top + 10, 0, 0, 0, 0)
    -- 裁切绘制：从 (0,32) 取 32×16 区域
    xtech_hud_showImage(iconImg, 200, 100, 0, 32, 32, 16)
end
```

### 精灵示例

```lua
function onLoad()
    -- 加载 128×128 的 4×4 网格精灵图，每帧 32×32
    xtech_sprite_loadImage("my_sprite_sheet.png", 1000, 0xFF00DC)

    local spr = xtech_sprite_place(CONST_SPRITE_STATIC, 1000, 400, 300, 0)
    if spr then
        spr.Visible = true
        spr.StaticScreenPos = true
    end
end
```

### 网格精灵图动画示例

```lua
-- 加载 256×64 精灵图，data1=32(帧高), data2=8(帧间隔), data3=64(帧宽)
-- 自动切分为 4列×2行 = 8帧
function onLoad()
    xtech_sprite_loadImage("player_walk.png", 2000, 0xFF00DC)

    local spr = xtech_sprite_place(CONST_SPRITE_NORMAL, 2000, 400, 300, 0)
    if spr then
        spr:setCustomVar("_BasicAnimate_data1", 32)
        spr:setCustomVar("_BasicAnimate_data2", 8)
        spr:setCustomVar("_BasicAnimate_data3", 64)  -- 0=竖排(兼容), >0=网格
    end
end

function onLoop()
    local cols = spr:getFrameCols()
    local col  = spr:getExtX()
    local row  = spr:getExtY()

    if col == 0 and row == 0 then
        -- 第1列第1行: 站立帧
    elseif col == 1 then
        -- 第2列: 走路帧
    end
end
```

### extx/exty 帧偏移示例

```lua
-- 更改 NPC 精灵图中的显示帧组
function onLoad()
    xtech_npc_forEach(CONST_NPC_FODDER_S3, 0, function(npc)
        npc.extx = 2   -- 偏移到第 3 列（跳过默认的 idle 帧组）
        npc.exty = 1   -- 偏移到第 2 行（选择 walk 帧组）
    end)
end
```

> **工作原理：** `extx` 偏移 `GFXSlot` → `src_x = (GFXSlot + extx) * frameWidth`；`exty` 偏移 `Frame` → `src_y = (Frame + exty) * frameHeight`。
> **编辑器工作流：** 可在关卡编辑器中为每个 NPC 实例选择 extx/exty，存入 `.lvlx` 文件。Lua 可在运行时读写来动态切换帧组。

### 自定义过关动画示例

```lua
local customEnding = false
local endingPhase   = 0
local endingTimer   = 0

function onLevelComplete()
    if xtech_sysval_getLevelMacro() == CONST_LEVELMACRO_FLAG then
        xtech_sysval_setLevelMacro(CONST_LEVELMACRO_OFF)  -- 杀死硬编码动画
        customEnding = true
        endingPhase  = 1
        endingTimer  = 0
    end
end

function onLoop()
    if not customEnding then return end

    local p = xtech_player_get(1)
    if not p then return end

    endingTimer = endingTimer + 1

    if endingPhase == 1 then
        p.Location.SpeedX = 0
        p.Location.SpeedY = 3
        if endingTimer > 60 then
            endingPhase = 2
            endingTimer = 0
        end
    elseif endingPhase == 2 then
        p.Location.SpeedX = 2
        p.Direction = 1
        if endingTimer > 120 then
            xtech_sysval_setLevelBeatCode(CONST_BEATCODE_FLAG)
            xtech_sysval_setEndLevel(true)
            customEnding = false
        end
    end
end
```

> **流程：** `onLevelComplete` → `setLevelMacro(0)` 杀死硬编码动画 → Lua 控制玩家 → 设 `LevelBeatCode` + `EndLevel = true`。

### 异步延迟示例

```lua
function onLoad()
    xtech_misc_wait(function()
        xtech_audio_playSFX(CONST_SFX_GotItem, 0, 128)
    end, 180)

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

---

## 13. 编译说明

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

---

## 14. 自定义角色注册（P8）

C++ 侧已将角色系统从 5 角色/11 状态扩容到 **50 角色/50 状态**。默认只激活 1-5（Mario/Luigi/Peach/Toad/Link），6-50 的槽位预留给 Lua 动态注册。

### 14.1 架构说明

| 概念 | 说明 |
|------|------|
| 角色槽位 | 1-50，通过 `p.Character` 使用 |
| 状态槽位 | 1-50，通过 `p.State` 使用 |
| 资源目录 | `graphics/<name>/<name>-S.png`（S=状态号，1-50） |
| INI 配置 | `<name>-S.ini`（Episode 或关卡自定义目录，可选） |
| 物理参数 | `Physics.PlayerWidth[id][state]` 等，运行时可用 API 修改 |
| 帧偏移 | `PlayerFrameX[id][index]` / `PlayerFrameY[id][index]`，由 INI 的 `[frame-X-Y]` 段自动填充 |
| 纹理 | `GFXCharacterBMP[id-1][state]`，`reload` 时加载 |

### 14.2 注册 API

**文件：** [script/src/xtech_lua_bindings.cpp](script/src/xtech_lua_bindings.cpp)

#### `xtech_player_setName(character, dirname)`

设置角色槽位的资源目录名。必须在 `xtech_player_reload` 之前调用。

| 参数 | 类型 | 说明 |
|------|------|------|
| character | int | 角色 ID（6-50 用于新角色，也可覆盖 1-5） |
| dirname | string | `graphics/` 下的目录名，例如 `"my_char"` → `graphics/my_char/` |

```lua
xtech_player_setName(6, "my_custom_mario")  -- 使用 graphics/my_custom_mario/ 目录
```

#### `xtech_player_reload(character)`

加载指定角色的纹理和 INI 配置。

**纹理加载路径：** `graphics/<dirname>/<dirname>-1.png` 到 `<dirname>-50.png`
**INI 加载路径：** 先搜 Episode 目录，再搜关卡自定义目录，文件格式 `<dirname>-S.ini`

只有文件存在才会加载，缺失静默跳过。

```lua
xtech_player_reload(6)  -- 扫描并加载角色 6 的所有资源
```

#### `xtech_player_setPhysics(character, state, field, value)`

运行时修改物理参数。不持久化（重启关卡后会恢复默认值）。

| 参数 | 类型 | 说明 |
|------|------|------|
| character | int | 角色 ID |
| state | int | 状态 ID（1-50） |
| field | string | 字段名（见下表） |
| value | number | 新值（整数，内部向下取整） |

**支持的 field 值：**

| field | 对应 C++ 字段 | 默认值 |
|-------|-------------|-------|
| `"width"` | `Physics.PlayerWidth[char][state]` | 0 |
| `"height"` | `Physics.PlayerHeight[char][state]` | 0 |
| `"duckHeight"` | `Physics.PlayerDuckHeight[char][state]` | 0 |
| `"grabX"` | `Physics.PlayerGrabSpotX[char][state]` | 0 |
| `"grabY"` | `Physics.PlayerGrabSpotY[char][state]` | 0 |
| `"accX"` | `Physics.PlayerAccessoryOffsetX[char][state]` | 0 |
| `"accY"` | `Physics.PlayerAccessoryOffsetY[char][state]` | 0 |

### 14.3 INI 文件格式

放在 Episode 目录或关卡自定义目录下，文件名如 `my_char-1.ini`：

```ini
[common]
width = 24
height = 32
height-duck = 20
grab-offset-x = 2
grab-offset-y = -4
accessory-offset-x = 0
accessory-offset-y = 0
propeller-climb-offset = 3
propeller-climb-frames = 4
accessory-climb-offset-x = 0
accessory-climb-offset-y = 0

[frame-0-4]
used = true
offsetX = 2
offsetY = -6
```

`[frame-X-Y]` 段对应 10×10 精灵网格坐标（X=0..9, Y=0..9）。`state*100` 偏移自动处理，不需要手动指定状态号。

### 14.4 完整示例

以下示例注册角色 6，加载资源，实现自定义跳跃：

```lua
-- level.lua

function onLoad()
    -- 注册自定义角色（槽位 6）
    xtech_player_setName(6, "my_char")
    xtech_player_reload(6)

    -- 设置物理参数
    xtech_player_setPhysics(6, 1, "width", 24)    -- 小状态宽度
    xtech_player_setPhysics(6, 1, "height", 32)   -- 小状态高度
    xtech_player_setPhysics(6, 2, "width", 28)    -- 大状态宽度
    xtech_player_setPhysics(6, 2, "height", 48)   -- 大状态高度
end

-- 自定义状态常量
local STATE_SMALL = 1
local STATE_SUPER = 2

function onLoop()
    local p = xtech_player_get(1)
    if not p or p.Character ~= 6 then return end

    -- 自定义跳跃
    if p.Controls.Jump and p.StandingOnNPC > 0 then
        -- 从 INI 读不到跳跃速度时可运行时设置
        -- 注意：JumpVelocity 是全局参数，不按角色分
    end

    -- 自定义动画帧
    if p.State == STATE_SUPER then
        -- 可以改变 p.Frame 来控制精灵
    end
end
```

### 14.5 资源目录布局

```
Episode/
├── my_char-1.ini          # 状态 1 的 INI（Episode 级别）
├── my_char-2.ini          # 状态 2 的 INI
├── 1-1 My Level/
│   ├── level.lua          # 关卡脚本
│   └── my_char-1.ini      # 关卡级别覆盖 INI（优先级最高）
└── graphics/
    └── my_char/
        ├── my_char-1.png  # 状态 1 的精灵图（建议 1000×? 布局）
        ├── my_char-2.png  # 状态 2 的精灵图
        └── ...
```

### 14.6 在当前存档中使用自定义角色

角色 6-50 默认为零值状态。要让玩家以自定义角色开始关卡：

**方法 A：Lua 强制切换**

```lua
function onLoad()
    local p = xtech_player_get(1)
    if p then
        p.Character = 6
        p.State = 1
    end
end
```

**方法 B：Future** — 后续可通过关卡起始点编辑器或 Episode 配置指定起始角色。

### 14.7 限制

- **角色专属逻辑（C++ 侧 82 处 `if(p.Character == N)`）**：新角色不匹配，行为回退到默认（类似 Mario 但无特殊能力）。所有特殊能力需在 Lua `onLoop` 中自行实现
- **帧偏移**：目前只能通过 INI 文件设置，没有 Lua API 直接写 `PlayerFrameX/Y`
- **跳跃速度/重力**：这些是全局参数（`Physics.PlayerJumpVelocity` 等），不按角色区分，修改会影响所有角色
- **角色名纹理**：`CharacterName` 只加载 1-5，新角色在 HUD 名字显示上需要自行用 `showText` 处理
- **声音/特效**：角色专属音效和死亡特效需要 Lua 事件钩子处理
