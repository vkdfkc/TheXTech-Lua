# TheXTech Lua 自定义 HUD 完整指南

## 概述

本文档介绍如何使用 Lua 脚本完全隐藏游戏内建 HUD，并利用现有的 API 系统（精灵、文字、Sysval、Player 对象）构建你自己的 HUD。

---

## 1. 核心流程

> **新增 `ShowInterface` 系统变量（默认 `true`）：** 设置为 `false` 时仅隐藏游戏内建界面（`DrawInterface`），`onRenderHud` 照常触发。相比 `ShowOnScreenHUD`（会同时禁用 `onRenderHud`），这是自定义 HUD 的正确开关。

### 推荐方案：隐藏内建界面 + 用 `onRenderHud` 绘制

```
onLoad()            → xtech_sysval_setShowInterface(false)，加载图像资源
onRenderHud(Z)      → 每帧在 PLANE_LVL_HUD 顶层绘制自定义 UI
onLoopEnd()         → 可选的清理工作
```

```lua
function onLoad()
    xtech_sysval_setShowInterface(false)   -- 仅隐藏内建 HUD，保留 onRenderHud
end

function onRenderHud(Z)
    -- Z 是当前渲染的屏幕层
    -- PLANE_LVL_HUD 保证你的 UI 渲染在最顶层
end
```

### 备选方案：完全隐藏 HUD 平面 + 用 `onRender` 绘制

如果你希望禁用整个 HUD 平面（节省性能），仍可使用旧方案：

```lua
function onLoad()
    xtech_sysval_setShowHud(false)    -- 禁用整个 PLANE_LVL_HUD（含 onRenderHud）
end

function onRender(Z)
    -- 在主场景渲染阶段绘制 HUD
    -- StaticScreenPos=true 的精灵在此仍正常渲染
end
```

### 1.3 渲染架构说明

以下是 `gfx_update.cpp` 中的渲染调用顺序及各开关的控制范围：

```
[主场景渲染阶段]
  ...
  onRender(Z)              ← 不受任何 HUD 开关影响

[PLANE_LVL_HUD 阶段]      ← 最顶层（0xA0）
  if(ShowOnScreenHUD) {
      onRenderHud(Z)       ← 受 ShowOnScreenHUD 控制
      if(ShowInterface) {  ← 受 ShowInterface 控制（新增）
          DrawInterface(Z) ← 原始游戏 HUD（生命/金币/分数等）
      }
  }
```

**开关控制矩阵：**

| ShowOnScreenHUD | ShowInterface | onRenderHud | 内建 HUD |
|---|---|---|---|
| `true` | `true` | ✅ 触发 | ✅ 显示 |
| `true` | `false` | ✅ 触发 | ❌ 隐藏 |
| `false` | `true` | ❌ 不触发 | ❌ 隐藏 |
| `false` | `false` | ❌ 不触发 | ❌ 隐藏 |

---

## 2. 获取游戏数据

### 2.1 Sysval 全局数据

| 函数 | 类型 | 说明 |
|---|---|---|
| `xtech_sysval_getLives()` | int | 当前生命数 |
| `xtech_sysval_setLives(n)` | void | 设置生命数 |
| `xtech_sysval_getCoins()` | int | 当前金币数 |
| `xtech_sysval_setCoins(n)` | void | 设置金币数 |
| `xtech_sysval_getScore()` | int | 当前分数 |
| `xtech_sysval_setScore(n)` | void | 设置分数 |
| `xtech_sysval_getGameTime()` | int | 游戏运行总帧数（`CommonFrame`，约 65fps） |
| `xtech_sysval_getScreenX(playerId)` | int | 指定玩家在屏幕上的 X 坐标 |
| `xtech_sysval_getScreenY(playerId)` | int | 指定玩家在屏幕上的 Y 坐标 |
| `xtech_sysval_getShowHud()` | bool | 当前 HUD 平面是否可见（同时控制 onRenderHud） |
| `xtech_sysval_setShowHud(bool)` | void | 设置 HUD 平面可见性 |
| `xtech_sysval_getShowInterface()` | bool | 仅控制内建游戏界面（不影响 onRenderHud） |
| `xtech_sysval_setShowInterface(bool)` | void | 设置内建界面可见性（推荐用于自定义 HUD） |
| `xtech_sysval_getBattleMode()` | bool | 是否为对战模式 |
| `xtech_sysval_getEndLevel()` | bool | 是否正在结束关卡 |
| `xtech_sysval_setEndLevel(bool)` | void | 强制结束关卡 |
| `xtech_sysval_getLevelMacro()` | int | 关卡结束宏类型 |
| `xtech_sysval_getLevelMacroCounter()` | int | 结束宏计数器 |

