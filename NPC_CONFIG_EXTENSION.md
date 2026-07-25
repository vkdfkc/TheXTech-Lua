# NPC 配置扩展（SMBX-38A 兼容）

## 完成状态

### ✅ 已完成：PGE 解析器 + NPCTraits 数据层（27 个字段）

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
| `megabreak` | bool | false | 巨大形态可破坏（仅数据层） |
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
| `nonpccollision` | bool | false | 跳过 NPC-NPC 碰撞 |

### ✅ 已完成：运行时逻辑（21 个字段）

| 字段 | 修改文件 | 逻辑 |
|------|---------|------|
| `NoBlockHit` | `src/npc/npc_hit.cpp` | B=2 时提前返回，不受方块顶出伤害 |
| `NoLava` | `src/npc/npc_hit.cpp` | B=6 时提前返回，不受岩浆伤害 |
| `NoLeaf` | `src/npc/npc_hit.cpp` | B=7 时提前返回，不受狸猫尾巴伤害 |
| `NoHammer` | `src/npc/npc_hit.cpp` | B=3,4,5 时检查投掷物是否为锤/斧/投掷物类型 |
| `NoShell` | `src/npc/npc_hit.cpp` | B=3,4,5 时检查投掷物是否为龟壳类型 |
| `NoPiercingDmg` | `src/npc/npc_hit.cpp` | B=3,4,5 时免疫贯穿类投掷物（龟壳/锤） |
| `WingsForever` | `src/npc/npc_hit.cpp` | 受伤时保留翅膀不消除 |
| `TurboWeak` | `src/npc/npc_hit.cpp` | 玩家冲刺时 B=1 升级为 B=3（龟壳伤害） |
| `GroundpoundBreak` | `src/npc/npc_hit.cpp` | B=8（压地震动）直接击杀 NPC |
| `SpinJump` | `src/player/player_npc_logic.cpp` | 控制旋转跳是否可踩踏该 NPC |
| `SpinJumpHurt` | `src/player/player_npc_logic.cpp` | 旋转跳踩 NPC 时反伤玩家 |
| `WaterJumpHurt` | `src/player/player_npc_logic.cpp` | 水中跳跃踩 NPC 时反伤玩家 |
| `YoshiHurt` | `src/player/player_npc_logic.cpp` | 骑耀西踩 NPC 时反伤玩家 |
| `InstantKill` | `src/player/player_npc_logic.cpp` | 玩家接触即死（5 个接触点：JumpHurt、Miniboss、龟壳撞击、通用接触、跳跃反伤） |
| `Pushable` | `src/player/player_npc_logic.cpp` | 扩展 IsAShell 抓取/踢逻辑到 Pushable NPC |
| `Immortal` | `src/npc/npc_kill.cpp` | NPC 被击杀时（B!=9）在出生点重生 |
| `NoSectionWrap` | `src/npc/npc_update/npc_movement_logic.cpp` | 提前返回，忽略场景边界循环 |
| `Stackable` | `src/npc/npc_update/npc_movement_logic.cpp` | 可堆叠 NPC 静止不自行移动 |
| `CanMeltBlock` | `src/npc/npc_update/npc_block_logic.cpp` | 触碰冰块时融化（同火球逻辑） |
| `Float` | `src/npc/npc_update.cpp` | 水中浮于水面，不受减速，同木筏行为 |
| `NpcBlockSide` | `src/npc/npc_update/npc_collide.cpp` | 侧面阻挡其他 NPC（强制转向） |
| `NoNPCCollision` | `src/npc/npc_update/npc_collide.cpp` | 跳过 NPC-NPC 碰撞检测 |
| `NoYoshi` | `src/player.cpp:3322` | 已有，耀西吞食跳过 |
| `NoFireBall` | `src/npc/npc_hit.cpp`（预存在） | 已有 |
| `NoIceBall` | `src/npc/npc_hit.cpp`（预存在） | 已有 |

### ⬜ 未完成：仅数据层，缺少运行时逻辑

#### 变形系统（需新子系统）

| 字段 | 说明 |
|------|------|
| `YoshiTransform` | 被耀西吞下后转换为指定 NPC ID |
| `FrozenTransform` | 被冻结后转换为指定 NPC ID |
| `JumpTransform` | 被踩踏后转换为指定 NPC ID |
| `FrozenTime` | 冻结持续帧数（含 -1 永不冻结逻辑） |

#### MegaBreak（仅数据层已就绪）

| 字段 | 说明 |
|------|------|
| `MegaBreak` | 巨大形态可破坏，运行时逻辑未实现 |

### ⬜ 未做：38A 版本中的以下字段

这些字段在 SMBX-38A 中存在但尚未加入：

| 字段 | 说明 | 原因 |
|------|------|------|
| `SmallHitbox` | 小碰撞体 | 需碰撞系统改动 |
| `MiniEnemy` | 迷你怪物 | 需新游戏机制 |
| `CanPressPSwitch` | 按 P 开关 | 需新机制 |
| `HoldGenerator` | NPC 生成器 | 需新子系统 |
| `Z-Position` | Z 层级 | 需渲染后端改动 |
| `DirectiveGFX` | 随方向旋转图像 | 需渲染后端改动 |
| `BlendMode` | 渲染模式 | 需渲染后端改动 |
| `ForceCheck` | 强制检测 | 语义不明 |
| 伤害值字段 | FireBallDmg, JumpDmg, ShellDmg 等 | 需伤害系统重构 |
| `Brightness` / `Darkness` | 光照系统 | 需光照系统支持 |
| `Scripts` | 执行脚本 | Lua 已有钩子可替代 |

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
nonpccollision=0
```

### 修改的文件

| 文件 | 改动 |
|------|------|
| `3rdparty/PGE_File_Formats/npc_filedata.h` | `NPCConfigFile` 新增 25 个字段 |
| `src/npc_traits.h` | `NPCTraits_t` 新增 26 个成员 |
| `src/custom.cpp` | `LoadCustomNPC` 新增 27 个配置应用语句 |
| `src/custom.cpp` | `s_playerFileName` 改为 `std::string[]`（Lua 可写） |
| `src/npc/npc_hit.cpp` | 9 个 trait 运行时检查 |
| `src/player/player_npc_logic.cpp` | 6 个 trait 运行时检查 |
| `src/npc/npc_kill.cpp` | Immortal 重生逻辑 |
| `src/npc/npc_update/npc_movement_logic.cpp` | NoSectionWrap + Stackable |
| `src/npc/npc_update/npc_block_logic.cpp` | CanMeltBlock |
| `src/npc/npc_update/npc_collide.cpp` | NoNPCCollision + NpcBlockSide |
| `src/npc/npc_update.cpp` | Float |

## 相关提交

- `b9fe0649` — NPC 配置扩展：新增 26 个 NPC trait 字段，PGE 解析器同步更新
- `6b53ff74` — 文档：NPC 配置扩展完成状态记录
- （待提交）— NPC 配置扩展：21 个字段的运行时逻辑实现
