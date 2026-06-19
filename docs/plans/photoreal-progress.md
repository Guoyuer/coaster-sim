# 照片级迭代进度（活文档 — 每轮迭代必须更新）

> 这是断点续跑的状态文件。任何 agent 开工前先读这里确定"现在在哪、下一步做什么"，收工前写回。
> 流程：`photoreal-overhaul.md` · 验收：`photoreal-acceptance.md` · 操作：`AGENTS.md`

## 当前状态
- **当前阶段**：阶段 A（地形单一真相源 + 真实 DEM）— A1 Done，A2 后台资产生成完成，UE 导入/视觉验收待执行
- **基线**：当前为原型/灰盒（顶点色假天空/假河、Cube 轨道、程序化解析地形、单张 macro 平涂地表）。基线截图：`Saved\hero-baseline.png`（2560×1440，`WaitSeconds 12`）
- **英雄段时间点**：`WaitSeconds 12`。理由：六张候选图（3/6/9/12/15/18s）中，12s 同框包含第一人称轨道、山谷/河道走廊、对岸坡面与远处山形，最接近“临江高弯 + 远中景”验收目标。
- **参考锚点**：`docs/refs/references.md`（外链，不下载入库；逐文件 license 验证后才可提交图片）
- **下一步动作**：在不打断用户全屏的时机执行 UE commandlet 导入 + `visual-check.ps1` 截图验证。后台已完成 Copernicus GLO-30 DEM 生成、hillshade 检查、真实尺度编码窗口、轨道/占位河线重定位。A1 已删除假天穹/方块云/平面远山；橙色非物理天空留到阶段 B 处理。

## 阶段状态表
| 阶段 | 内容 | 状态 | 出口标准（见 acceptance §3） | 备注 |
|---|---|---|---|---|
| 0 | 英雄段+参照+基线 | ✅ Done | 时间点选定/refs≥3/baseline 存档 | hero=`WaitSeconds 12`; baseline=`Saved\hero-baseline.png`; refs=`docs/refs/references.md` |
| A | 地形单一真相源+真实DEM | 🟦 进行中 | 无假几何/DEM不穿山/scatter贴地/D1≥4 | A1 Done；A2 DEM/track asset generation done；UE import+visual check pending |
| B | 物理天空+光照标定 | ⬜ TODO | 单一物理天空+大气透视/曝光物理化/D2≥4 D5≥3 | |
| C | 分层Nanite地表材质 | ⬜ TODO | 崖壁高频细节无moire/冷灰绿/D1≥4 D5≥4 | |
| D | 江水→轨道/车 | ⬜ TODO | 江面折射流动D3≥4/钢轨D6≥3 | |
| E | 密林+成像收尾 | ⬜ TODO | 密林D4≥4/TSR运动模糊D7≥4 D8≥4/60fps | |
| F | 沿轨道铺开 | ⬜ TODO | 整圈均值≥4无单维<3/60fps | |

状态图例：⬜ TODO ／ 🟦 进行中 ／ ✅ Done ／ ⛔ NEEDS-HUMAN ／ ❌ Blocked

## 打分记录（每轮追加，最新在上）
> 格式：`iterN @t<秒>: D1=.. D2=.. ... 均值=.. 短板=.. 根因/下一步=..`
- `hero-a1-no-fake-sky-ridges @t12s`: D1=1 D2=1 D3=1 D4=0 D5=1 D6=1 D7=2 D8=1，均值=1.0。A1 验收：无方块云、无顶点色天穹、无平面远山；仍有程序化地形、橙色非物理天空、顶点色河道、无植被、方条轨道。根因/下一步=A2 真实 DEM 建地形轮廓；橙色天空留到 B 阶段物理光照/天空标定。
- `hero-baseline @t12s`: D1=1 D2=1 D3=1 D4=0 D5=1 D6=1 D7=2 D8=1，均值=1.0，短板=植被缺失/假天空云/顶点色河道/程序化坡面/方条轨道。根因/下一步=A1 删除假环境几何，A2 用真实 DEM 建立地形轮廓。

