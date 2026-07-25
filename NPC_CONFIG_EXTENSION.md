# NPC 配置扩展（SMBX-38A 兼容）

## 完成状态

### ✅ 已完成：PGE 解析器 + NPCTraits 数据层（26 个字段）

所有字段均已加入 `NPCConfigFile`（PGE 解析）、`NPCTraits_t`（运行时存储）、`LoadCustomNPC`（配置应用）。

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `nohammer` | bool | false | 免疫锤子/斧头 |
| `noshell` | bool | false | 免疫投掷龟壳 |
| `nolava` | bool | false | 免疫岩浆 |
| `noleaf` | bool | false | 免疫狸猫尾巴 |
| `noblockhit` | bool | false | 免疫方块顶出伤害 |
| `spinjump` | bool | true | 允许旋转跳踩踏 |
| `spinjumphurt` | bool | false | 旋转跳导致玩家受伤 |
| `waterjumphurt` | bool | false | 水下跳跃导致玩家受伤 |
| `yoshihurt` | bool | false | 耀西踩踏导致玩家受伤 |
| `instantkill` | bool | false | 接触即死 |
| `immortal` | bool | false | 无法被杀死（自动重生） |
| `groundpoundbreak` | bool | false | 压地可破坏 |
| `megabreak` | bool | false | 巨大形态可破坏 |
| `turboweak` | bool | false | 冲刺水泵强制龟壳伤害 |
| `nosectionwrap` | bool | false | 无视场景循环边界 |
| `npcblockside` | bool | false | 对 NPC 侧向阻挡（独立于 npcblock） |
| `float` | bool | false | 浮于水面 |
| `nopiercingdmg` | bool | false | 免疫穿透伤害（摧毁投掷物） |
| `pushable` | bool | false | 可被玩家推动 |
| `stackable` | bool | false | 可堆叠（站在同类 NPC 上不移动） |
| `canmeltblock` | bool | false | 可融化冰块 |
| `wingsforever` | bool | false | 受伤后翅膀不消失 |
| `frozentime` | int | 0 | 冻结持续帧数（-1 = 永不冻结） |
| `yoshitransform` | int | 0 | 被耀西吞下后转换为 NPC ID |
| `frozentransform` | int | 0 | 被冻结后转换为 NPC ID |
| `jumptransform` | int | 0 | 被踩踏后转换为 NPC ID |

### ✅ 已完成：简单的免疫类运行时逻辑

| 字段 | 触发文件 | 逻辑 |
|------|---------|------|
| `NoYoshi` | `player.cpp:3322` | 已有，耀西吞食跳过 |
| `NoFireBall` | NPC hit 代码（预存在） | 已有 |
| `NoIceBall` | NPC hit 代码（预存在） | 已有 |

### ⬜ 未完成：需要运行时逻辑的字段

以下字段数据层已完成，但**缺少游戏逻辑消费代码**：

#### 高优先级（可直接参照 NoFireBall/NoIceBall 模式添加，预估 1-2 小时）

| 字段 | 需修改的位置 |
|------|------------|
| `NoHammer` | NPC 受锤子/斧头伤害处 |
| `NoShell` | NPC 受龟壳/投掷物伤害处 |
| `NoLava` | NPC 接触岩浆伤害处 |
| `NoLeaf` | NPC 受尾巴旋转伤害处 |
| `NoBlockHit` | NPC 受方块顶出伤害处 |
| `SpinJump` / `SpinJumpHurt` | 玩家旋转跳 NPC 交互处 |
| `WaterJumpHurt` | 水下跳跃 NPC 交互处 |
| `YoshiHurt` | 骑耀西踩踏 NPC 交互处 |
| `InstantKill` | 玩家-NPC 接触判定处 |
| `Immortal` | NPC 死亡处理处（KillNPC / NPCHit） |
| `WingsForever` | NPC 受伤掉翅膀处 |
| `NoSectionWrap` | 场景边界检查处 |

#### 中等优先级（已有类似实现可参考）

| 字段 | 可参考的已有逻辑 | 需修改的位置 |
|------|----------------|------------|
| `Pushable` | 龟壳推动逻辑 (`player_npc_logic.cpp` IsAShell 路径) | 将 `IsAShell` 检查扩展为 `IsAShell \|\| Pushable` |
| `CanMeltBlock` | 火球融冰逻辑 | NPC 触碰冰块检测处 |
| `NoPiercingDmg` | 龟壳/投掷物销毁逻辑 | NPC 受穿透伤害处 |
| `Stackable` | 部分 NPC 的静止行为 | NPC AI 更新处 |
| `GroundpoundBreak` | 压地破坏砖块逻辑 | 压地震动检测处 |
| `MegaBreak` | 巨大形态破坏逻辑 | 形态碰撞检测处 |
| `TurboWeak` | 龟壳伤害计算 | 冲刺伤害判定处 |
| `Float` | 水中浮力逻辑 | 物理更新处 |
| `NpcBlockSide` | `NpcBlock` 的侧向版本 | NPC-NPC 碰撞检测处 |