### 2.2 Player 对象字段

通过 `xtech_player_get(1)` 或 `xtech_player_get(2)` 获取玩家对象。

#### 角色 / 形态

| 字段 | 类型 | 说明 |
|---|---|---|
| `Character` | int | 0=Mario, 1=Luigi, 2=Peach, 3=Toad, 4=Link |
| `State` | int | 1=小形态, 2=超级, 3=火, 4=狸猫, 5=石像鬼, 6=锤子 |
| `Hearts` | int | 爱心数（爱心生命系统） |
| `Bombs` | int | 炸弹数 |
| `HasKey` | bool | 是否持有钥匙 |
| `Fairy` | bool | 仙灵状态 |
| `FairyTime` | int | 仙灵剩余帧数 |
| `FairyCD` | int | 仙灵冷却 |
| `HeldBonus` | int | 备选道具（物品栏） |
| `Dead` | bool | 是否死亡 |
| `TimeToLive` | int | 死亡动画剩余帧 |
| `Immune` / `Immune2` | bool | 无敌状态 |

#### 坐骑

| 字段 | 类型 | 说明 |
|---|---|---|
| `Mount` | bool | 是否骑乘中 |
| `MountType` | int | 0=无, 1=靴子, 2=小丑车, 3=耀西 |
| `MountSpecial` | int | 坐骑特殊数据 |
| `MountOffsetY` | int | 坐骑 Y 偏移 |
| `MountFrame` | int | 坐骑当前帧 |
| `Dismount` | int | 下马标记 |

#### 耀西专用字段

| 字段 | 类型 |
|---|---|
| `YoshiYellow`, `YoshiBlue`, `YoshiRed` | bool |
| `YoshiTongueLength` | int |
| `YoshiNPC`, `YoshiPlayer` | int |
| `YoshiTX`, `YoshiTY`, `YoshiBX`, `YoshiBY` | 坐标 |
| `YoshiTFrame`, `YoshiBFrame` | 动画帧 |
| `YoshiWingsFrame`, `YoshiWingsFrameCount` | 翅膀动画 |

#### 移动 / 能力

| 字段 | 类型 | 说明 |
|---|---|---|
| `CanFly` / `CanFly2` | bool | 飞行能力 |
| `FlyCount` | int | 飞行计数器 |
| `CanFloat` | bool | 浮空（桃花公主） |
| `FloatTime` | int | 浮空剩余帧 |
| `SpinJump` | bool | 旋转跳 |
| `Slide` | bool | 滑行 |
| `Driving` | bool | 驾驶小丑车 |
| `Vine` | bool | 爬藤 |
| `ShellSurf` | bool | 壳上滑行 |
| `Rolling` | bool | 滚动 |
| `Duck` | bool | 蹲下 |
| `Stoned` | bool | 石化 |
| `Wet` | bool | 进入水中 |
| `Quicksand` | int | 流沙计数器 |
| `NoGravity` | bool | 无重力 |
| `Slippy` | bool | 滑溜状态 |
| `DoubleJump` | bool | 二段跳 |

#### 位置 / 区域

