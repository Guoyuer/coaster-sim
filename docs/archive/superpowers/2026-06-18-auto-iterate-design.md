# Auto-Iterate 半自动驱动器 — 设计 Spec（Historical）

> **历史归档，不是当前路线。** 这份 spec 描述的是 2026-06-18 的半自动 `auto-iterate.ps1` / `AUTOSTATE` / Claude verifier 方案。当前无人值守入口已经改为 `scripts/yarlung-agent-status.ps1` + `scripts/iterate-yarlung.ps1` + `Config/yarlung-iteration.json`；接棒 agent 应按 `AGENTS.md`、`docs/plans/codex-iteration-scaffold.md`、`docs/plans/photoreal-progress.md` 执行。

> 把现在"人工编排 Claude（架构/验收/外援）+ Codex（执行/记进度）"的接力，换成一个驱动脚本，半自动跑、每个任务停一次。
> 配套：流程 `docs/plans/photoreal-overhaul.md` · 验收量规 `docs/specs/photoreal-acceptance.md` · 进度/状态 `docs/plans/photoreal-progress.md` · 操作手册 `AGENTS.md`。

## 1. 背景与目标

现状已经是一套文件消息总线：plan（做什么）+ acceptance（什么叫到位）+ progress（断点续跑/打分/决策）+ offscreen-shot 产出 PNG + git 一任务一 commit。缺的只是把中间的人工接力自动化。

**目标**：新增 `scripts/auto-iterate.ps1`，按任务粒度循环执行，每轮在检查点停下等人。

**已拍板的四个选择**：
1. 执行工保留 Codex（跨工具编排，文件当总线）。
2. 半自动 + 检查点（不追求无人值守）。
3. Claude 角色 = 独立验收 + 卡住时出根因诊断。
4. 检查点粒度 = 每个任务停一次（一个 commit 一轮）。

## 2. 设计红线（不变量）

1. **角色分离**：Codex 只改代码，Claude 只做独立验收/诊断。**两个独立进程**。Claude 进程绝不改代码；Codex 进程绝不写「独立验收」块。这是防"自己给自己打分注水"的根本（对应 `AGENTS.md §4` 的 headless 假成功禁令）。
2. **复用现有文件总线**，不另起一套约定。
3. **证据先于结论**：脚本只认 git/文件系统的**地面真值**，不相信任何 agent 的文字自述。
4. **状态识别不准 = 停**：任何状态歧义/不一致，停在检查点把原始状态摊给人，**绝不猜测继续**。

## 3. 架构与数据流

```
auto-iterate.ps1  (每个任务一轮)
  │
  ├─[1] 执行   codex exec  ← prompts/codex-execute.md
  │       读 progress 取下一个未完成任务 → 只做这一个 → build → offscreen 截图 → 写 progress → commit
  │
  ├─[2] 地面真值校验 (脚本本体，不信任何自述)
  │       git HEAD 变了吗/恰好 +1 commit? · OffscreenShots 目录真多了哪张新 PNG? · ≥1440p? · build success-marker?
  │
  ├─[3] 验收   claude -p  ← prompts/claude-verify.md   (独立进程)
  │       读 acceptance 量规 + 新 PNG + progress + refs → 逐维 dim.D1..D8 打分 → 核对 Codex 自评是否注水
  │       → 追加「独立验收」块 + 更新 AUTOSTATE 块 → 输出机器可读 VERDICT 行
  │
  ├─[4] 诊断 (条件触发)   claude -p  ← prompts/claude-diagnose.md   (重型: +WebSearch +大上下文)
  │       触发条件: VERDICT=FAIL 且回归, 或同一 task fail_streak ≥ 2 → 写根因+下一步进 progress 决策记录
  │
  └─[5] 检查点 (停下等人)
          打印 VERDICT 摘要 + 打开 PNG → 读一个键:
          [Enter]=下一轮  [s]=停  [h]=我亲自介入(退出)  [r]=带诊断重试本任务
```

## 4. 状态识别（核心，三层防线）

散文不可解析。状态识别建立在三层之上，缺一层就退化成猜。

### 4.1 第一层 — 规范任务身份（消除歧义）

任务用计划里的稳定编号作为唯一 ID：`0.1 0.2 0.3 / A1 A2 A3 / B1 B2 / C1 / D1 D2 / E1 E2 / F`。

