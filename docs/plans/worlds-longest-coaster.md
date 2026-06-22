# 重定位设计：世界最长的实景过山车（World's-Longest Scenic Coaster）

> 状态：设计 + §9 开放项已全部用户拍板（2026-06-19）；2026-06-21 用户更新轨道拓扑自由度，spec 改为风景优先的开放式 scenic layout。
> 关联：定位上游于 `photoreal-overhaul.md`（视觉）/`photoreal-acceptance.md`（验收）/`AGENTS.md`（操作）。
> 本文是 **codex-ready 实现 spec**：定义目标、原则、架构、三大子系统接口/算法/参数、实现顺序与验收增量。具体控制点坐标**由生成器从 DEM 算出，不在本文硬编码**。

---

## 1. 定位与指导原则

**一句话**：把现在「峡谷里的 ~600m 灰盒小环」重定位为 **世界最长（~5,000m）的第一人称实景过山车**——在真实尺度的雅鲁藏布大拐弯峡谷中布设一条**风景优先的开放式 scenic layout**，可沿江、跨江、高架、俯冲、overbank 或短暂翻滚，只要通过净空/G 值门禁并服务第一人称照片级构图。

**硬指标 / 治理优先级（冲突时按此裁决）**：
1. **安全/舒适 G 限**（硬约束，永不让步）——见 §4 舒适包络。
2. **长度**：`TrackLengthCm` ≥ 250,000cm（2,500m，必破纪录 Steel Dragon 2000 = 2,479m）；**目标 ~500,000cm（5,000m）**。这是身份指标，硬底。
3. **风景优先**：当风景与乘坐刺激冲突时，**风景赢**（跨江俯视、贴崖高弯、对岸崖壁、雪峰构图优先于多压一个 airtime 丘）。
4. **乘坐体验**：在 1–3 约束内，做成一条真正好玩、节奏对、有俯冲/airtime 的过山车。

**拓扑（2026-06-21 更新：开放式 scenic layout）**：轨道不再被限定为沿江往返。雅鲁藏布江/河谷底线是视觉锚点和空间参照，不是轨道必须贴着走的几何约束。当前范围允许跨江、斜穿峡谷、高架俯视、贴崖 overbank、俯冲/airtime、短暂 roll/inversion，以及类似 8 字的构图路线；唯一硬门是净空、舒适 G、闭环长度与第一人称空间合同。轨道本质上是 camera choreography tool：为了让画面同时看到江水、湿岩、密林、远山，可以主动离开河线、抬高或横跨河谷。

**不变量**：第一人称 on-rails、单一真实 DEM、不穿山、照片级目标不变；本重定位**替换轨道/走廊范围**，并相应扩大 photoreal 走廊预算。

---

## 2. 现状基线（实现者必读：已有什么，别重造）