| 字段 | 类型 | 说明 |
|---|---|---|
| `Location.X` | num_t | 世界 X 坐标 |
| `Location.Y` | num_t | 世界 Y 坐标 |
| `Location.SpeedX` | num_t | 水平速度 |
| `Location.SpeedY` | num_t | 垂直速度 |
| `Location.Width` | num_t | 碰撞宽度 |
| `Location.Height` | num_t | 碰撞高度 |
| `Section` | int | 所在区域编号 |
| `Direction` | int | 朝向 (-1=左, 1=右) |

#### 按键状态（通过函数查询）

| 函数 | 返回值 |
|---|---|
| `xtech_player_pressingDown(p)` | bool |
| `xtech_player_pressingUp(p)` | bool |
| `xtech_player_pressingLeft(p)` | bool |
| `xtech_player_pressingRight(p)` | bool |
| `xtech_player_pressingJump(p)` | bool |
| `xtech_player_pressingRun(p)` | bool |
| `xtech_player_pressingSEL(p)` | bool |

#### 辅助函数

| 函数 | 说明 |
|---|---|
| `xtech_player_usesHearts(p)` | 检查玩家是否使用爱心生命系统 |
| `xtech_player_isHoldingSpriteType(p, npcId)` | 检查玩家是否手持指定类型 NPC/物品 |

---

## 3. 绘制自定义 UI 元素

### 3.1 文字绘制

```lua
xtech_hud_showText("text", x, y, font)       -- 在屏幕坐标 (x, y) 显示文字，font=3 为游戏默认字体
xtech_hud_showLevelName(x, y, font)           -- 显示关卡名称
xtech_hud_showLevelFile(x, y, font)           -- 显示关卡文件名
xtech_hud_debugPrint("message")               -- 输出调试信息到游戏日志
```

### 3.2 图像绘制（精灵系统）

#### 加载图像

```lua
-- 完整版：指定透明色
xtech_sprite_loadImage("my_hud.png", 1000, 0xFF00DC)   -- 0xFF00DC = 品红透明

-- 简化版：使用默认透明色
xtech_sprite_loadImageSimple("my_hud.png", 1000)
```

- 图像从关卡自定义目录加载
- `1000` 是自定义资源代码（你自己分配的整数 ID，需要保证唯一）

#### 放置精灵到屏幕

```lua
-- 基础放置
spr = xtech_sprite_place(type, imgResourceCode, x, y, lifetime)

-- 扩展放置（带速度和出生标记）
spr = xtech_sprite_placeExt(type, imgResourceCode, x, y, lifetime, xspd, yspd, spawned)
```

对于 HUD 元素，**始终使用 `CONST_SPRITE_STATIC` + `StaticScreenPos = true`**：

```lua
local spr = xtech_sprite_place(CONST_SPRITE_STATIC, 1000, 400, 300, 1)
if spr then
    spr.StaticScreenPos = true       -- 屏幕坐标（不跟随世界移动）
    spr.DrawPriorityLevel = 10       -- 绘制优先级
    spr.Visible = true
end
```

#### 精灵类型常量

| 常量 | 值 | 用途 |
|---|---|---|
| `CONST_SPRITE_CUSTOM` | 0 | 自定义精灵 |
| `CONST_SPRITE_STATIC` | 1 | **HUD 元素 — 屏幕坐标** |
| `CONST_SPRITE_NORMAL` | 2 | 世界空间精灵 |
| `CONST_SPRITE_ITEM` | 3 | 物品精灵 |
| `CONST_SPRITE_BAR` | 4 | 进度条 |
| `CONST_SPRITE_PHANTO` | 5 | 幻影精灵 |

#### CSprite 完整字段参考