**命名空间隔离（关键坑）**：计划任务 `D1`（真实江水）与打分维度 `D1`（地形轮廓）重名。状态机一律加前缀区分：
- 任务 → `task:D1`、`task:A2` …
- 打分维度 → `dim.D1` … `dim.D8`

脚本与三个 prompt 模板全程只用带前缀写法，禁止裸 `D1`。

### 4.2 第二层 — 机器状态块 AUTOSTATE（结构化真相）

在 `photoreal-progress.md` 顶部嵌一个**机器读写**的状态块（HTML 注释包裹，人读时不可见、不干扰现有散文）：

```
<!-- AUTOSTATE
schema_version: 1
current_task: task:A2
task_status: in_progress        # todo | in_progress | passed | needs_human | blocked
last_iter: b2-daylight-exposure-v1
last_verdict: PASS              # PASS | FAIL | NEEDS-HUMAN | UNKNOWN
fail_streak: 0
need_help: false               # codex-execute 自报受阻时置 true (辅助触发诊断, 见 §5)
stuck_reason:                  # need_help=true 时的一行原因
updated_by: claude-verify       # 谁最后写的: codex-execute | claude-verify | driver
updated_at_commit: 18e66ac
-->
```

- **只有 claude-verify 进程**在验收后写 `last_verdict / task_status / fail_streak / last_iter`。
- **codex-execute 进程**只允许把 `current_task` 推进到它取的任务、`task_status=in_progress`、`updated_by=codex-execute`；不准碰 verdict 字段。
- **driver**只读，不写业务字段（除非做一致性修正且记日志）。
- 脚本用正则**精确**读写这个块；解析失败 = 状态未知 = 停。

### 4.3 第三层 — 地面真值校验（不信自述）

每轮执行后，driver 自己核对客观事实，与 agent 文字输出无关：

| 校验 | 方法 | 不过怎么办 |
|---|---|---|
| 真的提交了一个任务 | `git rev-parse HEAD` 前后对比；恰好 +1 commit | 0 个 → 执行可能失败，停；>1 个 → 违反"只做一个任务"，停并提示 |
| 真的出了新图 | 截图前后对 `Saved/OffscreenShots/` 做目录 diff，取**真正新增**的 PNG（**不信** Codex 自报的文件名） | 无新增 → 停 |
| 图是验收级 | 读 PNG 尺寸 ≥ 1920×1080（理想 2560×1440）；mtime 是本轮的 | 低于阈值 → 标记 smoke，不进验收，停 |
| build 真的成功 | success-marker / 资产存在性，不只看 exit code（`AGENTS.md §6`） | 无 marker → 停 |
| 验收真的产出裁决 | claude-verify 末行必须是 `VERDICT: PASS|FAIL|NEEDS-HUMAN; TASK: task:<id>; SHORTFALL: <...>` | 无法解析 → 停 |

### 4.4 一致性闸门

三层必须自洽，否则停：
- AUTOSTATE.current_task == VERDICT 行的 TASK == 本轮 Codex 实际改的任务（取自其 prompt 回包）。
- AUTOSTATE.last_verdict == VERDICT 行的判定。
- AUTOSTATE.updated_at_commit == 校验到的新 HEAD。

**任一不一致 → 不修正、不继续，停在检查点，原样打印三方状态让人判。** 这是"状态识别准确"的兜底。

## 5. 卡住检测（双触发：客观信号为主，自报为辅）

复刻人工习惯"识别卡住 → 问要不要外援"，但**不能只靠 Codex 自报**——真卡住的 agent 常不自知甚至谎报成功（这正是要独立验收的原因）。两路信号任一命中即自动触发第 4 步外援诊断。

**主：客观信号（地面真值，可脚本化兜底）**。`fail_streak` 由 claude-verify 维护，按 `current_task` 计数：
- 同一 `task:` 且 `VERDICT=FAIL` → `fail_streak += 1`。
- `PASS` 或 `current_task` 变化 → 归 0。
- `fail_streak >= 2` → 触发诊断，并**强制停**（即使用户上轮选了 Enter 连跑），把"反复失败"显式抛给人。
- 其他客观卡住信号同样触发：本轮无新 commit / 无新 PNG / 验收维度出现回归（任一 `dim.Dx` 比上轮下降）。