- **物理已存在**：`CoasterRideActor.cpp::AdvanceRide` 已实现能量法物理——重力沿前向投影、空气阻力 `0.000015·v²`、滚阻 `18 cm/s²`、Lift/Launch/Brake/Station 分段驱动、速度积分 `Clamp(180..5600 cm/s)`、距离推进；并算出 `Telemetry.{SpeedMps,HeightMeters,TrackDistanceMeters,VerticalG,LateralG,LongitudinalG,SectionName}`（座椅力 = 世界加速度 − 重力，投影到 Up/Right/Forward）。**保留并扩展，勿重写。**
- **轨道现状**：`UCoasterTrackComponent::LoadGeneratedTrack()` 从 `Content/Generated/YarlungLandscape/YarlungTrack.csv` 加载约 5km 闭环轨道；`ACoasterRideActor` 不再持有编辑器控制点，也不再自动回退旧 8 点短环。
- **倾斜现状**：生成器按曲率/设计速度写入 `roll_deg`，运行时 `SampleFrame` 按距离插值 CSV banking 并应用到座椅坐标。
- **分段现状**：`YarlungTrack.csv` 每点写 `section`，`UCoasterTrackComponent` 构建距离区间；运行时按距离查段，不再按 `TrackRatio` 写死。
- **净空现状**：全环不穿山保证由 `scripts/verify-track-clearance.py` 的生成即校验门禁承担；旧 no-op clearance-cut 代码已删除，不再靠运行时/导入侧硬挖山兜底。
- **坐标系/编码**（生成器与 commandlet 一致）：
  - 世界网格 `SIZE=1009`，`X∈[-337778.43, 337778.43]cm`（≈6.76km），`Y∈[-416981.55, 416981.55]cm`（≈8.34km），原点居中。
  - 历史 Landscape 导入曾使用原点 `Scale=(XYScaleX≈670.2, XYScaleY≈827.3, ZScale≈917.97)`、`SectionSizeQuads=63`；当前运行时可见地形已切到 generated corridor static mesh，但高度编码和世界 bounds 仍沿用同一数据契约。
  - 高度编码：`.r16` 16-bit，`HeightCm = Lerp(260000, 730000, raw16/65535)`（即海拔 2600–7300m）。
  - 世界→经纬：`world_to_lon_lat(x,y)`（`generate-yarlung-corridor-source-assets.py`）。
  - DEM bbox：lat 29.745–29.820N × lon 94.945–95.015E（大拐弯最深段）。
  - ⚠️ `river_center_y(x)` 是**合成 fallback 的解析河**，copernicus 真 DEM 路径**没有提取真实河中心线**——河 mask 也是按到解析中心线的横距近似。**真往返线路必须从真 `.r16` 提取真河谷底线（thalweg），不能直接用 `river_center_y`。**
- **诊断基线**：`scripts/verify-track-clearance.py` 是当前净空/G 值硬门；旧一次性 height-profile 诊断脚本已退役。

---

## 3. 架构总览：三大子系统 + 数据契约

```
[离线] 轨道生成器 (Python)                     [离线] 净空+舒适校验器 (Python)
 DEM/river anchors → scenic 控制点/候选路线 → 重采样
                                                 密采样轨道 vs .r16 → min净空/坡度/曲率/估算G
        │  emit                                        ▲ gate（不过则 build fail）
        ▼                                              │
   Content/Generated/YarlungLandscape/YarlungTrack.csv ─┘   ← 单一真相源（生成+提交）
        │  load at runtime
        ▼
[运行时] CoasterRideActor (C++)
   LoadGeneratedTrack() → RebuildSpline（变长、闭环）
   GetSectionName(dist) 读数据段 → AdvanceRide（已有物理）
   SampleFrame：曲率驱动 banking（替换正弦）→ Telemetry G
```

**数据契约 `YarlungTrack.csv`**（生成器写、C++ 读；UTF-8、逗号分隔、首行表头）：

| 列 | 单位 | 含义 |
|---|---|---|
| `idx` | int | 控制点序号（沿行进方向 0..N-1，闭环） |
| `x,y,z` | cm | 世界坐标控制点 |
| `roll_deg` | deg | 设计倾斜角（曲率驱动，生成器算好；运行时可用作基准或重算） |
| `section` | enum | `Station/Lift/Outbound/Turnaround/Return/Launch/Brake`（分段标签） |
| `terrain_z` | cm | 该点正下方地形高度（来自 `.r16`，供运行时/校验冗余核对） |

同时生成器更新 `manifest.json` 增加 `track` 块：`{length_m, control_point_count, out_back_split_m, min_clearance_m, min_curve_radius_m, max_grade_pct, est_max_vert_g, est_max_lat_g, section_distances_m:{...}}`。

> 决策 D1：**长轨道不再硬编码进 `.h`**。~5km 在合理控制点间距下约 80–160 点，且必须可从 DEM 再生。改为生成+提交的 CSV，符合现有「generated assets」模式（`.r16`/preview/masks/river CSV/manifest/track CSV 是生成提交的；旧 macro TGA 链已删除）。Yarlung 运行时路径必须加载 `YarlungTrack.csv`；旧 `DefaultTrackControlPoints()` / `YarlungCoasterProfile.h` 已删除，不能作为 fallback 重新引入。

---

## 4. 子系统 A：轨道生成器（离线 Python）