| 字段 | 类型 | 读写 | 说明 |
|---|---|---|---|
| `ImgResCode` | int | R/W | 图像资源代码 |
| `Xpos` | num_t | R/W | X 坐标（StaticScreenPos=true 时为屏幕坐标） |
| `Ypos` | num_t | R/W | Y 坐标 |
| `Wd` | num_t | R/W | 宽度 |
| `Ht` | num_t | R/W | 高度 |
| `Xspd` | num_t | R/W | X 速度 |
| `Yspd` | num_t | R/W | Y 速度 |
| `StaticScreenPos` | bool | R/W | **true=屏幕坐标, false=世界坐标** |
| `Visible` | bool | R/W | 可见性 |
| `DrawPriorityLevel` | int | R/W | 绘制优先级（层级控制） |
| `GfxXOffset` | num_t | R/W | 图像 X 偏移 |
| `GfxYOffset` | num_t | R/W | 图像 Y 偏移 |
| `FrameCounter` | int | R/W | 帧计数器 |
| `AnimationFrame` | int | R/W | 当前动画帧 |
| `AnimationTimer` | num_t | R/W | 动画计时器 |
| `AnimationPhase` | int | R/W | 动画阶段 |
| `AnimationSet` | int | R/W | 动画集索引 |
| `FramesLeft` | int | R/W | 剩余生命帧数 |
| `Birthed` | bool | R/W | 已出生 |
| `Died` | bool | R/W | 已死亡 |
| `Invalidated` | bool | R/W | 已失效 |
| `AlwaysProcess` | bool | R/W | 是否始终处理 |
| `Hitbox` | Hitbox | R/W | 碰撞盒 |
| `CollisionCode` | int | R/W | 碰撞代码 |
| `OffscreenCount` | int | R/W | 屏幕外计数 |

**方法：**

| 方法 | 说明 |
|---|---|
| `spr:setImage(img)` | 设置 LunaImage 图像 |
| `spr:setImageResource(code)` | 按资源代码设置图像 |
| `spr:makeLimitedLife(frames)` | 设置有限生命（frames 帧后自毁） |
| `spr:setCustomVar(name, op, value)` | 设置自定义变量 |
| `spr:customVarExists(name)` | 检查自定义变量 |
| `spr:getCustomVar(name)` | 获取自定义变量值 |
| `spr:birth()` | 标记为出生 |
| `spr:die()` | 标记为死亡 |

#### 获取已加载图像的信息

```lua
local img = xtech_sprite_getImage(1000)
if img and img:isLoaded() then
    local w = img:getWidth()
    local h = img:getHeight()
    local uid = img:getUID()
end
```

#### 删除图像和清理精灵

```lua
xtech_sprite_deleteImage(1000)           -- 删除指定资源代码的图像
xtech_sprite_clearAll()                  -- 清除所有活跃精灵
xtech_sprite_clearByCode(1000)           -- 清除指定资源代码的精灵
count = xtech_sprite_count()             -- 获取所有活跃精灵数
```

### 3.3 蓝图系统

用于创建可重复使用的精灵模板：

```lua
-- 注册一个精灵蓝图
spr = xtech_sprite_place(CONST_SPRITE_STATIC, 1000, 0, 0, 1)
xtech_sprite_addBlueprint("coin_icon", spr)

-- 之后按蓝图快速创建副本
local newSpr = xtech_sprite_copyFromBlueprint("coin_icon")
if newSpr then
    newSpr.Xpos = 200
    newSpr.Ypos = 100
    newSpr.StaticScreenPos = true
end
```

---

## 4. 时间与帧计数

| 数据源 | 说明 |
|---|---|
| `xtech_sysval_getGameTime()` | 游戏总帧数 (`CommonFrame`)，约 65fps |
| `xtech_misc_getFrame()` | 当前帧计数 |

帧 → 秒转换（近似）：

```lua
local seconds = math.floor(xtech_sysval_getGameTime() / 65)
local mins = math.floor(seconds / 60)
local secs = seconds % 60
local timeStr = string.format("%d:%02d", mins, secs)
```

> **注意：** 目前没有直接的"关卡剩余时间"API。如果关卡设置了时间限制，你需要从 `onLoad` 开始自行用定时器追踪倒计时。