## 决策记录（不可逆/重要选择，最新在上）
- 2026-06-18（后台执行）：A2 资产生成已跑通：`scripts/generate-yarlung-landscape-assets.py --source copernicus` 下载/缓存 Copernicus GLO-30 N29E094/N29E095 COG 到 gitignored `SourceAssets/DEM/CopernicusGLO30/`，生成 `Content/Generated/YarlungLandscape/YarlungTsangpo_1009.r16`、macro textures、`manifest.json` 和 `YarlungTsangpo_hillshade.png`。实采高程范围约 **2613m–7144m**，编码窗口改为 **2600m–7300m**，导入 scale 为 X≈6.70m/quad、Y≈8.27m/quad、Z≈917.97。轨道控制点已重定位到 `29.769–29.771N / 94.989–94.991E` 谷底附近，控制点 clearance 约 **18m–82m**；占位河高/river mask 同步到 DEM 谷底。UE 导入与截图未执行，避免抢占用户全屏。
- 2026-06-18（用户拍板）：**A2 选定子区域=大拐弯最深峡谷段**（南迦巴瓦↔加拉白垒之间，世界最深）。bbox `lat 29.745–29.820°N, lon 94.945–95.015°E`（~8.3×6.8km，谷底最深点 29.7697N/94.9899E 居中、加拉白垒作远景雪峰、落差~4500m）。DEM 源=Copernicus GLO-30 优先（ALOS 备选，不用 SRTM）。分辨率维持 1009。**垂直 1:1 真实尺度**：改 commandlet `EncodedMinZ/MaxZ` 为真实海拔(当前 2600–7300m)。导入前先出 hillshade 预览确认河道/雪峰。细节见 plan A2。
- 2026-06-18（用户拍板）：**A2 尺度方案=真实尺度子区域**。近 1:1 导成公里级 Landscape，崖壁/雪山保持真实尺度，过山车样条重定位到峡谷中。**否决"压进 164m 玩具尺度"方案**（Codex 曾算 1083km²→30,668m²，水平~212×/垂直~114× 压缩，第一人称读成桌面沙盘）。轨道样条坐标随新尺度重定位是 A2 核心工作。
- 2026-06-18：A1 完成，代码移除 `SkyDomeMesh`、`CloudLayerMesh`、`DistantRidgeMesh` 及对应 build 函数；验证图 `Saved\hero-a1-no-fake-sky-ridges.png`。不在 A1 里用调色修橙色天空，避免偏离阶段顺序。
- 2026-06-18：阶段 0 英雄段固定为 `WaitSeconds 12`；验收基线图为 `Saved\hero-baseline.png`。候选图为 `Saved\hero-candidates-t3s.png`、`t6s`、`t9s`、`t12s`、`t15s`、`t18s`。
- 2026-06-18：参考锚点使用 `docs/refs/references.md` 外链；public repo 不直接下载普通参考摄影，除非逐文件 license 允许再分发。
- 2026-06-18：旧的 C++/资产未提交改动已丢弃；后续植被/岩石/水体散布应进入 commandlet/PCG/Foliage/独立 scenery actor，而不是 `ACoasterRideActor`。

## NEEDS-HUMAN / Blocked 待办
- [x] **尺度方案**（阶段 A2）：已定=真实尺度子区域（见决策记录）。
- [x] **DEM 选型 + 裁哪段**（阶段 A2）：已定并已生成资产（见决策记录与 plan A2）。源=Copernicus GLO-30；bbox=`lat 29.745–29.820°N, lon 94.945–95.015°E`；分辨率维持 1009；垂直真实海拔编码 2600–7300m；轨道样条已重定位；hillshade 已检查。
- [ ] **A2 UE 导入 + 截图验收**：等待可拉起 UE/截图的时机。运行 `scripts\import-yarlung-landscape.ps1` 后再跑 `scripts\visual-check.ps1`，把结果图和 D1/D2/D3 短板追加到打分记录。
- [ ] **英雄段人工确认**（阶段 0）：已先选 `WaitSeconds 12` 作为最佳努力默认；如用户想换英雄段，再重新截图并更新本文件。

## 最终总结（到位后填写）
- （未到位）
