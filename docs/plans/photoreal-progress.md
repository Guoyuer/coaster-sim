# 照片级迭代进度（活文档 — 每轮迭代必须更新）

> 这是断点续跑的状态文件。任何 agent 开工前先读这里确定"现在在哪、下一步做什么"，收工前写回。
> 流程：`photoreal-overhaul.md` · 验收：`photoreal-acceptance.md` · 操作：`AGENTS.md`

## 当前状态
- **当前阶段**：阶段 A/B 交叉 — A1 Done；A2 **参数层已验收通过**；**B2 颜色根因（橙黄）已验收通过**，蓝天/绿地恢复；**B2 曝光收尾已验收通过**（亮度进入晴天可读区间）。当前真正挡住 D1 的问题是 **A2 地形"梯田台阶"疑似 8-bit 高程量化**。
- **基线**：当前为原型/灰盒（顶点色假天空/假河、Cube 轨道、程序化解析地形、单张 macro 平涂地表）。基线截图：`Saved\hero-baseline.png`（2560×1440，`WaitSeconds 12`）
- **英雄段时间点**：`WaitSeconds 12`。理由：六张候选图（3/6/9/12/15/18s）中，12s 同框包含第一人称轨道、山谷/河道走廊、对岸坡面与远处山形，最接近“临江高弯 + 远中景”验收目标。
- **参考锚点**：`docs/refs/references.md`（外链，不下载入库；逐文件 license 验证后才可提交图片）
- **下一步动作（按顺序，两步）**：
  1. **修 A2 高程台阶**（真正卡 D1）：核查 `YarlungTsangpo_1009.r16` 与导入链是不是被压成 8-bit/posterize（256 级摊 4700m ≈ 18m/台阶，和画面台阶高度吻合）。确认是真 16-bit 后台阶应消失，才可验 D1≥4 标 A2 Done。
  2. **继续 B1/B2 未覆盖项**：在台阶根因修完后补体积云/大气透视与作者化天空；B2 物理曝光本轮已收尾，但阶段 B 整体仍未达到 D2≥4。
- **禁区**：不要再调 SkyAtmosphere 散射；B 阶段只允许动太阳角度/强度、相机曝光、作者化天空/云与薄雾。不要用调色/几何遮盖去"掩盖"台阶——必须从 DEM 导入根因修。

## 阶段状态表
| 阶段 | 内容 | 状态 | 出口标准（见 acceptance §3） | 备注 |
|---|---|---|---|---|
| 0 | 英雄段+参照+基线 | ✅ Done | 时间点选定/refs≥3/baseline 存档 | hero=`WaitSeconds 12`; baseline=`Saved\hero-baseline.png`; refs=`docs/refs/references.md` |
| A | 地形单一真相源+真实DEM | 🟦 进行中 | 无假几何/DEM不穿山/scatter贴地/D1≥4 | A1 Done；A2 参数 PASS；**蓝天下暴露出"梯田台阶"疑似 8-bit 高程量化**，卡 D1 |
| B | 物理天空+光照标定 | 🟦 进行中 | 单一物理天空+大气透视/曝光物理化/D2≥4 D5≥3 | **B2 橙黄根因 + 曝光收尾 PASS**；仍缺云带/大气透视/材质层次 |
| C | 分层Nanite地表材质 | ⬜ TODO | 崖壁高频细节无moire/冷灰绿/D1≥4 D5≥4 | |
| D | 江水→轨道/车 | ⬜ TODO | 江面折射流动D3≥4/钢轨D6≥3 | |
| E | 密林+成像收尾 | ⬜ TODO | 密林D4≥4/TSR运动模糊D7≥4 D8≥4/60fps | |
| F | 沿轨道铺开 | ⬜ TODO | 整圈均值≥4无单维<3/60fps | |

状态图例：⬜ TODO ／ 🟦 进行中 ／ ✅ Done ／ ⛔ NEEDS-HUMAN ／ ❌ Blocked

