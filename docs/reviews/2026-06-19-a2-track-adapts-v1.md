# A2 Track Adapts V1 Independent Review

- Date: 2026-06-19
- Reviewer: independent external reviewer / human judge
- Review target: `a2-track-adapts-v1`
- Reviewed artifacts: `YarlungCoasterProfile.h`, `CoasterRideActor.cpp`, `YarlungLandscapeImportCommandlet.cpp`, `generate-yarlung-landscape-assets.py`, `Saved\OffscreenShots\a2-track-adapts-v1.png`
- Verdict: FAIL
- Related phase/task: A2 terrain/track adaptation gate

## Summary

`a2-track-adapts-v1` is directionally correct: the track should adapt to the
naturalized terrain instead of carving a hard trench through the mountain. The
iteration is also honestly reported as unfinished. However, A2 cannot be
released because whole-loop terrain clearance is not proven.

## Original Review Text

**【独立审阅 / 人类判官 2026-06-19 第四次】`a2-track-adapts-v1` 方向认可，但发现全环「不穿山」无保证、不予放行 A2**：核对 `YarlungCoasterProfile.h`（clearance 半径→`0.0`、8 控制点 Z=naturalized 地形+~22m）、`CoasterRideActor.cpp:83/292`、`YarlungLandscapeImportCommandlet.cpp:71` 与 `generate-yarlung-landscape-assets.py`。诚实性认可：未注水、`a2-track-adapts-v1` 标未完成、方向（轨道适配地形而非挖山）正确。**但 5 项问题，按严重度：**

1. 🔴 **全环不穿山不再有任何保证，验收只看一帧（必须先补）**：`ApplyTrackClearanceCut` 两处调用现都因半径=0 直接 `return Height`，地形完全不再为轨道让路；轨道仅 8 控制点、段间 Z 线性 lerp，代码无 LineTrace 贴地/clamp/防穿插。30m+naturalized 谷壁在两点间可能比线性更快抬升 → 爬坡段中点很可能把相机埋进山。`a2-track-adapts-v1.png` 只是 `WaitSeconds 12` 单机位，证明不了整圈。acceptance §3 阶段 A 出口②要求「轨道不穿山」未验。**放行 A2 前须沿样条密采样地形高度 vs 轨道 Z 做全环 clearance 诊断（仿 `dump-yarlung-height-profile.py`），或多机位绕圈截图。**
2. 🟠 **「撤掉/最小化」被做成「完全撤掉」**：决策记录原意是 clearance cut 作「最后的微小安全兜底」；设 `0.0` 连兜底也删了。更稳：保留很小 inner 半径，只把轨道正下方会戳出的地形压到轨道下方一点点，而非从「挖太多」跳到「完全不挖」。
3. 🟠 **三个分别打分/各有截图的迭代被压成单个 commit `34c2298`**（`git log -S` 证实 B-spline+naturalize+clearance/Z 同 commit）：`a2-bicubic-bspline-v1`/`a2-naturalized-v1` 对应的 `.r16`/代码态无法从 git 还原或 bisect，与「断点续跑/单一真相源」原则相悖。后续迭代请按截图粒度分 commit。
4. 🟡 **地形被平滑三道**（B-spline 近似上采样不过点 + box blur r=18×3 + slope relaxation×8）：有把 D1「壮阔尺度」一起磨掉的风险。建议提交谷深/对岸落差 before/after 量化对照，确认大轮廓未被洗掉。
5. 🟡 **控制点 Z 手算、与 naturalize 参数无绑定**：改 radius/passes/max_slope 会静默使 +22m 净空假设失效，无断言兜住。须把「如何重算 8 个 Z」精确步骤写进脚本/文档。

**裁决：A2 不予放行。先补第 1 项全环穿山诊断（硬门槛），并考虑第 2 项保留微小安全半径；3/4/5 为流程与稳健性改进。**

## Blocking Issues

1. **Whole-loop no-clipping is not guaranteed.** `ApplyTrackClearanceCut` now
   returns the original height because the clearance radius is `0.0`, so the
   terrain no longer yields to the track at all. The track has only 8 control
   points, segment Z is linearly interpolated, and there is no runtime
   line-trace/clamp/no-clipping guard. A single `WaitSeconds 12` screenshot does
   not prove that the full loop stays above terrain. Before A2 can pass, add a
   dense track-vs-terrain clearance diagnostic or multi-view whole-loop proof.
2. **"Minimize clearance cut" became "remove clearance cut completely."** The
   intended decision was to keep a small safety fallback, not to remove every
   protection. A small inner clearance radius is safer than jumping from a large
   hard trench to no fallback.

## Non-Blocking Suggestions

1. Three scored iterations were collapsed into one commit (`34c2298`), making
   the exact states for `a2-bicubic-bspline-v1`, `a2-naturalized-v1`, and
   `a2-track-adapts-v1` hard to restore or bisect. Future iterations should use
   one commit per scored screenshot/diagnostic.
2. The terrain has been smoothed through several stages: B-spline sampling, box
   blur, and slope relaxation. Add before/after measurements for valley depth
   and opposite-slope relief to verify that D1 scale has not been flattened.
3. Track control point Z values are manually calculated and not tied to the
   naturalization parameters. Document or script how to recompute them so
   changes to radius, passes, or max slope cannot silently invalidate the
   clearance assumption.

## Required Next Action

Add a whole-loop clearance diagnostic that densely samples the coaster spline,
compares track/camera Z to the generated terrain height, reports the minimum
clearance and all dangerous segments, and blocks A2 if any sample clips terrain
or is below the accepted clearance threshold.

Consider restoring a very small inner clearance cut as a safety fallback after
the diagnostic proves where it is needed.

## Agent Disposition

- Status: Superseded by generated 5km track path; runtime closure pending
- Progress link: `docs/plans/photoreal-progress.md`
- Follow-up evidence: P1 generated `Content/Generated/YarlungLandscape/YarlungTrack.csv` and `scripts/verify-track-clearance.py` reports `length=5031.8m`, `min_clearance=22.49m`, `violations=0` under the draft scenic-route gate. This supersedes the old 8-point `a2-track-adapts-v1` path but does not make A2 Done until P2 loads the generated track at runtime.
