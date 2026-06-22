# 视觉路线假阴性复审协议

> 目的：避免把“pipeline bug / 资产接线 bug / 遮挡 bug / 截图验收 bug”误判成“路线失败”。任何视觉方向被否掉前，必须先证明它不是被基础链路污染。

## 核心原则

- 画面不符合参考图时，第一假设是 **BUG 或接线缺口**，不是路线本身错误。
- “不好看”要拆成三类：`BUG`、`CONTENT-LIMIT`、`DIRECTION-LIMIT`。只有第三类才允许否定方向。
- 不能因为一次截图失败就把高天花板路线打死；如果遮挡、材质、资产覆盖、法线、曝光、截图时点、gate 配置还没排除，只能标为 WIP 或 reopen candidate。
- 旧实现可以被删除，旧方向不一定被否定。删除 `YarlungCanyonWallActor` 不等于否定“连续岩壁”；删除 UE Water gate-only 链路不等于否定“高质量江水”。

## 否定路线前必须记录的证据

1. **观察到的视觉 mismatch**：引用 contact sheet / 单帧路径，说明和 `docs/refs/local/` 的差距是什么。
2. **隔离截图或等价证据**：至少用一类隔离手段排除管线 bug，例如：
   - `-YarlungHideTerrain`
   - `-YarlungHideScenery`
   - `-YarlungHideRide`
   - map inspect / asset inspect / material compile warning
   - 生成资产存在性和 object path 检查
   - 同一时点不同 layer 的对照图
3. **失败归因**：
   - `BUG`：实现、接线、遮挡、材质、法线、曝光、截图、gate 有错误；先修。
   - `CONTENT-LIMIT`：链路通了，但资产质量、覆盖、构图或 authored content 不够；继续换资产/补内容。
   - `DIRECTION-LIMIT`：bug 和内容缺口已排除，路线本身天花板不够；才允许归档/删除。
4. **后续动作**：写入 `docs/plans/photoreal-progress.md`。如果没有足够证据，禁止写“失败路线”，只能写“未验证 / 需复审 / WIP”。

## 当前应重开的方向

- **显式 river surface 高质量江水**：旧 procedural river 和 gate-only UE Water 已删是正确的；但“单一显式水面 + flow normal / depth fade / reflection / 岸边泡沫”仍是主线，不应因过去水不可见、双水体、法线平面、SingleLayerWater 输出错误而否定。
- **连续 Nanite cliff / rock-wall kitbash**：旧程序化 canyon wall 被删是正确的；但“连续湿岩崖壁”是参考图核心，应以 Megascans 模块化 cliff/rock wall 和 authored placement 继续做，不用旧 actor 复活。
- **真实 foliage / forest-floor / scree 层**：branch-clump HISM 只是过渡；早期森林不真实多半是资产形态和轨道净空问题，不是“密林路线失败”。
- **蓝天白云和大气层**：Rayleigh、cached pre-exposure、阴天云层曾污染判断。山体/水完成前不作为最高优先级，但后续可复审。
- **轨道跨江、高架、贴崖、翻滚**：路线拓扑已放宽；若画面不对，先排除内容覆盖、相机 pitch、遮挡和水/山体可见性，再判断是否需要改路线。

## 当前仍保持删除/不恢复的旧实现

- `YarlungCanyonWallActor` 程序化幕布。
- `YarlungRiverActor` 旧 procedural ribbon。
- `WaterZone` / `WaterBodyRiver` gate-only 双水体链路。
- square full-map / UE Landscape fallback。
- 盲目增加 scatter 密度、盲目扩大河面几何波幅、只调曝光掩盖内容问题。

## 写进进度文件的模板

```text
- False-negative audit: <direction>
  - Evidence: <screenshot / inspect / isolate command>
  - Verdict: BUG | CONTENT-LIMIT | DIRECTION-LIMIT
  - Decision: <fix / continue / archive>
  - Next: <one concrete next action>
```