## 打分记录（每轮追加，最新在上）
> 格式：`iterN @t<秒>: D1=.. D2=.. ... 均值=.. 短板=.. 根因/下一步=..`
- `b2-daylight-exposure-v1 @t12s`: D1=2 D2=3.5 D3=1 D4=0 D5=2 D6=1 D7=2 D8=2.5，均值=1.75。验证图：`Saved\OffscreenShots\b2-daylight-exposure-v1.png`（2560×1440）。改动：`AutoExposureBias 0.0 → +1.2`，保留 `ISO100 / 1/500 / f11` 物理相机参数。像素均值从上一版约 `(65.85,87.98,93.60)` 提升到 `(94.15,122.12,129.62)`；肉眼看蓝天/岩壁/轨道进入晴天可读区间，B2 曝光收尾判 PASS。未到位项清楚暴露：左侧峡谷壁硬水平台阶仍严重，江水/轨道/材质/植被仍是占位，下一步转 A2 高程台阶根因。
- **【独立验收 / 人类判官 2026-06-18 第二次】B2 颜色根因 PASS，曝光未完，新发现 D1 台阶**：核对 `CoasterRideActor.cpp:169-170` 确认 `SkyAtmosphere` 三行手调散射已全删、用物理默认；`:157` SkyLight realtime capture；`:128-134` 已切 `AEM_Manual`+物理相机曝光；`:180` 体积雾关。截图 `b2-daylight-rayleigh-default-v2` **橙黄彻底消失、蓝天回归**——外援诊断（Rayleigh 被放大~33×）兑现，**B2 去橙主目标判 PASS**。但两处未到位：① **曝光偏暗**：算得 f/11·1/500·ISO100 ≈ EV≈15.9，比 sunny-16 暗~1.2 档，整幅读成黄昏/阴天，非"风景如画晴天"→ B2 曝光标定记为进行中。② **D1 新发现**：左侧峡谷壁是水平硬台阶（梯田状），疑似高程图被当 8-bit 量化（256 级 over 4700m≈18m/级，与画面吻合），**台阶不修地形再亮也过不了 D1**，属 A2 账。过程认可：未作弊调色、未谎报、按建议根因修。**裁决：放行继续，下一步先收尾 B2 曝光(EV~14.6)，再回查 A2 8-bit 台阶。** 评分 D2 由 3 上修为 3.5（蓝天对但偏暗）。
- `b2-daylight-rayleigh-default-v2 @t12s`: D1=2 D2=3 D3=1 D4=0 D5=2 D6=1 D7=2 D8=2，均值=1.6。验证图：`Saved\OffscreenShots\b2-daylight-rayleigh-default-v2.png`（2560×1440）。结论：外援诊断命中，移除非物理 `SkyAtmosphere` Rayleigh/Mie/AerialPerspective overrides 后，橙黄天空/红褐地形消失，画面恢复蓝天与绿地；`SkyLight` 开 realtime capture，物理相机 `ISO100 / 1/500 / f11`，体积雾临时关闭。仍不达照片级：无云带、左侧近景地形块面/切槽强、地表材质平、轨道/支撑线过乱。下一步=回到 A2 视觉层，先处理轨道走廊与地形构图/块面问题。
- `b2-daylight-rayleigh-default-v1 @t12s`: D1=2 D2=3 D3=1 D4=0 D5=2 D6=1 D7=2 D8=2，均值=1.6。验证图：`Saved\OffscreenShots\b2-daylight-rayleigh-default-v1.png`。Rayleigh/Mie/AerialPerspective 改回默认后第一次验证：蓝天和绿色地形回归，证明橙黄不是贴图源、不是 HDR、不是截图路径，而是大气散射设置。
- `b2-daylight-jump-v1 @t12s`: D1=2 D2=1 D3=1 D4=0 D5=1 D6=1 D7=2 D8=1，均值=1.1。验证图：`Saved\OffscreenShots\b2-daylight-jump-v1.png`（2560×1440）。工作流 PASS：`scripts\offscreen-shot.ps1` 已默认用 `-CoasterStartSeconds=<WaitSeconds>` 直接跳到英雄帧附近，只截 `CaptureSeconds=1`，临时 `MovieFrame*.png` 自动清理；不再为 `WaitSeconds 12` 生成上千帧。画面 FAIL：仍为橙色天空 + 黑色峡谷剪影，B2 v1 物理光照/曝光改动未解决欠曝；下一步=继续诊断 sun direction / exposure method / SkyAtmosphere 高海拔交互。
- **【独立验收 / 人类判官 2026-06-18】A2 参数层 PASS，视觉层 BLOCKED**：核对 commandlet 确认世界 6.76×8.34km、垂直真实海拔 2600–7300m、分辨率 1009、样条已重定位谷底（X~216m），均符合规格；`YarlungTsangpo_hillshade.png` 经肉眼确认是真实峡谷河谷地形（非程序噪声）。但 `offscreen-smoke.png` 严重橙红欠曝，**D1 壮阔尺度无法判定**（同意 Codex 自评 D1≈2、均值~1.1，不注水）。过程认可：未作弊调色、未谎报 Done、offscreen 截图工具考虑周到。**裁决：橙黑是唯一挡路项 → 调整顺序，B2 光照提前到 A2 视觉验收之前。** 待跟进：hillshade 中部一条竖向 DEM 瓦片拼接缝，后续留意别在地表显形。 D1=2 D2=1 D3=1 D4=0 D5=1 D6=1 D7=2 D8=1，均值=1.1。A2 smoke：`scripts\offscreen-shot.ps1` 成功生成 `Saved\OffscreenShots\offscreen-smoke.png`（2560×1440，`-RenderOffScreen` + `-DUMPMOVIE`）。真实 DEM 轮廓/峡谷尺度已进入画面，但画面严重橙红/低曝光，近处地形与轨道关系仍不可信；下一步=继续 A2 调整轨道/地形 clearance 或构图，然后 B 阶段处理物理天空/曝光。
- `hero-a1-no-fake-sky-ridges @t12s`: D1=1 D2=1 D3=1 D4=0 D5=1 D6=1 D7=2 D8=1，均值=1.0。A1 验收：无方块云、无顶点色天穹、无平面远山；仍有程序化地形、橙色非物理天空、顶点色河道、无植被、方条轨道。根因/下一步=A2 真实 DEM 建地形轮廓；橙色天空留到 B 阶段物理光照/天空标定。
- `hero-baseline @t12s`: D1=1 D2=1 D3=1 D4=0 D5=1 D6=1 D7=2 D8=1，均值=1.0，短板=植被缺失/假天空云/顶点色河道/程序化坡面/方条轨道。根因/下一步=A1 删除假环境几何，A2 用真实 DEM 建立地形轮廓。

