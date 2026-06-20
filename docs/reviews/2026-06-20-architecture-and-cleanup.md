# 架构评估 + 代码精简 — 2026-06-20

- Reviewer: independent（双 agent 全代码扫描 + 人工核实行号 + 实施安全清理）
- 范围: 全 `Source/CoasterSim/*`（~3.6k 行）+ `scripts/*`（~4.6k 行）
- 方法: 两个只读 agent 分别审 C++ 运行时架构 与 资产管线/脚本，共报 ~25 条；我**逐条核实并分级**（真问题 / 过度工程 / 误报），只实施安全可验证的那部分。

---

## 一、架构判断（核心回答："有没有架构问题？"）

**只有一个严重架构问题：C++ / Python "split-brain"——同一套世界常量和地形公式在两种语言里各手抄了一份。**

运行时走廊网格在 **C++** 建（`YarlungLandscapeImportCommandlet` + `YarlungTerrain`/`YarlungCorridorProfile`），高度图/宏观贴图在 **Python** 建（`generate-yarlung-landscape-assets.py`）。两者必须空间对齐，否则产生**静默接缝/错色**。已核实逐字重复（非 agent 臆测）：

| 量 | C++ | Python | 状态 |
|---|---|---|---|
| 高度编码范围 | `YarlungTerrainProfile.h:7-8` `260000/730000` | `generate-...py:32-33` `HEIGHT_MIN/MAX` | 逐字相同 |
| 河心锚点 | `:9-10` `95543/-142330` | `:29-30` `RIVER_ANCHOR_X/Y` | 逐字相同 |
| `river_center_y` 公式 | `:28-34` `+9000·sin(x·0.00009+0.25)+4200·sin(x·0.00021-0.6)` | `:72-78` | 一字不差 |
| `smooth01` | `YarlungTerrain::Smooth01` | `:48-50` | 各写一份 |
| 网格分辨率 1009 / 世界 bounds | commandlet + `YarlungTerrain` | 多个脚本 | 重复 |

**风险**：任何一边改公式而另一边不跟 → 网格与贴图错位、河道 mask 与 thalweg 漂移（进度文档里 P4 追过的正是这类 bug）。**这是唯一值得作为"架构债"认真对待的问题。**

**其余被 agent 列为 CRITICAL 的，我判定为过度工程、本阶段不做（附理由）**：
- ❌ "把 `CoasterRideActor` 拆成 5 个组件（Physics/Visuals/Lighting/Camera）"：单场景 on-rails 模拟器，单 actor 持有这些是合理且常见的；拆分是大重构、风险高、收益低，且正在视觉迭代期。
- ❌ "物理与渲染未分离，需引入不可变 snapshot 层"：相机就在车上、确定性 on-rails，`AdvanceRide` 直接写 transform 完全够用；snapshot 层是为多线程/回放设计的，本项目不需要。
- ❌ "`OnConstruction` + `BeginPlay` 双重 Rebuild 是浪费，要 dirty-flag"：这是 UE 标准生命周期（编辑器预览 vs 运行），不是 bug；dirty-flag 反而易破坏编辑器预览。
- ❌ agent 把 `preview-terrain-relief.py` / `diagnose-yarlung-staircase-risk.py` / `dump-yarlung-height-profile.py` 当"死代码"：**误报**，进度文档明确在用，不删。

---

## 二、本轮已实施的安全整理（全部 build + offscreen 验证无回退）

1. **统一 `YarlungTrack.csv` 解析**：新增 `Source/CoasterSim/YarlungTrackCsv.h`（`FYarlungTrackRow` + `YarlungTrackCsv::Load`，列契约单一真相源）。迁移 `CoasterTrackComponent::LoadGeneratedTrack`、commandlet `LoadYarlungTerrainTrackPoints` 复用之；消除两处手抄列索引 + 各异的错误处理。
2. **`CoasterTrackComponent` 内 3× 重复的"重置块"** 收敛成一个 `ResetToEmpty` lambda（可读性，行为不变）。
3. **C++ `Smooth01` 去重**：`YarlungCorridorProfile` 删掉自带定义、`using YarlungTerrain::Smooth01`。（`YarlungTerrainRelief` 早已复用 `YarlungTerrain::Smooth01`，无需动。）

验证：UE build PASS；offscreen `refactor-csv-verify` 已 Read——轨道正常加载、构图与重构前一致、无回退。

---

## 三、推荐的后续重构（有价值但需各自专注 + 验证，未在本轮做）

按性价比排序，建议各自单独一刀：