---

## 5. 音频播放

自定义 HUD 常伴随音效反馈：

```lua
-- 内置音效
xtech_audio_playSFX(CONST_SFX_Coin, 0, 128)        -- 音效ID, 循环, 音量
xtech_audio_playSFX(CONST_SFX_GotItem, 0, 128)
xtech_audio_playSFX(CONST_SFX_1up, 0, 128)

-- 自定义音效（关卡目录）
xtech_audio_playSFXExt("my_sound.wav", 0, 128)
xtech_audio_stopSFXExt("my_sound.wav")
xtech_audio_preloadSFXExt("my_sound.wav")

-- 音乐切换
xtech_section_setMusic(sectionIndex, musicId)
xtech_section_setMusicFile(sectionIndex, "filename")
xtech_audio_playMusic(sectionIndex, fadeInMs)
xtech_audio_playMusicFile("filename", fadeInMs)
```

---

## 6. 完整示例

```lua
-- ============================================================
-- 完整自定义 HUD 示例
-- 显示生命、金币、分数、时间、形态、坐骑、道具栏
-- ============================================================

-- === 资源代码分配 ===
local IMG_HUD_FRAME   = 1000   -- HUD 背景框
local IMG_HUD_HEART   = 1001   -- 生命图标
local IMG_HUD_COIN    = 1002   -- 金币图标
local IMG_HUD_STAR    = 1003   -- 分数图标
local IMG_HUD_CLOCK   = 1004   -- 时间图标
local IMG_HUD_YOSHI   = 1005   -- 耀西图标
local IMG_HUD_BOOT    = 1006   -- 靴子图标
local IMG_HUD_CLOWN   = 1007   -- 小丑车图标
local IMG_HUD_KEY     = 1008   -- 钥匙图标

-- 道具栏图标（以 HeldBonus 值为偏移）
local IMG_POWERUP_BASE = 2000

-- === 辅助表 ===
local STATE_NAMES = {
    [1] = "Small", [2] = "Super", [3] = "Fire",
    [4] = "Raccoon", [5] = "Tanooki", [6] = "Hammer"
}
local MOUNT_NAMES = {
    [0] = "None", [1] = "Boot", [2] = "Clown Car", [3] = "Yoshi"
}
local MOUNT_ICONS = {
    [1] = IMG_HUD_BOOT, [2] = IMG_HUD_CLOWN, [3] = IMG_HUD_YOSHI
}
local CHAR_NAMES = {
    [0] = "Mario", [1] = "Luigi", [2] = "Peach", [3] = "Toad", [4] = "Link"
}

-- === 布局常量 ===
local HUD_X = 32
local HUD_Y = 16
local LINE_H = 22

-- ============================================================
function onLoad()
    -- 1) 隐藏内建 HUD（保留 onRenderHud）
    xtech_sysval_setShowInterface(false)

    -- 2) 加载所有 HUD 图像
    xtech_sprite_loadImage("hud_frame.png", IMG_HUD_FRAME, 0xFF00DC)
    xtech_sprite_loadImage("hud_heart.png", IMG_HUD_HEART, 0xFF00DC)
    xtech_sprite_loadImage("hud_coin.png",  IMG_HUD_COIN,  0xFF00DC)
    xtech_sprite_loadImage("hud_star.png",  IMG_HUD_STAR,  0xFF00DC)
    xtech_sprite_loadImage("hud_clock.png", IMG_HUD_CLOCK, 0xFF00DC)

    -- 坐骑图标
    xtech_sprite_loadImage("hud_yoshi.png", IMG_HUD_YOSHI, 0xFF00DC)
    xtech_sprite_loadImage("hud_boot.png",  IMG_HUD_BOOT,  0xFF00DC)
    xtech_sprite_loadImage("hud_clown.png", IMG_HUD_CLOWN, 0xFF00DC)

    -- 钥匙
    xtech_sprite_loadImage("hud_key.png", IMG_HUD_KEY, 0xFF00DC)

    -- 道具图标
    xtech_sprite_loadImage("items/mushroom.png",   IMG_POWERUP_BASE + 1, 0xFF00DC)
    xtech_sprite_loadImage("items/fireflower.png", IMG_POWERUP_BASE + 2, 0xFF00DC)
    xtech_sprite_loadImage("items/leaf.png",       IMG_POWERUP_BASE + 3, 0xFF00DC)
    xtech_sprite_loadImage("items/star.png",       IMG_POWERUP_BASE + 4, 0xFF00DC)

    xtech_misc_log("[CustomHUD] Initialized with " ..
        xtech_sysval_getLives() .. " lives")
end

-- ============================================================
-- 放置静态 HUD 精灵的辅助函数
local function placeStaticHudIcon(code, x, y, priority)
    local spr = xtech_sprite_place(CONST_SPRITE_STATIC, code, x, y, 1)
    if spr then
        spr.StaticScreenPos = true
        spr.DrawPriorityLevel = priority or 2
        spr.Visible = true
    end
    return spr
end

-- ============================================================
function onRenderHud(screenZ)
    local p = xtech_player_get(1)
    if not p then return end

    local y = HUD_Y

    -- ===== HUD 背景框 =====
    placeStaticHudIcon(IMG_HUD_FRAME, HUD_X, y, 1)

    -- ===== 关卡名称 =====
    xtech_hud_showLevelName(HUD_X + 200, y + 4, 3)

    -- ===== 玩家角色 + 形态 =====
    local charName = CHAR_NAMES[p.Character] or "?"
    local stateName = STATE_NAMES[p.State] or "?"
    xtech_hud_showText(charName .. " (" .. stateName .. ")", HUD_X + 200, y + 30, 3)

    -- ===== 生命数 =====
    placeStaticHudIcon(IMG_HUD_HEART, HUD_X + 10, y + 60, 2)
    xtech_hud_showText("x " .. xtech_sysval_getLives(), HUD_X + 42, y + 60, 3)

    -- ===== 金币 =====
    placeStaticHudIcon(IMG_HUD_COIN, HUD_X + 10, y + 86, 2)
    xtech_hud_showText("x " .. xtech_sysval_getCoins(), HUD_X + 42, y + 86, 3)

    -- ===== 分数 =====
    placeStaticHudIcon(IMG_HUD_STAR, HUD_X + 10, y + 112, 2)
    xtech_hud_showText("" .. xtech_sysval_getScore(), HUD_X + 42, y + 112, 3)

    -- ===== 时间 =====
    local totalFrames = xtech_sysval_getGameTime()
    local seconds = math.floor(totalFrames / 65)
    local mins = math.floor(seconds / 60)
    local secs = seconds % 60
    placeStaticHudIcon(IMG_HUD_CLOCK, 600, y + 60, 2)
    xtech_hud_showText(string.format("%d:%02d", mins, secs), 632, y + 60, 3)

    -- ===== 坐骑状态 =====
    if p.Mount ~= 0 and MOUNT_ICONS[p.MountType] then
        placeStaticHudIcon(MOUNT_ICONS[p.MountType], HUD_X + 200, y + 60, 2)
        xtech_hud_showText(MOUNT_NAMES[p.MountType] or "?", HUD_X + 232, y + 60, 3)
    end

    -- ===== 钥匙 =====
    if p.HasKey then
        placeStaticHudIcon(IMG_HUD_KEY, HUD_X + 200, y + 86, 2)
        xtech_hud_showText("Key", HUD_X + 232, y + 86, 3)
    end

    -- ===== 爱心 / 炸弹 =====
    if xtech_player_usesHearts(p) then
        xtech_hud_showText("HP: " .. p.Hearts, HUD_X + 200, y + 112, 3)
    end
    if p.Bombs > 0 then
        xtech_hud_showText("Bombs: " .. p.Bombs, HUD_X + 300, y + 112, 3)
    end

    -- ===== 仙灵状态 =====
    if p.Fairy then
        local fairySecs = math.floor(p.FairyTime / 65)
        xtech_hud_showText("Fairy: " .. fairySecs .. "s", HUD_X + 400, y + 60, 3)
    end

    -- ===== 道具栏（备选道具） =====
    if p.HeldBonus > 0 then
        local bonusIconCode = IMG_POWERUP_BASE + p.HeldBonus
        placeStaticHudIcon(bonusIconCode, 700, y + 60, 2)
        xtech_hud_showText("Item", 732, y + 60, 3)
    end
end

-- ============================================================
function onLoopEnd()
    xtech_misc_log("[CustomHUD] Level ended, cleaning up")
end
```

