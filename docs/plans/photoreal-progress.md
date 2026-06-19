# 照片级迭代进度（活文档 — 每轮迭代必须更新）

> 这是断点续跑的状态文件。任何 agent 开工前先读这里确定"现在在哪、下一步做什么"，收工前写回。
> 流程：`photoreal-overhaul.md` · 验收：`photoreal-acceptance.md` · 操作：`AGENTS.md`

## 当前状态
- **当前阶段**：阶段 A（地形单一真相源 + 真实 DEM）— A1 Done，A2 待开始
- **基线**：当前为原型/灰盒（顶点色假天空/假河、Cube 轨道、程序化解析地形、单张 macro 平涂地表）。基线截图：`Saved\hero-baseline.png`（2560×1440，`WaitSeconds 12`）
- **英雄段时间点**：`WaitSeconds 12`。理由：六张候选图（3/6/9/12/15/18s）中，12s 同框包含第一人称轨道、山谷/河道走廊、对岸坡面与远处山形，最接近“临江高弯 + 远中景”验收目标。
- **参考锚点**：`docs/refs/references.md`（外链，不下载入库；逐文件 license 验证后才可提交图片）
- **下一步动作**：执行 plan 阶段 A2（真实 DEM 地形）。A1 已删除 `ACoasterRideActor` 内假天穹/方块云/平面远山；截图暴露出橙色/非物理天空，但按计划留到阶段 B 光照标定处理。

## 阶段状态表
| 阶段 | 内容 | 状态 | 出口标准（见 acceptance §3） | 备注 |
|---|---|---|---|---|
| 0 | 英雄段+参照+基线 | ✅ Done | 时间点选定/refs≥3/baseline 存档 | hero=`WaitSeconds 12`; baseline=`Saved\hero-baseline.png`; refs=`docs/refs/references.md` |
| A | 地形单一真相源+真实DEM | 🟦 进行中 | 无假几何/DEM不穿山/scatter贴地/D1≥4 | A1 Done；DEM 源待定，见 NEEDS-HUMAN |
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
- 2026-06-18：A1 完成，代码移除 `SkyDomeMesh`、`CloudLayerMesh`、`DistantRidgeMesh` 及对应 build 函数；验证图 `Saved\hero-a1-no-fake-sky-ridges.png`。不在 A1 里用调色修橙色天空，避免偏离阶段顺序。
- 2026-06-18：阶段 0 英雄段固定为 `WaitSeconds 12`；验收基线图为 `Saved\hero-baseline.png`。候选图为 `Saved\hero-candidates-t3s.png`、`t6s`、`t9s`、`t12s`、`t15s`、`t18s`。
- 2026-06-18：参考锚点使用 `docs/refs/references.md` 外链；public repo 不直接下载普通参考摄影，除非逐文件 license 允许再分发。
- 2026-06-18：旧的 C++/资产未提交改动已丢弃；后续植被/岩石/水体散布应进入 commandlet/PCG/Foliage/独立 scenery actor，而不是 `ACoasterRideActor`。

## NEEDS-HUMAN / Blocked 待办
- [ ] **DEM 数据源与对齐**（阶段 A2）：选 SRTM 30m / ALOS AW3D30 / Copernicus GLO-30 哪个？真实地形尺度/朝向如何对齐现有轨道走廊（X∈[-1800,10400], Y∈[-3900,1500]，`YarlungCoasterProfile.h:17`）？— 可先最佳努力并标假设。
- [ ] **英雄段人工确认**（阶段 0）：已先选 `WaitSeconds 12` 作为最佳努力默认；如用户想换英雄段，再重新截图并更新本文件。

## 最终总结（到位后填写）
- （未到位）