1. ~~**【最高 · 修架构债】共享常量清单**~~ **✅ 已实施（见下方"split-brain 根治实施记录"）。**
2. ✅ **完成 CSV 整合（已实施）**：`YarlungSceneryActor::ParseTrackRow` 已并入 `YarlungTrackCsv`（split-brain 轮）；本轮新增 `YarlungRiverCsv.h`，`YarlungRiver.csv` 的两处解析（ride actor 雾锚 + river actor 水体）改为共用之，删除 `ParseRiverRow`。
3. ✅ **物理 feel 常量数据化（已实施，部分）**：`AdvanceRide` 的 drag `0.000015`、滚阻 `18.0`、top speed `5600` 提为 `AeroDragCoefficient`/`RollingResistanceCms2`/`MaxSpeedMps` 三个 EditAnywhere 属性（`Ride|Resistance`）。各 section 的响应增益（2.0/1.1/1.4 等）保留为就地常量——它们是控制律的一部分，不是"手感旋钮"，外露反而噪声。
4. ⬜ **拆 commandlet 的 god-function**：`BuildYarlungCorridorTerrainStaticMesh`（~266 行）与 `Main` 按 `LoadHeightmap / BuildMesh / SpawnActors / SaveMap` 切开。**风险较高**（需重烤走廊地形比对），留作单独一刀。
5. ✅ **section 用枚举（已实施）**：`AdvanceRide` 改用 `GetSectionAtDistance` 返回的 `ECoasterSection` + `switch`，删除每帧 `FName` 字符串比较和冗余的 `GetSectionName` wrapper；Telemetry 仅在末尾 `SectionName(enum)` 转一次 FName 供 HUD。
6. ⬜ **材质脚本去样板**：`create-coaster-materials.py` 的 `create_landscape_material`/`create_mesh_terrain_material` 抽 `add_textured_layer()` helper。低风险，待做。

### 第二轮清理实施记录（2026-06-20）
- 新增 `YarlungRiverCsv.h`；ride actor 雾锚 + river actor 水体共用，删 `ParseRiverRow`。
- `AdvanceRide`：string→`ECoasterSection` switch；drag/滚阻/top-speed 提为 EditAnywhere；删 `GetSectionName`。
- 验证：UE build PASS；offscreen `refactor-physics-verify` 已 Read（江水/轨道/构图无回退）；运行时 loader 复现历史值（river 213 samples、fog Z 267655.0cm、track 198 pts / 5057.2m）；物理改动按构造等价（enum case 与旧 string 分支 1:1、常量值不变、`MaxSpeedMps 56×100=5600`）。

---

## split-brain 根治实施记录 — 2026-06-20（1+2+3 全做，已验证）

用户决定全做且"架构需最优"。采用的最优架构 = **数据做契约：单一 JSON 真相源，C++ 与 Python 都消费它；公式层用 parity 测试锁死。**

### 1 — 常量配置化（单一真相源）
- 新增 `Config/yarlung-terrain.json`：grid_size、world bounds、encoded height 范围、river 锚点/Z/mask 半宽/centerline 系数。**唯一真相源。**
- Python：新增 `scripts/yarlung_config.py` 读 JSON；`generate-yarlung-landscape-assets.py` 与 `preview-terrain-relief.py` 改为 import，删除各自硬编码常量 + 重复的 `smooth01`/`river_center_y`。
- C++：`YarlungTerrainProfile.h/.cpp` 改为 `YarlungTerrain::Config()` 懒加载 JSON（离线/构造期使用，无运行时热路径成本）；`Build.cs` 加 `Json` 依赖。

### 2 — parity 测试（公式层安全网）
- C++ automation `CoasterSim.Yarlung.TerrainConfigParity`：断言 `Config()` 常量 + `RiverCenterY`/`HeightValueToCm` 命中 golden，并校验 JSON 存在。
- Python `scripts/test_yarlung_parity.py`：断言 `yarlung_config` 命中**同一套 golden**。
- 两侧 pin 同一 golden ⇒ C++↔Python 必然一致；任一侧公式漂移立刻变红。

### 3 — 架构精简（消除重复源，非仅配置化）
- 删掉 C++ 里 **4 套重复常量**：`YarlungSceneryActor`（grid/bounds/encoded + 自带 `HeightValueToCm`）、commandlet（grid/bounds + 硬编码 `26000` river 宽）、`YarlungTerrainRelief` 默认值 —— 全部改读 `Config()`。
- 完成 CSV 整合：`YarlungSceneryActor` 自带的 `ParseTrackRow` 迁到共享 `YarlungTrackCsv::Load`。
- （承上轮）`YarlungCorridorProfile::Smooth01` 去重、`CoasterTrackComponent`/commandlet 共用 `YarlungTrackCsv`。

### 验证
- UE build PASS（含 Json 依赖 + 新测试）。
- Automation：**10/10 Success，0 Fail**（新增 `TerrainConfigParity` + 既有 TerrainRelief/CorridorProfile/ViewCorridor 全过 ⇒ `Config()` 重构行为不变）。
- Python parity：PASS。
- Offscreen `refactor-config-verify`（已 Read）：运行时 `Config()` JSON 加载正常、scenery 散布/构图与重构前一致、无回退。