---

## 7. 高级技巧

### 7.1 帧性能优化

`onRenderHud` **每帧都会调用一次**。为避免性能问题：

- 不要在 `onRenderHud` 中每帧调用 `xtech_sprite_place` 创建新精灵。应当在 `onLoad` 中创建精灵，存储引用，然后在 `onRenderHud` 中通过引用更新位置/可见性。
- 将不变的值缓存在 `onLoad` 中或使用帧计数器限制更新频率。

```lua
-- ❌ 低效：每帧创建新精灵
function onRenderHud(Z)
    for i = 1, xtech_npc_count() do ... end   -- 每帧遍历所有 NPC
    xtech_sprite_place(CONST_SPRITE_STATIC, 1000, x, y, 1)  -- 每帧创建图片
end

-- ✅ 高效：缓存 + 按需更新
local hudSprites = {}
function onLoad()
    -- 在 onLoad 中创建好所有 HUD 精灵
    local spr = xtech_sprite_place(CONST_SPRITE_STATIC, 1000, 400, 300, 0)
    spr.StaticScreenPos = true
    hudSprites.bg = spr
end

local frameCounter = 0
function onRenderHud(Z)
    frameCounter = frameCounter + 1
    if frameCounter % 10 ~= 1 then return end  -- 每 10 帧更新一次足矣

    -- 更新现有精灵的属性（而非创建新精灵）
    if hudSprites.bg then
        hudSprites.bg.Visible = true
    end
end
```

