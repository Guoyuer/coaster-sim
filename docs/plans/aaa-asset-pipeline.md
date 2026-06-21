# AAA 资产管线计划（Megascans-first）

> 目标：把雅鲁藏布/林芝第一人称过山车做到 **AAA 照片级**，对齐 `docs/refs/local/02_fp_yarlung_coaster_canyon.png`、`03_fp_mountain_coaster_valley.png`、`01_yarlung_valley_river_blossom.jpeg`。
> 决策（2026-06-21）：走 **Megascans 优先**。引擎已 AAA-ready，瓶颈是内容资产。最佳资产（Megascans）免费但需在编辑器登录 Fab/Quixel Bridge——这一步必须由人来做，agent 无法无头登录。

## 现状

- **引擎基线已就绪**（`Config/DefaultEngine.ini`）：Lumen GI + Lumen 反射、Virtual Shadow Maps、Nanite、Mesh Distance Fields、Virtual Textures、DX12/SM6。渲染技术不是瓶颈。
- **程序化基线**（已停止继续投入）：旧 canyon-wall 程序化幕布和旧 procedural river actor 已从默认链路删除；它们会制造假山片/黑洞/河槽遮挡/浅色平片水面，结构上到不了 AAA。当前默认关卡保留 DEM corridor terrain + Megascans scatter + UE Water；项目不再依赖 `ProceduralMeshComponent`。
- **现有 CC0 资产**：PolyHaven 1k `rock_face_01/02`、`boulder_01`、`shrub_03/04`（codex 评为"低模幕布"，不达 AAA），`aerial_grass_rock` 2k 地表贴图。

## 你要做的一步：用 Quixel Bridge 导入 Megascans（登录 Fab）

在 UE 编辑器里打开 Quixel Bridge 插件，登录后按下表添加资产。导入设置统一：**Nanite 开**、最高质量 LOD0、目标路径默认 `/Game/Megascans/...` 即可（我会自动发现路径）。林芝大峡谷是亚热带→高山过渡带：陡峭密林 + 灰色岩壁 + 翡翠江水 + 远处雪峰。

| 类别 | 搜索词 / 选择 | 数量 | 用途 |
|---|---|---|---|
| **岩壁 Cliff（主角，Nanite）** | "cliff", "rock wall", "canyon wall", "granite cliff", "alpine cliff" | 4–6 个大型 | 作为唯一近中景岩壁层，沿峡谷两壁 kitbash/散布 |
| **巨石 Boulder（Nanite）** | "granite boulder", "mossy rock", "forest rock", "mountain rock" | 6–10 个多尺寸 | 山腰/前景碎石打散，轨道附近近景细节 |
| **针叶/阔叶树（Foliage，带 LOD）** | Megascans 3D Plants 的 spruce/pine/fir；或 Fab 上 "conifer forest pack"/"Himalayan forest" | 3–5 种乔木 + 2–3 种灌木 | 森林树冠 massing（替换当前 branch-clump 近似）|
| **下层植被** | "fern", "grass", "understory", "bush" | 3–4 种 | 地面层次、近景绿量 |
| **地表 Surface** | "forest floor", "alpine grass", "mossy ground", "rocky ground", "scree" | 4–5 张 | 多层混合地形材质（替换单张平铺贴图）|
| **（可选）远山雪峰** | Fab "mountain backdrop" / "snow peak"，或保留地形 mesh + 大气 | 1 套 | 远景雪山层次（refs 地平线雪峰）|
| **（可选）过山车** | Fab "roller coaster track"/"steel coaster"，或后续自做管轨 mesh | 1 套 | 前景红车 + 钢管轨（替换 BasicShapes cube）|

导入后在这里打钩或留言资产实际路径，我据此接线。

## 我来做的接线（资产落地后，全自动）

1. **岩壁系统**：用 Nanite cliff mesh 沿轨道走廊两壁散布/拼接；不恢复旧 `YarlungCanyonWallActor` 程序化幕布。
2. **森林系统**：`YarlungSceneryActor` 重新启用散布（codex 因旧低模禁用，新 Megascans 树可放开），乔木 instanced foliage 高密度树冠 + 下层植被；按高程/坡度/到江距离分层。
3. **多层地形材质**：用 Megascans 地表做 height/slope 混合（rock/scree/grass/forest-floor），替换当前单张 `M_YarlungMeshTerrain`。
4. **江水**：在 UE Water `WaterBodyRiver` 上升级真实河流材质（翡翠色 + 流动 + 岸边泡沫），不回到旧 procedural river actor。
5. **结构去灰盒**：过山车车体 + 管轨 mesh 替换 `BasicShapes::Cube`。
6. **成像收尾**：复核 Lumen/TSR/曝光/雾，保住已得的峡谷纵深与 aerial perspective。

## 验收（AAA 照片级）

- 多个第一人称时点（t30/t90/t150…）都读成真实林芝峡谷：有破碎岩壁、密林树冠颗粒、翡翠江水纵深、远山层次。
- 不再有：平涂幕布、灯芯绒条纹、灰盒结构、单张拉伸贴图。
- RiskGate 进入 OK；并通过肉眼对照 L1–L3。性能：on-rails 帧可接受（Nanite + foliage LOD + VSM）。
