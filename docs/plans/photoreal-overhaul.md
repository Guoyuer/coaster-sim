# 照片级第一人称过山车画质重构计划 (Camera-Real On-Rails Overhaul)

> 本文档供**执行用 AI agent** 使用。读者默认没有上下文，请把本文当作唯一交接材料。
> 引擎：Unreal Engine 5.8（路径见 §0）。平台：Windows / DX12 / SM6。

## 文档层级（避免拿旧 spec 走回头路）

- 当前 Yarlung 照片级画质迭代以 `AGENTS.md`、本文、`docs/specs/photoreal-acceptance.md`、`docs/plans/photoreal-progress.md` 为最高优先级。
- `docs/specs/product.md`、`architecture.md`、`physics.md`、`validation.md`、`visual.md` 是基础仿真/M1 资料。它们若提到 free camera、sky dome、procedural placeholder、灰盒美术，只能作为历史/调试语境；照片级验收只看第一人称 on-rails 帧。
- 永久环境美术的归属是 generated level / commandlet / PCG/Foliage / asset pipeline，不是 `ACoasterRideActor`。Ride actor 只负责仿真、轨道/车、相机、HUD 所需状态，最多做临时诊断查询。

## 目标（精确定义，不要跑偏）

1. **Camera-real（像相机拍的，不是像游戏）**：照片级真实度。判据是"截一帧出来像一张真实照片/电影画面"，而不是"比原型好"。
2. **风景如画的雅鲁藏布江 / 林芝**：湿润深绿密林、乳白—青绿湍流江水、冷灰/灰绿湿岩崖壁、云带薄雾、远处南迦巴瓦式雪山、晴天蓝天白云（视觉锚点见 `CONTEXT.md`）。
3. **视角永远是过山车第一人称（on-rails）**：相机始终挂在沿固定轨道样条运动的车上（`CoasterRideActor.cpp` 的 `RideCamera` + `SampleFrame`，轨道控制点在 `YarlungCoasterProfile.h:17`）。**没有自由漫游、没有任意机位。**

> **这三条合起来给出一条比"通用电影级"清晰得多、可行得多的路**，因为约束 3 让我们只需要把"相机沿轨道飞过去时视锥里看到的东西"做到照片级，而不是整个世界从任何角度都成立。这是 on-rails 最大的红利，全程要吃满。

## 三条从目标推出的核心策略（决定一切优先级）

- **S-A. 范围只锁"轨道走廊"**：相机轨迹完全已知（闭环样条）。只有相机飞过时视锥扫到的体积需要照片级；走廊外可以低模/裁掉/不做。预算全砸在走廊内。
- **S-B. 远景/中景 > 近景（与走路模拟器相反）**：第一人称高速运动下，**近景（轨道、脚下植被）运动模糊一闪而过**，真正"如画"且**长时间停留在画面里**的是峡谷崖壁、谷底江水、远山雪峰、云带。预算优先级：**真实地形轮廓 + 大气透视 + 物理天空 + 江面 > 近景细节**。
- **S-C. "相机感"靠成像链，不靠堆几何**：照片感来自**物理曝光 + ACES filmic 色调 + TSR 抗锯齿 + 按车速调的运动模糊 + 克制的镜头脏污/光晕/色散/暗角/景深**——建在**正确的物理光照地基**上。现有 `RideCamera` 后期（`:107-142`）方向对、地基错。

---

## 0. 关键路径与命令（执行前先读）