### 7.2 帧计数器限制重绘

对于不需要每帧更新的信息（如分数、生命数），使用帧计数器降低刷新率：

```lua
local tick = 0
function onRenderHud(Z)
    tick = tick + 1
    if tick % 15 ~= 1 then return end   -- 每 15 帧（约 0.23 秒）更新一次
    -- 你的 HUD 更新逻辑
end
```

### 7.3 多人 HUD

```lua
function onRenderHud(Z)
    for i = 1, 2 do
        local p = xtech_player_get(i)
        if p then
            -- 为每个玩家分别绘制（偏移 HUD 位置）
            local offsetX = (i - 1) * 400
            -- ... 绘制玩家 i 的 HUD 元素
        end
    end
end
```

### 7.4 蓝图批量创建

```lua
function onLoad()
    -- 创建模板精灵并注册为蓝图
    local template = xtech_sprite_place(CONST_SPRITE_STATIC, IMG_HUD_HEART, 0, 0, 0)
    template.StaticScreenPos = true
    xtech_sprite_addBlueprint("heart_template", template)

    -- 批量创建生命图标
    for i = 1, xtech_sysval_getLives() do
        local heart = xtech_sprite_copyFromBlueprint("heart_template")
        if heart then
            heart.Xpos = 50 + i * 24
            heart.Ypos = 30
            heart.Visible = true
        end
    end
end
```

### 7.5 组合使用延迟回调

