# 照片级迭代进度（当前仪表盘）

> 这是断点续跑的状态文件。任何 agent 开工前先读这里确定"现在在哪、下一步做什么"，收工前写回。
> 历史长日志已归档到 `docs/plans/archive/photoreal-progress-pre-p0p1-cleanup-2026-06-21.md`。
> 流程：`photoreal-overhaul.md` · 验收：`photoreal-acceptance.md` · 操作：`AGENTS.md` · 脚手架：`codex-iteration-scaffold.md`

## 当前裁决

- **总体视觉：FAIL**。目标仍是第一人称 5km 雅鲁藏布/林芝风景过山车，画面要接近 `docs/refs/local/02_fp_yarlung_coaster_canyon.png` 和 `docs/refs/local/03_fp_mountain_coaster_valley.png`，不是"比原型好"。
- **当前技术路线**：只做第一人称视锥/轨道走廊；不回到 square full-map heightfield；不恢复旧 8 点短环；不靠全图 fallback。
- **当前高收益方向**：继续把山体/峡谷视觉从程序化 heightfield 读感转向真实资产化/作者化的 canyon wall、forest canopy、wet cliff、river、atmosphere 分层；不要无限微调同一张灰绿底座。
- **当前无人值守入口**：先跑 `.\scripts\yarlung-agent-status.ps1`，再按推荐命令执行 `.\scripts\iterate-yarlung.ps1`。每轮必须看 contact sheet 图片后再写结论。

## 最新记录

- **2026-06-21 macro canyon profile v2**：重新对照 `docs/refs/local/02_fp_yarlung_coaster_canyon.png` 与 `03_fp_mountain_coaster_valley.png`，确认目标不是"更高的 smooth wall"，而是山腰第一人称视角下的河谷纵深、森林覆盖山体、灰岩/湿岩崖面、远山层次。尝试 `mountain-macro-profile-v1` 后出现刀切暗缝和大块泥山；v2 改成更宽的山脊/浅冲沟，并增强 terrain vertex color 的森林覆盖。最终验证以已重建材质的 `mountain-macro-profile-v2-material-reset.png` 为准：RiskGate=FAIL，worst=`t90`，risk=1.599，washed=0.252，green=0.150，flat=0.601，edge=0.031。肉眼结论：比基线更有山体尺度和绿量，但仍明显是程序化大面山皮，缺真实树冠颗粒、岩壁破碎、河谷开阔构图；不能按阶段 C 完成。下一步必须转向真实/半真实资产层或重排山腰视线，不能继续只调 profile。
- **2026-06-21 dead source asset cleanup**：删除运行时已死的 source 输入资产：`SourceAssets/PolyHaven/leafy_grass/*.jpg` 与 `SourceAssets/Generated/YarlungLandscape/YarlungTsangpo_macro_*.tga`（约 72MB tracked files）。这些只服务旧 `M_YarlungLandscapeGround` / macro TGA / LeafyGrass 链，live 代码不再引用。同步更新 `photoreal-overhaul.md` 当前架构说明为 corridor static mesh + live material/scenery actor 路线；`worlds-longest-coaster.md` 去掉 macro generated-asset 表述；旧 bug log 加 historical note。验证：`rg` 确认 live `scripts/Source/Config/AGENTS` 无已删资产引用（剩余命中为历史 docs/reviews）；`python -m py_compile` 相关脚本 PASS；`yarlung-agent-status.ps1` PASS。视觉未重评，山体照片级仍 FAIL。
- **2026-06-21 P0/P1 repo hygiene**：旧 145KB `photoreal-progress.md` 已压缩为当前仪表盘，完整历史移到 `docs/plans/archive/photoreal-progress-pre-p0p1-cleanup-2026-06-21.md`；旧半自动 `auto-iterate/AUTOSTATE/Claude verifier` spec 移到 `docs/superpowers/archive/2026-06-18-auto-iterate-design.md` 并标为 historical；根目录两张参考图副本已删除，保留 gitignored `docs/refs/local/` 版本；`yarlung-agent-status.ps1` 增加 dirty 分组，能区分 generated tracked dirty、local-only refs、source/docs dirty、other dirty。验证：`Config/yarlung-iteration.json` JSON parse PASS；`yarlung-agent-status.ps1` 文本 PASS；`yarlung-agent-status.ps1 -Json` PASS；`iterate-yarlung.ps1 -Mode ScreenshotOnly -Preset Quick -SkipCapture -NamePrefix p0p1-cleanup-smoke` PASS（RiskGate=UNKNOWN，产出 manifest/handoff）。视觉未重评，当前山体照片级仍 FAIL。
- **2026-06-20/21 Codex unattended scaffold v2**：`Config/yarlung-iteration.json` 统一默认模式、Quick/Standard/Route/Hero/Final preset、risk gate 阈值、L1-L3 本地参考锚点；`scripts/iterate-yarlung.ps1` 读取 config，输出 `run.json` + `handoff.md` + `RiskGate=OK|WARN|FAIL|UNKNOWN`。最近 smoke：`scaffold-harness-shot-v2` 已 Read，RiskGate=WARN，视觉仍灰盒/假山体。
- **2026-06-20/21 山体 muted base v1**：proxy cliff/scatter 噪声已压掉，错误黑点和程序化肋纹减少；但画面仍是灰绿 heightfield 山皮，缺真实 forest canopy / wet cliff / authored canyon-wall 主体，山体照片级仍 FAIL。
- **2026-06-20/21 截图速度优化**：`visual-survey.ps1` 默认 batch capture，一次 UE 启动内跳多个第一人称时间点；低分辨率两时点约 14.0s，旧 per-shot 约 27.5s。