## 决策记录（不可逆/重要选择，最新在上）
- 2026-06-18（截图工作流默认切换）：低打扰 `scripts\offscreen-shot.ps1` 已连续验证可用，并且直接跳到英雄帧、只保留最终 PNG。默认验收路径改为 offscreen；删除旧窗口截图路径，避免后续误用抢焦点脚本。性能验收以后单独用 `stat unit`/CSV，不再和截图脚本绑定。
- 2026-06-18（B2 曝光收尾）：采用物理相机曝光补偿而非调色遮盖，`AutoExposureBias` 从 `0.0` 提到 `+1.2`，等效把当前 `f/11 · 1/500 · ISO100` 从 EV≈15.9 拉回晴天可读区间。`b2-daylight-exposure-v1` 肉眼与像素统计均确认亮度提升，橙黄未回归；B2 曝光子目标 PASS。阶段 B 仍未 Done，因为还缺体积云/大气透视/材质层次。
- 2026-06-18（B2 验收裁定 + 路径）：**B2 去橙根因判 PASS**（蓝天回归，见独立验收）。但 B2 整体未 Done，拆成两条明确下一步并定序：① **先收尾曝光**——当前 EV≈15.9 偏暗，调亮到晴天 EV~14.5–15（`AutoExposureBias`+1.0~1.3 或 ISO/快门等效），便宜且能立刻看清地形；② **再修 A2 高程台阶**——蓝天下暴露左壁"梯田台阶"，疑似 8-bit/posterize 量化，必须从 `.r16`/DEM 导入链根因修，**禁止用调色或几何遮盖掩盖**。D1 验收以台阶消失为前提。
- 2026-06-18（B2 根因，外援诊断验证）：橙黄天空/红褐地形根因是 `SkyAtmosphere` 散射被手调离物理默认：`SetRayleighScatteringScale(1.1f)` 约等于把默认 Rayleigh 放大数十倍，叠加错误的 `SetAerialPerspectiveStartDepth(9000.0f)`，导致大气像厚黄雾/夕阳。已删除 Rayleigh/Mie/AerialPerspective overrides，保留 UE 默认物理大气；以后不要在 C++ 里手调散射/星球参数。可动项限制为太阳角度/强度、相机曝光、作者化天空/云与合理薄雾。
- 2026-06-18（截图工作流修正）：`scripts\offscreen-shot.ps1` 默认不再按 `WaitSeconds` 全程模拟并 `-DUMPMOVIE` 每帧输出，而是把 `-CoasterStartRatio`、`-CoasterStartSpeed`、`-CoasterStartSeconds=<WaitSeconds>` 传给 runtime，直接把第一人称车推进到目标帧附近，再用 `CaptureSeconds=1` / `CaptureFps=1` 截短片段并自动删除源 `MovieFrame*.png`。如需旧行为，可显式加 `-SimulateWait`；如需保留源帧，可加 `-KeepSourceFrames`。
- 2026-06-18（用户拍板，顺序调整）：**B2 光照/曝光提前到 A2 视觉验收之前**。原因：A2 真实 DEM 参数层已验收通过，但画面严重橙红欠曝，无法肉眼判定峡谷尺度（D1）与轨道贴地；在黑暗里继续调 A2 几何是白费。先 B2 把场景照亮成晴天物理光照，再回头验 A2 地形。A 的其余项（A3 scatter 迁出）仍在 B2 之后按原顺序。
- 2026-06-18（低打扰截图实测）：`scripts\offscreen-shot.ps1` 可用。命令：`powershell -ExecutionPolicy Bypass -File scripts\offscreen-shot.ps1 -Name offscreen-smoke -WaitSeconds 2 -ResX 2560 -ResY 1440 -TimeoutSeconds 180`。输出：`Saved\OffscreenShots\offscreen-smoke.png`，源帧 `Saved\Screenshots\WindowsEditor\MovieFrame00843.png`。脚本修正：加 `-ForceRes` 才能稳定 2560×1440；`Start-Process` 的 `ExitCode` 可能为空，不能当失败。日志仍会出现 Slate window 生命周期记录，因此它是低打扰/offscreen 渲染路径，不等同于严格无窗口证明。
- 2026-06-18（后台执行）：A2 资产生成已跑通：`scripts/generate-yarlung-landscape-assets.py --source copernicus` 下载/缓存 Copernicus GLO-30 N29E094/N29E095 COG 到 gitignored `SourceAssets/DEM/CopernicusGLO30/`，生成 `Content/Generated/YarlungLandscape/YarlungTsangpo_1009.r16`、macro textures、`manifest.json` 和 `YarlungTsangpo_hillshade.png`。实采高程范围约 **2613m–7144m**，编码窗口改为 **2600m–7300m**，导入 scale 为 X≈6.70m/quad、Y≈8.27m/quad、Z≈917.97。轨道控制点已重定位到 `29.769–29.771N / 94.989–94.991E` 谷底附近，控制点 clearance 约 **18m–82m**；占位河高/river mask 同步到 DEM 谷底。UE 导入与截图未执行，避免抢占用户全屏。
- 2026-06-18（低打扰截图探索）：新增实验脚本 `scripts/offscreen-shot.ps1`，尝试用 `UnrealEditor-Cmd.exe -RenderOffScreen` + UE 自己输出 PNG/`-DUMPMOVIE`，避免旧窗口截图脚本的窗口创建和桌面抓图路径。尚未实测；若成功，可用于用户全屏游戏期间的视觉 smoke test。`-NullRHI` 仍只适合 commandlet/资产导入，不能用于照片级截图。
- 2026-06-18（用户拍板）：**A2 选定子区域=大拐弯最深峡谷段**（南迦巴瓦↔加拉白垒之间，世界最深）。bbox `lat 29.745–29.820°N, lon 94.945–95.015°E`（~8.3×6.8km，谷底最深点 29.7697N/94.9899E 居中、加拉白垒作远景雪峰、落差~4500m）。DEM 源=Copernicus GLO-30 优先（ALOS 备选，不用 SRTM）。分辨率维持 1009。**垂直 1:1 真实尺度**：改 commandlet `EncodedMinZ/MaxZ` 为真实海拔(当前 2600–7300m)。导入前先出 hillshade 预览确认河道/雪峰。细节见 plan A2。
- 2026-06-18（用户拍板）：**A2 尺度方案=真实尺度子区域**。近 1:1 导成公里级 Landscape，崖壁/雪山保持真实尺度，过山车样条重定位到峡谷中。**否决"压进 164m 玩具尺度"方案**（Codex 曾算 1083km²→30,668m²，水平~212×/垂直~114× 压缩，第一人称读成桌面沙盘）。轨道样条坐标随新尺度重定位是 A2 核心工作。
- 2026-06-18：A1 完成，代码移除 `SkyDomeMesh`、`CloudLayerMesh`、`DistantRidgeMesh` 及对应 build 函数；验证图 `Saved\hero-a1-no-fake-sky-ridges.png`。不在 A1 里用调色修橙色天空，避免偏离阶段顺序。
- 2026-06-18：阶段 0 英雄段固定为 `WaitSeconds 12`；验收基线图为 `Saved\hero-baseline.png`。候选图为 `Saved\hero-candidates-t3s.png`、`t6s`、`t9s`、`t12s`、`t15s`、`t18s`。
- 2026-06-18：参考锚点使用 `docs/refs/references.md` 外链；public repo 不直接下载普通参考摄影，除非逐文件 license 允许再分发。
- 2026-06-18：旧的 C++/资产未提交改动已丢弃；后续植被/岩石/水体散布应进入 commandlet/PCG/Foliage/独立 scenery actor，而不是 `ACoasterRideActor`。