**位置**：扩展 `generate-yarlung-corridor-source-assets.py`，或新增 `scripts/generate-yarlung-track.py`（读已生成的 `YarlungTsangpo_1009.r16` + manifest）。推荐后者（关注点分离）。

**输入**：naturalized `.r16`（高度场）、世界坐标系常量、可调参数（见下）。
**输出**：`Content/Generated/YarlungLandscape/YarlungTrack.csv` + manifest `track` 块。

### A1. 河谷底线（thalweg）与 scenic anchors 提取
1. 把 `.r16` 解码为 `height_cm[y][x]`（`Lerp(260000,730000,raw/65535)`）。
2. 找全局/区域最低点作为种子（谷底）。
3. 沿河谷主轴（本段大致沿 X，但有大拐弯须跟真实走向）做**最小代价谷底追踪**：在一个横向搜索带内，逐步推进时选「局部横剖面最低 + 与上一步方向连续」的点（贪心 least-cost / 或逐列 min-in-corridor），得到有序谷底折线。
4. 重采样为等弧长点、宽核平滑（去抖动但保大走向），得到 `centerline[]`（世界 cm）。
5. **验证**：把 centerline 叠加到 `YarlungTsangpo_hillshade.png` 输出预览 PNG，人眼确认贴合真实河道（仿 hillshade 验收先例）。
6. 从 DEM/river 派生 scenic anchors：高处俯视点、可跨江点、对岸湿岩入镜点、远山方向、森林坡带、站台候选 bench。这些 anchor 用来驱动开放式 route solver，而不是把轨道锁死在 thalweg offset 上。

### A2. 站台与开放式 scenic route 结构
- **站台/起点**：选稳定 bench 或坡肩，优先给出站后第一帧可读的峡谷尺度。Station 段短、低速。
- **控制点骨架**：route solver 至少考虑这些候选段，而不是固定 outbound/return offset：
  - 高处俯视段：轨道可在峡谷坡肩或高架上运行，让谷底江水进入第一人称视锥。
  - 跨江段：允许斜跨河谷或跨江桥式段，制造同帧江水 + 对岸崖壁 + 森林坡。
  - 贴崖/overbank 段：沿湿岩或森林边缘做大半径倾斜弯，优先保证横向 G 经 banking 后可控。
  - 俯冲/airtime 段：用真实高度差制造速度与节奏，但坡度、竖曲线和 G 值必须由校验器硬门。
  - 回站补能段：用 Lift/Launch/Brake 闭合能量，不要求几何上沿原路返回。
- **闭合**：所有候选路线必须平滑接回 Station 起点（闭环），并保持 `TrackLengthCm` 身份指标。
- **重要反例**：不要把旧 thalweg-offset 生成器简单加正弦/S 曲线当成开放式路线。2026-06-21 快速实验已证明这种做法会产生过小曲率半径、极端坡度/G 值，并且仍不能稳定让水进入视锥。下一版应是 control-point/scenic scoring/优化式生成器。

### A3. 分段标注
生成器按弧长给每段贴 `section` 标签并记录 `section_distances_m`。推荐语义为 `Station/Lift/Scenic/Crossing/Drop/Overbank/Launch/Brake` 等风景与动力学标签；旧 `Outbound/Turnaround/Return` 只作为兼容标签，不再表达主拓扑。运行时 `GetSectionName` 改为**按距离查表**，不再按 ratio 写死。

### A4. 舒适与几何约束（生成时强制裁剪）
- **最大坡度** `max_grade_pct`（如 ≤ ~60% 给 Lift，自由段更缓）。
- **最小曲率半径** `R_min`：由设计速度与横向 G 限反推 `R ≥ v²/(g·lat_g_max)`；平面与竖直弯都受限。
- **airtime 丘**：丘顶设计垂直 G 目标 ∈ [-1.2, +0.2]（**ejector airtime**，强烈失重/抬离感——「更刺激」定档）。
- 任何裁剪后**重新平滑**，避免 C1 折点（沿用 B-spline 思路）。

