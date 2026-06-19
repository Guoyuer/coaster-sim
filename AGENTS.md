# AGENTS.md — 自主迭代操作手册

> 给在本仓库**自主迭代画质**的 AI agent。你的任务：把第一人称过山车画面迭代到**照片级 / 风景如画的雅鲁藏布江**，直到达标。本文件是操作手册；**做什么/什么顺序**看 `docs/plans/photoreal-overhaul.md`，**什么叫到位**看 `docs/specs/photoreal-acceptance.md`，**当前进度**看 `docs/plans/photoreal-progress.md`。

## 0. 任务定义（钉死，别跑偏）
- **Camera-real**：截一帧出来要像真实照片/电影画面，不是"比原型好"。
- **风景如画的雅鲁藏布 / 林芝**：湿润深绿密林、乳白—青绿湍流江水、冷灰/灰绿湿岩崖壁、云带薄雾、远处南迦巴瓦式雪山、晴天蓝天白云（锚点 `CONTEXT.md`）。
- **视角永远是过山车第一人称 on-rails**：相机挂在沿固定闭环轨道运动的车上。没有自由漫游、没有任意机位。
- 由此三条推出的优先级（务必吃满，理由见 plan §"核心策略"）：
  1. 只做"相机视锥沿轨道扫到的体积"，走廊外不做。
  2. **远景/中景 > 近景**（高速第一人称下近景运动模糊一闪而过）。
  3. "相机感"靠成像链（曝光/ACES/TSR/运动模糊/镜头），不靠堆几何。

## 1. 真相源文档（每次迭代开始前必读）
| 文件 | 作用 |
|---|---|
| `docs/plans/photoreal-overhaul.md` | 顺序计划：阶段 0→F，每步目标/改动/验收 |
| `docs/specs/photoreal-acceptance.md` | 验收 spec：到位定义 + 打分量规 + 每阶段出口标准 |
| `docs/plans/photoreal-progress.md` | 进度状态（**你每轮都要更新它**，断点续跑靠它） |
| `docs/refs/README.md` | 参照照片收集说明 + 评分锚点 |
| `CONTEXT.md` / `docs/specs/visual.md` | 视觉锚点与规范 |
| `docs/bugs/2026-06-18-visual-pipeline-bugs.md` | 既往坑（moire/暖色/雾回归/headless 假成功） |

**层级规则**：当前 Yarlung 照片级迭代以本文件、`photoreal-overhaul.md`、`photoreal-acceptance.md`、`photoreal-progress.md` 为最高优先级。旧的 `docs/specs/*.md` 是基础仿真/M1 设计资料；若它们允许 free camera、sky dome、procedural placeholder、灰盒美术等，与照片级计划冲突时，按照片级计划执行。

## 2. 环境与命令
引擎：UE 5.8。路径：
- 编辑器 `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`
- 命令行 `...\UnrealEditor-Cmd.exe`，Build.bat `...\Engine\Build\BatchFiles\Build.bat`
- 工程 `CoasterSim.uproject`（模块 `CoasterSim`），默认关卡 `/Game/Generated/YarlungLandscape/YarlungLandscape_Level`

**构建 C++：**
```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CoasterSimEditor Win64 Development "-Project=$PWD\CoasterSim.uproject" -WaitMutex -NoHotReloadFromIDE
```
**重建资产+关卡管线：** `.\scripts\import-yarlung-landscape.ps1 -Build`
（顺序：生成高度图/macro → 建材质 → 导模型 → commandlet 重建 .umap。**.umap 是生成物，关卡 actor 改动必须改 `YarlungLandscapeImportCommandlet.cpp` 再重跑，手摆会被覆盖。**）
**画面验收（高分辨率第一人称截图）：**
```powershell
.\scripts\visual-check.ps1 -Build -Name "iterN" -ResX 2560 -ResY 1440 -WaitSeconds <英雄段时间点>
```