#### 低优先级（需额外设计）

| 系统 | 字段 | 说明 |
|------|------|------|
| 变形系统 | `FrozenTime`, `YoshiTransform`, `FrozenTransform`, `JumpTransform` | 需在 NPC kill/spit 流程中插入变形逻辑，创建新 NPC 替换旧 NPC |
| 小碰撞体 | `SmallHitbox` | 需新增碰撞体计算分支（SMBX 1.3 兼容） |
| 迷你怪物 | `MiniEnemy` | 需新增爬附玩家机制 |
| 无NPC碰撞 | `NoNPCCollision` | 需在 NPC-NPC 碰撞检测中跳过 |
| 按P开关 | `CanPressPSwitch` | 需在 P 开关交互中检查 |
| NPC生成器 | `HoldGenerator` | 需新增定时生成子系统 |
| 渲染器 | `Z-Position`, `DirectiveGFX`, `BlendMode` | 需渲染后端支持 |

### ⬜ 未做：38A 版本中的以下字段

这些字段在 SMBX-38A 中存在但尚未加入：

| 字段 | 说明 | 原因 |
|------|------|------|
| `SmallHitbox` | 小碰撞体 | 需碰撞系统改动 |
| `MiniEnemy` | 迷你怪物 | 需新游戏机制 |
| `NoNPCCollision` | 无 NPC 碰撞 | 需碰撞系统改动 |
| `CanPressPSwitch` | 按 P 开关 | 需新机制 |
| `HoldGenerator` | NPC 生成器 | 需新子系统 |
| `Scripts` | 执行脚本 | Lua 已有钩子可替代 |
| `Z-Position` | Z 层级 | 需渲染后端改动 |
| `DirectiveGFX` | 随方向旋转图像 | 需渲染后端改动 |
| `BlendMode` | 渲染模式 | 需渲染后端改动 |
| `ForceCheck` | 强制检测 | 语义不明 |
| 伤害值字段 | FireBallDmg, JumpDmg, ShellDmg 等 | 需伤害系统重构 |
| `Brightness` / `Darkness` | 光照系统 | 需光照系统支持 |
| `FrozenTime` (永远) | -1 = 永不冻结 | 数据层已有，需运行时逻辑 |
| `YoshiTransform` 等变形 | 死亡变形 | 数据层已有，需运行时逻辑 |

### 不计划实现

| 字段 | 原因 |
|------|------|
| `npcblock1` | 与 `npcblocktop` 重复（38A 别名） |
| `Scripts` | Lua `onLoop` 已提供完整脚本能力，无需 NPC 绑定脚本 |
| `HoldGenerator` | Lua 可在 `onLoop` 中用 `xtech_npc_create` 实现更灵活的生成器 |
| `ForceCheck` | SMBX-38A 内部调试标志，无实际游戏效果 |

## 实现方式

### 如何为现有 NPC 添加新属性

在 Episode 或关卡目录下创建 `npc-{ID}.txt`：

```ini
nohammer=1
noshell=1
nolava=1
noleaf=1
noblockhit=1
spinjump=1
spinjumphurt=0
waterjumphurt=0
yoshihurt=0
instantkill=0
immortal=0
groundpoundbreak=0
megabreak=0
turboweak=0
nosectionwrap=0
npcblockside=0
float=0
nopiercingdmg=0
pushable=0
stackable=0
canmeltblock=0
wingsforever=0
frozentime=0
yoshitransform=0
frozentransform=0
jumptransform=0
```

### 修改的文件

| 文件 | 改动 |
|------|------|
| `3rdparty/PGE_File_Formats/npc_filedata.h` | `NPCConfigFile` 新增 24 个字段 |
| `src/npc_traits.h` | `NPCTraits_t` 新增 25 个成员 |
| `src/custom.cpp` | `LoadCustomNPC` 新增 26 个配置应用语句 |
| `src/custom.cpp` | `s_playerFileName` 改为 `std::string[]`（Lua 可写） |

## 相关提交

- `b9fe0649` — NPC 配置扩展：新增 26 个 NPC trait 字段，PGE 解析器同步更新