### A5. 可调参数（manifest 记录，便于复跑）
`target_length_m=5000`、`route_style=scenic_freeform`、`station_anchor`、`scenic_control_points`、`crossing_candidates`、`air_clearance_m`、`bridge_clearance_m`、`overbank_limit_deg`、`roll_accent_segments`、`max_grade_pct`、`R_min_m`、`design_speed_mps`、`vert_g_limits`、`lat_g_max`、控制点重采样间距。旧 `out_back_split/outbound_offset/return_offset` 只能作为兼容参数或 baseline，不再是主约束。

---

## 5. 子系统 B：净空 + 舒适校验器（离线 Python，硬门禁）

**位置**：`scripts/verify-track-clearance.py`（独立，可单跑、也由生成器末尾调用）。
**输入**：`YarlungTrack.csv` + `.r16`。
**做什么**：
1. 沿控制点重建与 C++ 同构的样条（Catmull-Rom/Curve 近似），**密采样**（如每 1–2m 一点）。
2. 每采样点：取样条 Z 与正下方地形 Z（双线性采 `.r16`），算 `clearance = splineZ − terrainZ`。
3. 估算几何 G：沿轨道积分真实速度剖面，结合 `roll_deg` 后的座椅坐标估 `vert_g/lat_g/long_g`；`long_g` 由 drive/drag/rolling/brake 座椅前后力计算，不能用坡度假公式。
4. **输出**：`Saved/Diagnostics/track-clearance-*.csv/.png`（净空沿弧长曲线、违规段高亮），并打印 `min_clearance_m / 违规段数 / min_radius / max_grade / est_max_g`。
5. **门禁**：任一采样 `clearance < clearance_floor`（如 < 2m）或 G 超舒适包络 → **非零退出**，build/验收**不放行**。

> 决策 D6：这把**关掉 clearance cut 后失去的全环不穿山保证**用「生成即校验 + 硬门禁」补回来，直接闭环未结外审。当前代码不保留 `ApplyTrackClearanceCut` fallback；**主保证靠校验器**，不靠挖山。

**舒适包络（硬约束，校验器与运行时遥测共用阈值）—— 用户拍板「更刺激」定档**：
- 垂直 G：`[-1.5, +5.5]`（强 ejector airtime ↔ 强正向压）
- 横向 G：`|lat_g| ≤ 2.5`（用户 2026-06-19 调整；允许更强横向冲击，但 P3 起必须用真实速度剖面 + banking 后 G 做硬门）
- 纵向 G：`|long_g| ≤ 2.5`
（「刺激但仍真人可乘」：真实过山车持续上限 ~5–6G、ejector airtime ~-1~-1.5G，本档落在其内。若实测过猛/过软，在 P1 校验器参数里收放。）

**P1 draft 例外（2026-06-19 用户拍板「急一点也 ok」）**：P1 首版离线线路曾允许用更宽的横向 G 估算门槛（`3.8`）先放行 scenic 5km centerline + 净空证据；P3 起改为用户更新后的最终门槛 `|lat_g| ≤ 2.5`，并必须使用真实速度剖面 + banking 后 G。

---

## 6. 子系统 C：运行时改造（C++）

### 6.0 P0：行为保持式重构（C++ 前置，用户拍板 2026-06-19）

现 `CoasterRideActor.cpp`（802 行 god actor）把相机/后期、轨道几何、物理、环境建模混在一起；重定位的 C++ 改动正落在最乱的核心。**在写任何 5km 新功能前，先做一次零行为变更的针对性重构**，切出干净的轨道/乘坐核心；现短环重构后骑乘**一模一样**，用 offscreen smoke 验证零回归。范围**只限**轨道/乘坐/banking/分段这条缝；**环境建模迁出不并入 P0**，留到 P4（已另有 2026-06-18 迁出决策）。