## 3. 迭代循环（每一轮严格照做）
1. **读状态**：读 `photoreal-progress.md`，确定当前阶段与下一个未完成任务。
2. **取任务**：从 `photoreal-overhaul.md` 取该任务的目标/改动/验收。
3. **改一处**：只做这一个任务的最小改动（high-ceiling 方法，见 §4 禁令）。
4. **构建**：编译 C++ / 跑相关管线步骤；失败先修构建。
5. **出图**：跑 `visual-check.ps1` @1440p，在**固定英雄段时间点**截图到 `Saved\iterN-*.png`。
6. **看图（强制）**：用 **Read 工具把每张 PNG 当图片打开**，按 `photoreal-acceptance.md` 的量规逐维打分，并和 `docs/refs/` 参照图对比。**只跑脚本不看图 = 没做验收，不允许。**
7. **判定**：
   - 达到该任务/阶段出口标准 → 标记完成。
   - 出现回归或不达标 → 用 systematic-debugging 找**根因**（材质图错误、光照量级、时序、moire 等），**不许用临时覆盖几何/调色掩盖**。
8. **提交**：一个任务一个 commit，message 末尾加 `Co-Authored-By: Claude <noreply@anthropic.com>` 行（按仓库约定）。
9. **写状态**：更新 `photoreal-progress.md`（当前阶段、分数、截图名、根因/决策、下一步）。
10. **回到 1**，直到 §5 的"到位"全部满足。

## 4. 硬禁令（违反即视为失败）
- ❌ "脚本没报错就当完成"——必须 Read 截图肉眼核对。
- ❌ 用临时覆盖几何、顶点色色块、一次性调色掩盖资产/材质问题（`CONTEXT.md` Visual Iteration Rule）。
- ❌ 往 `ACoasterRideActor::RebuildEnvironment` 里加新美术——造景要迁出 C++、作者化到关卡。
- ❌ 把永久植被/岩石/天空/水体 scatter 写进 `ACoasterRideActor`。这些属于 commandlet 生成的关卡内容、PCG/Foliage、或独立 scenery actor；`ACoasterRideActor` 只保留仿真、轨道/车、相机和必要的运行时查询。
- ❌ headless 脚本只信 exit code——保留 success-marker / 资产存在性校验。
- ❌ 把预算花在相机看不到的走廊外、或高速近景上（违反优先级 2）。
- ❌ 接 Landscape 近景法线不做宏观/微观尺度分离（会重现 moire）。

## 5. 到位 = 停止条件
全部满足才算到位（细则见 `photoreal-acceptance.md`）：
- 英雄段 + 整条走廊的第一人称帧，按量规每维 ≥ 阈值，整体"像照片不像游戏"。
- 对照 `docs/refs/` 参照照片，地形轮廓/天空大气/江水/植被/光照可信。
- 性能：桌面 60 FPS @ 高画质（`stat unit` 验证，为 VR 90 留头寸）。
- `photoreal-progress.md` 中阶段 0→F 全部 Done。
达到后停止并在进度文件写最终总结，不要继续无意义微调。

## 6. 需要人类拍板的门（遇到就这样处理）
这些不要瞎猜，按下述处理并在进度文件标 `NEEDS-HUMAN`：
1. **DEM 数据源与世界对齐**（plan A2）：可用公开 DEM（SRTM/ALOS/Copernicus，公有领域）下载并尝试对齐现有轨道走廊；**对齐尺度/朝向若不确定，做最佳努力并把假设写清，标 NEEDS-HUMAN 等确认**，不要让轨道穿山。
2. **英雄段选哪段 + 参照照片选哪几张**（plan 阶段 0）：属审美决策。可先按"临江高弯、同帧见江+对岸崖壁+远山"自选一段并出基线，标 NEEDS-HUMAN 请人确认后再大规模投入。
3. **任何付费资产**：只用 CC0 / Fab-Megascans(随 UE 免费) / 公开 DEM。参照照片仅用于 `docs/refs/` 对比，不进产品。
遇到真正无法自动解决的阻塞（如需登录/付费/外部数据缺失），写清阻塞点停下，别用低质方案硬填。