## 阶段状态

| 阶段 | 状态 | 当前判断 |
|---|---:|---|
| 0 参照/英雄段 | ✅ | 英雄段与 L1-L3 local refs 已确定；本地 refs 不入库。 |
| A 真实地形/路线基础 | ✅ | DEM/长轨道/走廊基础可运行；不再用旧短环。 |
| B 光照/曝光/大气基础 | 🟨 | 物理日光/曝光有基础，但画面仍受山体资产和成像层拖累。 |
| C 山体/峡谷几何 | 🟦 | 当前主瓶颈；heightfield/程序化 mesh 已到低天花板，需资产化/作者化分层。 |
| D 江水/轨道/车 | ⬜ | 水体与结构仍是低保真；先别让它掩盖山体主问题。 |
| E 密林/成像收尾 | ⬜ | 需要 forest canopy、TSR/运动模糊、镜头感。 |
| F 全程铺开/性能 | ⬜ | 需整条 on-rails 视锥验收，当前未到。 |

状态图例：⬜ TODO ／ 🟦 进行中 ／ 🟨 部分完成 ／ ✅ Done ／ ⛔ NEEDS-HUMAN ／ ❌ Blocked

## 下一步动作

1. **先保持 repo 清洁**：运行 `.\scripts\yarlung-agent-status.ps1`，确认 dirty 分组。提交时只 stage intentional files；generated assets dirty 需要说明来自哪次 build/visual change。
2. **继续视觉时用快循环**：
   ```powershell
   .\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix <short-name>
   ```
   如果改变 terrain mesh/vertex color/displacement，改用 `-Mode Terrain`。
3. **下一刀视觉建议**：停止单纯 profile/vertex-color 微调，做真实/半真实山体资产层或山腰视线重排：森林树冠 massing、湿岩/灰岩 cliff mesh、河谷开阔视线、远山层次。目标是多时点第一人称帧都能读成参考图那种真实峡谷山体。不要回到 square full-map fallback。
4. **验收要求**：每次必须打开 `Saved\Diagnostics\<name>.png` contact sheet，并把命令、manifest、risk gate、肉眼 verdict、下一步写回本文件。

## 独立外审索引

- `docs/reviews/2026-06-20-architecture-and-cleanup.md`：架构清理已执行多轮；剩余更激进项不要混进视觉迭代，除非单独验证。
- `docs/reviews/2026-06-20-bottleneck-misframed-strategic-pivot.md`：视觉瓶颈曾被纠偏为整体画面链路；当前仍需避免只烧在低收益 heightfield 微调上。
- `docs/reviews/2026-06-19-nanite-mesh-terrain.md`：Nanite 替换基础成立，但单值 heightfield/DEM 忠实 mesh 仍读成折面；不能把 C 标 Done。

## 历史与证据

- 完整旧进度账本：`docs/plans/archive/photoreal-progress-pre-p0p1-cleanup-2026-06-21.md`
- 迭代脚手架说明：`docs/plans/codex-iteration-scaffold.md`
- 画质基础读物：`docs/learn/rendering-basics.md`
- 本地视觉参考：`docs/refs/local/`（gitignored，license-unverified，不发布）