**辅：自报信号（便宜的提前升级）**。codex-execute prompt 允许 Codex 在判定自己受阻时，于输出和 AUTOSTATE 写 `need_help: true` + 一行 `STUCK: <原因>`；driver 读到即**立刻**触发诊断（不必等 fail_streak 攒到阈值）。这对应"它主动喊救命就马上给外援"。

**判据**：`触发诊断 = 客观信号命中 OR need_help==true`。自报只能让诊断**提前**，不能阻止客观信号触发——Codex 说"没事"但 fail_streak 到阈值，照样起诊断。

## 6. 组件清单（每个单元：职责/接口/依赖）

| 单元 | 职责 | 接口（输入→输出） | 依赖 |
|---|---|---|---|
| `scripts/auto-iterate.ps1` | 编排循环 + 地面真值校验 + 一致性闸门 + 检查点交互 | CLI flags → 退出码 + 控制台裁决摘要 | git, codex CLI, claude CLI, offscreen 产物 |
| `scripts/prompts/codex-execute.md` | 给 Codex 的执行 prompt 模板 | 当前 AUTOSTATE → 改一处+build+截图+commit+更新 AUTOSTATE | AGENTS.md/plan/progress |
| `scripts/prompts/claude-verify.md` | 给 Claude 的独立验收 prompt | PNG+量规+progress → 「独立验收」块 + AUTOSTATE + VERDICT 行 | acceptance/refs |
| `scripts/prompts/claude-diagnose.md` | 给 Claude 的外援根因诊断 prompt | 失败上下文 → 决策记录里的根因+下一步 | bug 日志/源码 |
| `docs/plans/automation.md` | 运行手册（怎么跑/键位/故障处理/恢复） | — | 本 spec |

AUTOSTATE 的读/写/校验逻辑封装成 `auto-iterate.ps1` 内的独立函数（`Read-AutoState` / `Write-AutoState` / `Assert-Consistency`），便于单测。

## 7. 错误处理与恢复

- **断点续跑**：progress.md（含 AUTOSTATE）就是唯一状态。脚本随时重跑即从 `current_task` 继续，无额外状态文件。
- **回滚单元**：git 一任务一 commit。某轮坏了 `git checkout <prev>` 即回退一个任务。
- **超时**：UE build+截图可能 5–15 分钟。每步设宽松超时（默认 build 20min / 截图 10min / agent 调用 15min，可 flag 调），超时即停并打印对应 log 路径（`Saved\Logs\offscreen-shot.log`、`CoasterSim.log`）。
- **任何"停"**都打印：当前 AUTOSTATE 原文 + 最近 VERDICT + 相关 log 路径 + 建议的人工动作。

## 8. 实现前必须 spike 的未知（不阻塞设计，阻塞编码）

1. **Codex 非交互调用**：`codex exec` 的确切免批准/沙箱标志，确保它能跑 unreal-mcp + Build.bat + git 而不卡在确认；pin Codex 版本号写进 automation.md。
2. **`claude -p` 形态**：`--allowedTools`（至少 Read + 写 progress 所需）、读 PNG 的方式、如何稳定捕获末行 `VERDICT:`（必要时 `--output-format` / 结构化输出）。
3. **offscreen-shot 无人值守**：确认低打扰路径在脚本连跑时不抢焦点、不弹窗（已是 offscreen，确认即可）。

## 9. 测试策略

- `--DryRun`：只打印每步会调用什么、解析出的状态，不真正起 agent。
- **状态解析单测**：喂若干样例 progress.md（含/缺/损坏 AUTOSTATE），断言 `Read-AutoState` 行为；缺/损坏必须返回"未知→停"。
- **完整性反作弊测**：构造"Codex 自报文件名与目录 diff 不符""HEAD 没动""多出 2 个 commit"三种情况，断言 driver 都能拦下并停。
- **命名空间测**：构造同时含 `task:D1` 与 `dim.D1` 的样例，断言不串。
- 一次真实 end-to-end 干跑：拿当前 `task:A2`（高程台阶）跑一轮，人工核对裁决与现有人工验收口径一致。

## 10. 范围外（YAGNI，本期不做）

- 全自动无人值守 / 仅 NEEDS-HUMAN 才停（保留为未来增强）。
- 阶段级检查点（本期固定任务级）。
- 把诊断升级成"自动让 Codex 应用修复并重判"的闭环（本期诊断只写建议，重试由人按 `r` 触发）。
- CI/多机/远程触发。