### 剩余尾巴（非阻塞）
- `preview-terrain-relief.py` 的 noise 函数仍是 C++ relief 的 numpy 镜像（公式镜像，非常量；它是诊断工具不产出货资产，低风险）。若要彻底，可让预览从 C++ dump 生成。
- 更激进的"删除 `YarlungColorAtPosition` 调色板、让 mesh 采样 Python 烤的 macro_albedo"属视觉影响型改动，应单独一刀并截图验证，未纳入本次（本次目标是消除*静默漂移*，已达成）。

### 第三轮清理实施记录（2026-06-20）— #6 + #4

**#6 材质脚本去样板**：`create-coaster-materials.py` 新增 `connect_detail_texture(material, texture, x, y, coords, prop, label)` helper；live 的 `create_mesh_terrain_material` 的 normal/rough/AO 三段 sample+connect 样板（~44 行）收成 3 行；两个材质函数里 6 处 inline `connect_material_property + raise` 改用既有 helper。验证：py_compile PASS、`-ForceMaterials` 重生成 PASS、offscreen `refactor-material-helper-verify` 已 Read（地形材质渲染无变化）。
- **新发现（已记入 backlog）**：`M_YarlungLandscapeGround`（`create_landscape_material`，198 行）+ 其 macro 贴图依赖 **运行时已死**——corridor-mesh pivot 后 `landscape_count=0`，无 C++ 引用，只剩 build-marker/inspect 脚本指向它。建议单独一刀移除（级联到 macro 贴图生成 + inspect + ps1 marker），本轮只做等价 helper 替换、未删。

**#4 拆 commandlet god-function**：
- `Main`（89 行）→ 抽 `LoadYarlungHeightmap`（顺手把漏网的 `HeightmapSize=1009` 改 `Config().GridSize`）+ `SpawnYarlungWorldActors`（4 actor 放置收成一个 `Spawn` lambda），Main 变成 ~25 行纯编排（load→build→world→spawn→save）。
- `BuildYarlungCorridorTerrainStaticMesh`（266 行）→ 把 6 个 tessellation 常量提到 namespace 作用域，按"关注点"抽两个 phase：`ComputeCorridorPositions`（authored profile + relief 位移 + UV + 统计）、`ComputeCorridorNormalsAndColors`（中心差分法线 + 顶点色），统计收进 `FCorridorMeshStats`。三角发射 + 资产 finalize 留在主函数（finalize 与统计日志强耦合、`return nullptr` 流，抽出反而增噪）。
- **验证（关键，确定性 mesh）**：UE build PASS；重烤地形 commandlet 日志 `vertices=114840`（拓扑数与历史一致）、三角公式自洽（792 rings×144 − 66539 skipped ×2 = 95018）；loop 体逐字搬迁未改算法；offscreen `refactor-commandlet-verify` 与重构前同帧一致；Automation **10/10 Success / 0 Fail**。

### 全会话清债收尾
backlog #1–#6 处置：#1 split-brain ✅根治、#2 CSV 整合 ✅、#3 物理 feel 常量 ✅、#5 section 枚举 ✅、#6 材质去样板 ✅、#4 commandlet 拆分 ✅。唯一剩余：移除运行时已死的 `M_YarlungLandscapeGround` + macro 贴图链（需级联验证，建议单独一轮）。

### 第四轮清理（2026-06-20）— 清除不需要的 fallback

1. **C++ `YarlungTerrain::FConfig` 的硬编码"JSON 缺失回退"默认值**：上轮为防御保留的 260000/730000/95543/centerline 等默认值，其实是把刚消除的 split-brain 常量又抄回了 C++。改为**字段全部零初始化**，值只来自 JSON；缺/坏文件 → 记 Error + 零配置（被 `TerrainConfigParity` 测试与 heightmap 字节数校验当场抓住），不再有"静默用对的常量"的影子源。JSON 成为唯一真相，无 C++ 镜像。
2. **Python 合成程序化地形 fallback**：`generate-yarlung-landscape-assets.py` 的 `--source synthetic` 分支 + `terrain_height()` 函数 + `if source_heights else terrain_height(...)`/`else None` 守卫，是真实 DEM 架构之前的遗留路径——管线只走 `copernicus`（默认、无人传 `--source`），`build_copernicus_height_grid` 永远返回满网格。全部删除并 dedent 主路径；顺带去掉因此变 unused 的 `RIVER_Z` 导入。copernicus 输出可证不变（守卫恒为真）。

验证：UE build PASS；`py_compile` + `--help` argparse PASS；Python parity PASS；C++ Automation 10/10 Success / 0 Fail（`TerrainConfigParity` 仍过 ⇒ 零默认下 `Config()` 仍正确读 JSON，证明回退确实多余）。

保留（非"不需要"的 fallback，刻意不动）：`verify-track-clearance.py` 的 `normalize(a, fallback)` 数值退化保护；actor `LoadObject` 的"缺则跳过"守卫（不是回退到默认材质，旧的默认材质回退此前已删）。