```lua
function onLoad()
    xtech_sysval_setShowHud(false)       -- 隐藏原始 HUD

    -- 48 帧后显示关卡标题
    xtech_misc_wait(function()
        xtech_hud_showLevelName(400, 300, 3)

        -- 再过 120 帧后隐藏标题（使用命名定时器）
        xtech_timer_create("hideTitle", function()
            xtech_hud_showLevelName(400, 300, -1)  -- 不再绘制
        end, 120)
    end, 48)
end
```

---

## 8. 常见常量参考

### 玩家形态

| 常量 | 值 | 说明 |
|---|---|---|
| `CONST_PLAYER_SMALL` | 1 | 小形态 |
| `CONST_PLAYER_SUPER` | 2 | 超级形态 |
| `CONST_PLAYER_FIRE` | 3 | 火形态 |
| `CONST_PLAYER_RACCOON` | 4 | 狸猫 |
| `CONST_PLAYER_TANOOKI` | 5 | 石像鬼 |
| `CONST_PLAYER_HAMMER` | 6 | 锤子 |

### 坐骑类型

| 常量 | 值 | 说明 |
|---|---|---|
| `CONST_MOUNT_NONE` | 0 | 无坐骑 |
| `CONST_MOUNT_BOOT` | 1 | 弹簧靴 |
| `CONST_MOUNT_CLOWN_CAR` | 2 | 小丑车 |
| `CONST_MOUNT_YOSHI` | 3 | 耀西 |

### 方向

| 常量 | 值 |
|---|---|
| `CONST_DIR_UP` | 1 |
| `CONST_DIR_RIGHT` | 2 |
| `CONST_DIR_DOWN` | 3 |
| `CONST_DIR_LEFT` | 4 |

### 碰撞方向

| 常量 | 值 |
|---|---|
| `CONST_COLTYPE_NONE` | 0 |
| `CONST_COLTYPE_LEFT` | 1 |
| `CONST_COLTYPE_RIGHT` | 2 |
| `CONST_COLTYPE_TOP` | 3 |
| `CONST_COLTYPE_BOT` | 4 |

---

## 9. 当前限制与替代方案

| 需求 | 现状 | 替代方案 |
|---|---|---|
| **关卡名称字符串** | 仅有 `xtech_hud_showLevelName` 直接渲染，无 getter 返回字符串 | 在 `onLoad` 中手动定义变量 |
| **关卡剩余时间倒计时** | 无直接 API | 从 `onLoad` 开始用帧计数器自行追踪 |
| **道具栏图标映射** | `HeldBonus` 返回数值 | 用 Lua table 映射道具 ID → 精灵资源代码 |
| **角色名 / 形态名字符串** | Character/State 返回数值 | 用 Lua table 映射数字 → 字符串 |
| **世界总分数** | 仅有关卡内 Score | 使用 `xtech_var_get/set` 持久化存储 |
| **关卡内计时器** | 无内建计时器 | 用 `xtech_timer_create` 或 `xtech_misc_wait` 实现 |
| **粒子/特效叠加** | 特效系统（EFFID）在 HUD 层级有限制 | 使用精灵 `CONST_SPRITE_BAR` 类型模拟 |

---

## 10. 总结

| 步骤 | 函数 | 说明 |
|---|---|---|
| 1. 隐藏内建界面 | `xtech_sysval_setShowInterface(false)` | 仅隐藏内建 HUD，保留 onRenderHud |
| 2. 加载图像 | `xtech_sprite_loadImage(...)` | 加载 PNG 等资源 |
| 3. 渲染回调 | `onRenderHud(Z)` | 你的 HUD 绘制入口（PLANE_LVL_HUD 顶层） |
| 4. 绘制图像 | `xtech_sprite_place(CONST_SPRITE_STATIC, ...)` + `StaticScreenPos = true` | 放置屏幕坐标图像 |
| 5. 绘制文字 | `xtech_hud_showText(...)` | 显示文字 |
| 6. 读取数据 | `xtech_sysval_get*()` / `xtech_player_get()` | 获取生命/金币/分数/时间/玩家状态 |