| 项 | 路径 |
|---|---|
| 引擎编辑器 | `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe` |
| 编辑器 (命令行) | `...\UnrealEditor-Cmd.exe` |
| Build.bat | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat` |
| 工程 | `CoasterSim.uproject`（模块 `CoasterSim`） |
| 默认/启动关卡 | `/Game/Generated/YarlungLandscape/YarlungLandscape_Level`（`Config/DefaultEngine.ini`） |

**构建 C++：**
```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CoasterSimEditor Win64 Development "-Project=$PWD\CoasterSim.uproject" -WaitMutex -NoHotReloadFromIDE
```

**重建资产 + 关卡管线（地形/材质/模型/关卡导入一条龙）：**
```powershell
.\scripts\import-yarlung-landscape.ps1 -Build
```
该脚本顺序：① `generate-yarlung-landscape-assets.py` 生成高度图 `.r16` + macro 贴图 → ② `create-coaster-materials.py` 建材质 → ③ `import-polyhaven-models.py` 导模型 → ④ `-run=YarlungLandscapeImport` commandlet 重建 `.umap`。
- **关卡 (.umap) 是 commandlet 生成物，不是手摆的。** 要往关卡加/删 actor，改 `YarlungLandscapeImportCommandlet.cpp` 再重跑，否则手改会被下次导入覆盖。
- headless UE 脚本即使内部报错也可能返回 exit code 0 → 必须用 success-marker / 资产存在性校验兜底（已有先例，保持）。

**画面验证（截图回归——这就是验收标准，因为它抓的正是真实第一人称画面）：**
```powershell
.\scripts\offscreen-shot.ps1 -Build -Name "hero-stepN" -ResX 2560 -ResY 1440 -WaitSeconds 12
```
- **必须升到高分辨率（≥1440p）**：720p 看不出照片级；若脚本默认值不是 1440p，执行验收时必须显式传 `-ResX 2560 -ResY 1440`。低于 1440p 的图只能算 smoke test，不能算 acceptance。
- 验收每步：跑 offscreen-shot → **用 Read 工具把 PNG 当图片看** → 对照 §"目标"和参考照片。**禁止"脚本没报错=完成"**。
- 脚本日志看 `Saved\Logs\offscreen-shot.log`；导入/材质日志仍看 `Saved\Logs\CoasterSim.log`（搜 `[YARLUNG-MATERIAL]`、`Yarlung scatter instances`）。

---

## 1. 当前架构速览

```
generate-yarlung-landscape-assets.py  ──> YarlungTsangpo_1009.r16 (程序化高度图) + macro_*.tga
create-coaster-materials.py           ──> M_YarlungLandscapeGround / M_CoasterTint / 导入 PBR 贴图
import-polyhaven-models.py            ──> /Game/Generated/Models/*  (PolyHaven 1k/2k 小道具)
YarlungLandscapeImportCommandlet.cpp  ──> 读 .r16 → spawn ALandscape + spawn ACoasterRideActor → 存 .umap
CoasterGameMode::BeginPlay            ──> 运行时若无 ride actor 再 spawn 一个
ACoasterRideActor                     ──> C++ 里生成：轨道/车/支撑 + 天空/云/远山/河/植被/巨石(全部环境)
```

**两套地形并存（结构性 bug）**：真实地形 = commandlet spawn 的 `ALandscape`（高度来自 `.r16`，材质 `M_YarlungLandscapeGround`，`YarlungLandscapeImportCommandlet.cpp:89`）；影子地形 = `CoasterRideActor.cpp:53` 的解析函数 `YarlungLandscapeHeight()`，被用来摆巨石(`:676`)/植被(`:720`)/支撑脚。两份高度场来源不同、不保证一致 → 植被/巨石浮空或穿插。唯一共用的是轨道净空切槽 `ApplyTrackClearanceCut`（`YarlungCoasterProfile.h:70`）。

---

## 2. 核心发现（按严重度，含 file:line）

- **S1（最高，天花板）几何全是引擎基本体 + 顶点色程序化四边形**：轨道/枕木/支撑/车 = `/Engine/BasicShapes/Cube` 拉伸（`CoasterRideActor.cpp:232-243`、`RebuildVisuals():382`）；天空 = 顶点色 `BuildSkyDome():490`；云 = 扁平椭圆 `BuildCloudLayer():541`；远山 = 朝相机平面 `BuildDistantRidges():598`；河/泡沫/激流 = 顶点色 ribbon `BuildRiverRibbon():813`/`:856`/`:899`；用调试材质 `VertexColorMaterial`/`BasicShapeMaterial_Inst`（`:294-300`）。这些正是 `docs/bugs/2026-06-18-visual-pipeline-bugs.md` 判定要删的 low-ceiling 覆盖几何。
- **S2 两套地形高度场冲突**（见 §1）。
- **S3 材质平涂/借贴图**：`M_CoasterTint`（`create-coaster-materials.py:196`）单色平涂；Landscape 材质（`:226`）单张 macro albedo 当 BaseColor、**没接法线**、AO 借自无关贴图——文档明令禁止的"单张照片当整座山"。
- **S4 物理天空与假天穹并存 + 光照非物理**：`SkyAtmosphere`（`:157`）与假天穹/假云冲突；太阳 `SetIntensity(32.0)`（`:152`）量级可疑，又用曝光锁 min=max=1.0（`:121-124`）硬拉 → 能量比非物理，PBR 响应整体假。无体积云。项目级 Lumen/VSM/Nanite 已开（`Config/DefaultEngine.ini`）——地基对，缺上层。
- **S5 植被密度不足/阴影开销错配**：520 树/3400 草等 ISM 解析摆放（`BuildVegetationScatter():691`，数量 `:752-756`），达不到林芝密林；草开 `CastShadow=true`（`:223`）很贵。
- **S6 美术内容生成在 C++ 里**：每改一点画面要重编 C++，迭代慢；git 历史反复缠斗暖色 albedo/雾/moire 即此症状。

---

## 3. 执行硬规则

1. **High-ceiling only**：真实 DEM 地形、分层 Landscape 材质、Nanite 资产、物理天空/体积云、TSR/Lumen。**禁止**临时覆盖几何、顶点色色块、一次性色彩微调来掩盖资产问题（`CONTEXT.md` "Visual Iteration Rule"）。
2. **单一真相源**：地形以导入的 `ALandscape` 为准，删除 actor 内影子地形/假天空/假云/假远山。
3. **造景迁出 C++（硬决策）**：环境美术作者化到关卡/资产（经 commandlet 添加），`ACoasterRideActor` 最终只保留仿真 + 轨道/车几何。不要再往 `RebuildEnvironment` 里加美术。
4. **走廊优先 + 远中景优先**（S-A/S-B）：预算按"相机视锥扫到的体积"和"远中景画意"分配。
5. **每步高分辨率 FP 截图验收 + Read 看图 + 对照参考照片**。证据先于结论。
6. **关卡内容改动走 commandlet**；headless 脚本保留 marker/资产校验。
7. **英雄段纵切优先**（见 §4 阶段 0）：先把一段轨道做到照片级跑通全链，再铺开。横向铺开会摊薄、很久看不到一张真像照片的帧。
8. 资产授权：仅 CC0 / license-compatible（PolyHaven / Fab-Megascans 随 UE 免费 / 真实公开 DEM）。禁止商业模拟器资产。
9. 小步提交，一个 TODO 一个 commit，message 末尾按仓库约定加 `Co-Authored-By`。开工前在 `main` 上新建分支隔离（当前工作区已有大量未提交改动，先确认状态）。

---

## 4. TODO（英雄段纵切优先；阶段 0 跑通全链后再横向铺开）

### 阶段 0 — 选英雄段 + 钉死参照（先做，半天内出第一张对照）
- **0.1 选英雄段**：在轨道样条上选一段**最如画**的弧——理想是一个**临江高弯**，同一帧能看到：脚下/侧下方江水、对岸崖壁、远处雪山+云带。用 `SampleFrame` 在若干 `TrackRatio` 上打印相机位置/朝向，挑出取景最好的 2–3 个 `WaitSeconds` 时间点固定下来（之后 offscreen-shot 只看这几个时间点）。
- **0.2 钉参照图**：收集 3–5 张雅鲁藏布大峡谷/南迦巴瓦/林芝的真实照片，放 `docs/refs/`，作为所有验收的对照基准与"如画"定义。
- **0.3 基线截图**：当前状态在英雄段时间点出一张 1440p FP 截图存档 `Saved\hero-baseline.png`，作为 before。

### 阶段 A — 地形单一真相源 + 真实 DEM（地基）
- **A1 删假环境几何**：从 `CoasterRideActor.cpp`/`.h` 删除 `BuildSkyDome`/`BuildCloudLayer`/`BuildDistantRidges` 及对应组件 (`SkyDomeMesh`/`CloudLayerMesh`/`DistantRidgeMesh`) 和 `VertexColorMaterial` 绑定。河流(`BuildRiver*`)先留占位，D 阶段换真实水体。保留仿真与轨道/车几何。
  - 验收：编译通过；FP 截图不再有方块云/顶点色天穹/平面远山；天空交给 `SkyAtmosphere`（可能暂时偏暗，B 修）。
- **A2 真实 DEM 地形（如画轮廓的来源）— 真实尺度，不压缩**（2026-06-18 决策）：
  - **尺度方案=真实尺度子区域**：裁取约 **5–10 km** 的一段壮观真实峡谷，按**接近 1:1** 导成**公里级 UE Landscape**（UE 跑公里级地形正常）。**禁止**把整条峡谷压进现有 164×187m 的玩具尺度——那会让崖壁只剩 ~43m、远山变成贴轨小丘，第一人称读成桌面沙盘，丢掉"如画"壮阔感（压缩倍率水平~212×/垂直~114×，已验证不可接受）。
  - **垂直保持真实尺度**：崖壁几百米量级，抬头能看到压过来的峡谷墙与真正"远"的雪山。
  - **过山车作为峡谷里的小型设施**：占地约 150m 的轨道是这个公里级峡谷中的一段（符合现实）。**轨道样条坐标需随新世界尺度重新定位/缩放**（`YarlungCoasterProfile.h:17` 的控制点），并让样条穿过取景好的临江高弯。这是 A2 的核心工作。
  - **选定子区域（2026-06-18 用户拍板）=大拐弯最深峡谷段**，南迦巴瓦与加拉白垒之间（世界最深，最大深度 6009m）。险峻+如画。
    - **裁剪 bbox（约 8.3×6.8 km）**：`lat 29.745–29.820°N, lon 94.945–95.015°E`。框内含：谷底最深点 `29.7697°N, 94.9899°E`（Copernicus GLO-30 实采约 **2652m**，接近画面中心）、加拉白垒峰 `29.8133°N, 94.9672°E`（实采约 **7048m**，北缘，做远景雪山背景）、两者之间最陡崖壁。当前 bbox 实采范围约 **2613m→7144m ≈ 4530m**。参考：南迦巴瓦峰 `29.6258°N, 95.0572°E`（7782m）。
    - **DEM 源 = Copernicus GLO-30（30m）优先**（高海拔陡坡质量最好、空洞最少）；ALOS AW3D30 备选；**不要用 SRTM 做主力**（喜马拉雅陡坡有空洞/噪点）。
    - **分辨率维持 1009×1009×16-bit，不用加**（DEM 原生 30m，8km 框原生才~267 采样点，1009 已过采样；近景细节交给阶段 C 材质/法线/Nanite，不靠高度图）。**不需要改** commandlet 的 `HeightmapSize`。
    - **垂直 1:1 真实尺度（关键）**：把 `YarlungLandscapeImportCommandlet.cpp` 的 `EncodedMinZ/EncodedMaxZ` 从 `-360cm/3900cm` 改成框内**真实海拔范围**（当前编码窗口 **2600m–7300m**，注意单位 cm），让崖壁是几百上千米而非 43m。`XYScale` 由 bbox 自动算出（当前 X≈6.70m/quad、Y≈8.27m/quad，可接受）。保留 `ApplyTrackClearanceCut`。
    - **保险步骤**：正式导入前先出一张该 bbox 的 hillshade（晕渲）预览图肉眼确认河道走向与雪峰位置，避免下错瓦片/朝向不对白跑一轮。
  - 远景"画意"主要来自真实山体轮廓+真实尺度——这一步价值最高。
  - 验收：英雄段 FP 帧里对岸崖壁/远山轮廓接近参照照片的真实山形与**尺度感**（壮阔，不是小土坡），不再是平滑解析坡。
- **A3 scatter 改查真实地形，并迁出 ride actor**：用 Landscape 真表面点+法线放置巨石/植被，替换对解析 `YarlungLandscapeHeight()` 的依赖。首选在 `YarlungLandscapeImportCommandlet.cpp` 或专门的 generated scenery/PCG 管线中生成关卡内容；若必须用运行时 `LineTraceSingleByChannel`（高空向下打 `ECC_WorldStatic`），也要封装到独立 scenery actor，不要继续写进 `ACoasterRideActor::RebuildEnvironment`。注意 Landscape 碰撞就绪时序（commandlet 离屏环境与 PIE/运行时都要验证）。最终删除影子地形函数。
  - 验收：植被/巨石与地表无缝贴合，无浮空/半埋；`Yarlung scatter instances` 计数 > 0。

### 阶段 B — 物理天空 + 光照标定 + 大气透视（远景画意主力）
- **B1 单一物理天空**：`SkyAtmosphere` + `UVolumetricCloud`（晴天蓝天白云/云带）+ `SkyLight`(Real-Time Capture)，作者化到关卡（走 commandlet）。删一切假天空。**配置 Aerial Perspective**（大气透视）——这是峡谷远景纵深"如画"的关键。
  - 验收：远景有自然的蓝雾纵深与云带；天际线可信。
- **B2 光照物理量级标定**：方向光设为物理日光量级（lux），`SkyLight` 匹配；**去掉曝光硬锁**改 Manual 合理 EV 或带合理 min/max 的自适应；在正确光照下重调 ACES filmic 曲线（代码已有 `FilmSlope/Toe/Shoulder`，`:127-132`）。把 Lumen GI + VSM 的效果验证出来。
  - 验收：晴天直射的阴影/高光对比可信；地表有 GI 反弹；金属有方向性高光。

### 阶段 C — 分层 Nanite 地表材质（中近景崖壁/坡面）
- **C1 分层地表材质**：改 `create_landscape_material`（`create-coaster-materials.py:226`）：macro 决定远景色/覆盖，叠 Megascans/扫描 PBR 做近景 normal/roughness/AO/breakup，按坡度/高度分层（河岸/森林/岩石/雪/湿度带）。**把法线接回**但做宏观/微观尺度分离避免重现 moire（bug 日志教训）。可选 Nanite 位移(Tessellation)做近景地表起伏。AO 用各图层自己的，别再借 `aerial_grass_rock`。
  - 验收：崖壁/坡面有高频岩石细节与法线起伏，颜色冷灰绿（非橙）；远看无可见平铺。

### 阶段 D — 真实资产替换（江水 > 轨道/车）
- **D1 真实江水（远中景主角，优先）**：UE Water 插件或自定义水体材质——深度、折射、流动法线、Lumen/SSR 反射、湍流泡沫，乳白—青绿色（`CONTEXT.md`）。替换顶点色 ribbon。
  - 验收：江面有反射/折射/流动/岸边泡沫，非卡通青平面。
- **D2 轨道/支撑/车 → 真实网格 + 金属 PBR**：扫掠钢轨剖面 + 枕木/螺栓次级细节（Nanite）；支撑换钢结构件；车体材质分区（漆/金属/橡胶/玻璃）。从 `M_CoasterTint` 平涂换金属 PBR（normal/roughness/metallic/磨损）。
  - 注意：近景高速运动模糊会弱化这部分权重，按 S-B 排在江水之后。
  - 验收：FP 帧里轨道是钢轨非方条，金属有方向性高光。

### 阶段 E — 密林植被 + 成像链精修（把"相机感"最后拉满）
- **E1 植被密度 → PCG/Foliage**：按坡度/高度/河流 mask 用 Megascans/CC0 树/灌木/草散布到林芝密林量级 + LOD；关掉草投影或改接触阴影。优先英雄段视锥内。实现位置仍是 generated level/PCG/Foliage，不是 `ACoasterRideActor`。
  - 验收：英雄段中景成片可信密林，帧率达标。
- **E2 成像链精修（camera-real 收尾）**：在光照/材质稳定后重调 `RideCamera` 后期（`:107-142`）：
  - **TSR** 设为抗锯齿方法（高速运动时序稳定，消闪烁）；
  - **运动模糊按车速**（现 0.025 太低；`visual.md` 要"高速可读且有速度感"）；
  - 景深/暗角/色散/grain/bloom 克制保留，对照参照照片微调曝光与色调。
  - 验收：英雄段 FP 帧高速运动下干净不闪、有速度感、像相机拍的。

### 阶段 F — 沿轨道铺开
- 英雄段达标后，沿整条样条逐段验证（多个 `WaitSeconds`），补齐走廊内其余区段；走廊外保持低模/裁剪。
  - 验收：整圈 FP 飞行任意时间点截图都达照片级，帧率稳定。

---

## 5. 整体验收标准
- 英雄段及整条走廊的**第一人称帧**对照 `docs/refs/` 参照照片达到"像真实照片/电影画面"。
- 性能（`docs/specs/architecture.md`）：桌面 60 FPS / 高画质，为 VR 90 FPS 留头寸；关键阶段单独跑 `stat unit`/CSV，不和截图脚本绑定。
- 每阶段归档 `Saved\hero-<阶段>.png` 对照演进。

## 6. 风险与坑
- **headless exit code 0 假成功** → 保留 marker + 资产校验。
- **.umap 是生成物** → 关卡 actor 改动走 commandlet，别手摆。
- **碰撞时序**：A3 射线放置依赖 Landscape 碰撞就绪；commandlet 离屏环境与运行时时序不同，分别验证。
- **moire 回归**：C1 接法线务必尺度分离。
- **曝光放开后**早期截图可能过曝/欠曝，属预期，按物理量级标定后再判。
- **DEM 对齐**：A2 真实 DEM 的世界尺度/朝向要和现有轨道走廊对齐，否则轨道飞出地形或穿山。
- **TSR vs 截图**：offscreen-shot 使用 UE 渲染输出路径，仍需确认能反映 TSR 后的画面（必要时切换脚本的 `-Mode ImmediateHighResShot` 或经 `-ExecCmds` 出图）。

## 7. 参考文件清单
- 仿真+环境主体：`Source/CoasterSim/CoasterRideActor.cpp` / `.h`
- 轨道剖面/控制点/净空：`Source/CoasterSim/YarlungCoasterProfile.h`
- 关卡导入 commandlet：`Source/CoasterSim/YarlungLandscapeImportCommandlet.cpp`
- GameMode：`Source/CoasterSim/CoasterGameMode.cpp`
- 材质/贴图生成：`scripts/create-coaster-materials.py`
- 高度图/macro 生成：`scripts/generate-yarlung-landscape-assets.py`
- 模型导入：`scripts/import-polyhaven-models.py`
- 管线编排：`scripts/import-yarlung-landscape.ps1`
- 画面验证：`scripts/offscreen-shot.ps1`
- 渲染设置：`Config/DefaultEngine.ini`
- 视觉规范/锚点：`docs/specs/visual.md`、`CONTEXT.md`
- 既往 bug 教训：`docs/bugs/2026-06-18-visual-pipeline-bugs.md`
- 架构/性能预算：`docs/specs/architecture.md`