目标模块边界：
- **`UCoasterTrackComponent`（或 `FYarlungTrack`）= 轨道唯一真相**：从 CSV 载控制点 + `USplineComponent`；`SampleFrame(distance)→frame`（纯几何）、`GetLength()`、`GetSection(distance)`、`GetGeneratedBankRadiansAtDistance()`、`GetGeneratedTerrainZAtDistance()`。**消灭双表示**——运行时只用 generated CSV/spline；旧 `DistanceToTrack2D` / `ApplyTrackClearanceCut` / `YarlungCoasterProfile.h` 已退役。
- **`ECoasterSection` 枚举 + `TArray<FCoasterSectionRange>`（距离区间）** 替换字符串分派（`AdvanceRide` 的 `== TEXT("Lift")` 等全删）。
- **banking 抽成纯函数** `CoasterBanking::AngleFromCurvature(R, v, limits)`，与帧采样解耦；P0 阶段先用它复刻「等效现有观感」的占位实现，曲率真值在 P3 接。
- **`AdvanceRide` 保留**（已较自洽），改为消费 track component 的 `GetSection`/几何，不再自持轨道。
- **`ACoasterRideActor` 瘦成编排者**：持 track 组件 + 相机 + 车，tick 物理、设变换。相机/后期构造块可抽 helper（低优先，非必须）。

出口：编译通过；P0 阶段曾用旧短环 offscreen smoke 做零回归；当前运行时已切到 generated CSV 长轨道，`CoasterRideActor.cpp` 职责继续收窄。

### 6.1 长轨道与数据驱动（P2 起，建在 P0 干净边界上）

1. **加载长轨道**：`UCoasterTrackComponent::LoadGeneratedTrack()` 读 `YarlungTrack.csv` 填控制点（+ 每点 `roll_deg/section/terrain_z`）。CSV 缺失或解析失败必须报错并停止装载，不能回退旧 8 点短环。`RebuildSpline` 逻辑不变（已支持任意点数、闭环）。
2. **分段按距离查表**：`GetSectionName(float TrackRatio)` 改为 `GetSectionName(float DistanceCm)`，按生成器写入的 `section_distances` 命中标签。`AdvanceRide` 调用处同步。
3. **曲率驱动 banking**（替换 `SampleFrame:775` 正弦）：
   - 由样条在该距离的曲率 `κ`（或半径 `R=1/κ`）与当前速度 `v` 求**抵消横向 G 的倾斜** `θ = atan(v² / (g·R))`，限幅 `≤ bank_max`（如 70°），并沿弧长**限速率**（避免 banking 抖动）。
   - 也可用 CSV 的 `roll_deg` 作基准、运行时按真实 `v` 微调。
   - 目标：使 `Telemetry.LateralG` 在设计速度下接近 0；偏差作为舒适验证量。
4. **物理调参**：为开放式长线重设 `LiftTargetSpeedMps/LaunchTargetSpeedMps/BrakeTargetSpeedMps` 与速度 Clamp 上限，使俯冲/airtime/高架/跨江段有节奏变化，并靠 Lift/Launch/Brake 闭合能量；保留能量法不变。
5. **遥测门禁（可选自检）**：运行时若 `|LateralG|/VerticalG` 越包络，记日志（便于回归发现生成器漏网）。

---

## 7. 对 photoreal 计划与验收的影响

- **走廊扩张**：照片级「走廊」从 ~150m 扩到 **~5km scenic camera corridor**，覆盖跨江、高处俯视、贴崖、俯冲与回站补能段的第一人称视锥。`photoreal-overhaul.md` S-A「预算只锁走廊」不变，但走廊体积大增——`photoreal-progress.md` 阶段 A–F 的范围需按新走廊重述（尤其 A2 地形/轨道、A3 scatter、F 沿轨铺开）。
- **英雄段**：不再是单一 12s 截图。沿 5km 选**若干**代表性第一人称帧（跨江、俯冲、高处俯视、贴崖 overbank、远山开阔段）固定为多个验收时间点。
- **A2 验收口径**：在现「中远景轮廓/不穿山/贴地」之上，**新增**：① `TrackLengthCm` 达标（≥2,500m，目标~5,000m）；② 全环净空校验器 PASS（闭环外审硬门槛）；③ 全环 G 在舒适包络内。
- **`photoreal-acceptance.md` 增量**（§2 量规 + §3 出口）：
  - 新维度 **D9 乘坐体验**：节奏/airtime/速度变化/G 舒适（量规 1–5）。
  - 新维度 **D10 线路尺度**：长度达世界最长且线路如画（量规 1–5；锚点：是否 >2,479m、是否贯穿如画走廊）。
  - 阶段 A 出口追加「长度 + 净空校验 + G 包络」三条硬门。