## NEEDS-HUMAN / Blocked 待办
- [x] **尺度方案**（阶段 A2）：已定=真实尺度子区域（见决策记录）。
- [x] **DEM 选型 + 裁哪段**（阶段 A2）：已定并已生成资产（见决策记录与 plan A2）。源=Copernicus GLO-30；bbox=`lat 29.745–29.820°N, lon 94.945–95.015°E`；分辨率维持 1009；垂直真实海拔编码 2600–7300m；轨道样条已重定位；hillshade 已检查。
- [x] **A2 UE 导入 + smoke 截图**：`scripts\import-yarlung-landscape.ps1 -Verify` 已成功，`offscreen-shot.ps1` 已产出 smoke PNG。
- [x] **B2 橙黄根因（颜色）**：删 SkyAtmosphere 手调散射 + 默认物理大气 + SkyLight realtime capture，蓝天/绿地恢复，独立验收 PASS；截图 `b2-daylight-rayleigh-default-v2`。
- [x] **B2 曝光收尾**：`AutoExposureBias +1.2`，截图 `Saved\OffscreenShots\b2-daylight-exposure-v1.png`；蓝天/岩壁进入晴天可读区间，像素均值提升且橙黄未回归。
- [ ] **A2 高程台阶修复（下一步②，卡 D1 的真问题）**：核查 `Content/Generated/YarlungLandscape/YarlungTsangpo_1009.r16` + 生成脚本 + 导入链是否被压成 8-bit/posterize（左壁可见 ~18m 梯田台阶）。确认真 16-bit、台阶消失后才可验 D1≥4 标 A2 Done。禁止调色/几何遮盖掩盖。
- [x] **低打扰截图实测**：已成功。推荐命令：`powershell -ExecutionPolicy Bypass -File scripts\offscreen-shot.ps1 -Name <name> -WaitSeconds <seconds> -ResX 2560 -ResY 1440`。注意这是 offscreen/低打扰 smoke path，最终验收仍需确认画面有效并按量规打分。
- [ ] **英雄段人工确认**（阶段 0）：已先选 `WaitSeconds 12` 作为最佳努力默认；如用户想换英雄段，再重新截图并更新本文件。

## 最终总结（到位后填写）
- （未到位）