- **决策同步**：本重定位为**流程级、用户拍板**——需在 `photoreal-progress.md` 决策记录追加一条，并把「实现子系统 A/B/C」拆进 NEEDS-HUMAN/待办。

---

## 8. 实现顺序（供 writing-plans 拆步）

- **P0（C++ 行为保持式重构，C++ 前置）**：§6.0 切出 `UCoasterTrackComponent` + `ECoasterSection` + banking 纯函数，瘦身 ride actor。**零行为变更**。出口：编译过、现短环 offscreen smoke 与重构前逐帧一致、各模块单一职责。**与 P1 可并行**（P0 改 C++、P1 写 Python，互不依赖）。
- **P1（基础，离线，无 UE）**：子系统 A 轨道生成器 + 数据契约 CSV/manifest + 子系统 B 校验器。出口：生成一条贴真 DEM、过净空+舒适门禁、长度~5km 的 `YarlungTrack.csv`，并通过 spatial contract 证明至少一个英雄帧稳定看见江水 + 对岸湿岩 + 森林坡/远山。centerline 叠 hillshade 仍用于确认河道 anchor，但不要求轨道贴江。
- **P2（运行时接入）**：C++ `LoadGeneratedTrack` + 距离分段 + RebuildSpline。**依赖 P0（干净边界）+ P1（CSV）**。出口：PIE/offscreen 跑通变长轨道，`TrackLengthCm` 达标，车能跑完闭环不穿山（用 P1 校验器复核）。
- **P3（乘坐打磨）**：曲率 banking 接真值 + 物理调参 + 运行时 G 自检。出口：全环 `Telemetry` G 在包络内，俯冲、跨江、高架、overbank、airtime 与补能节奏成立。
- **P4（走廊照片化）**：按新 5km 走廊重述 photoreal A–F，选多帧英雄段；**含环境建模（river/foam/rapids/boulders）从 ride actor 迁出到 scenery/PCG**（兑现 2026-06-18 决策，P0 故意未并入）。
- **P5（验收收口）**：`photoreal-acceptance.md` 增 D9/D10 + 阶段 A 硬门；多帧打分。

依赖：`(P0 ∥ P1) → P2 → P3`；P4 依赖 P2；P5 最后。**writing-plans 先只规划 P0 与 P1**（其余的基础），不一次摊开全部。

---

## 9. 开放项裁定（2026-06-19 用户已拍板）

1. ✅ **舒适包络 = 更刺激**：见 §5 更新值（垂直 `[-1.5,+5.5]`、横向 `≤2.5`、纵向 `≤2.5`，airtime 丘 ejector `[-1.2,+0.2]`）。
2. ✅ **双轨/跨江轨道入镜 = 接受**：偶尔瞄到自己另一段轨道、跨江桥段或高架回线可接受；路线间距/高度差按风景和 G 值选择，但**不为「不可见」设硬约束**，P4 不卡这条。
3. ✅ **thalweg = 自动化**：生成器贪心追踪谷底 + 自动叠 hillshade 校验（A1.5），**不预设手工锚点**；仅当自动结果明显跑偏时再回退人工干预。
4. ✅ **数据载体 = CSV**（简单、可 diff）；若后续运行时加载有性能/打包顾虑再改生成 header。
5. ✅ **跨江/8 字/overbank 放开（2026-06-21 更新）**：用户明确轨道不必沿河，横跨河、翻滚、大起伏都可纳入当前方案。实现仍必须通过净空/G 值/第一人称空间合同，不用旧 thalweg-offset hack 硬弯。
6. ✅ **spec 落点 = `docs/plans/`** + 在 `AGENTS.md` §1 表与 `photoreal-overhaul.md` 顶部加引用（让 Codex 迭代循环可发现）。
