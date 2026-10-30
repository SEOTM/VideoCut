# AppState 模块 (src\app\AppState.h)

## 概述

VideoCut 的业务状态与界面联动。本模块只依赖 Kanat 的公开接口（`Dispatch` / `Layout` / `CtrlBuffer` / `AllocatorHub`），不修改框架层。

职责：

- 布局切换：编辑布局（槽 0）是基座（承载窗口），准备布局（槽 1）是它的内嵌(flex)成员；准备界面默认不打开，由「重设目录」（`##RESET`）的 `OnReset` 显示，打开目录后隐藏；编辑界面恒可见（内嵌成员必须沿用基座的窗口段）；
- 控件缓冲：为框架未分配缓冲的控件补缓冲（路径输入），并初始化滑块、进度、倍速、按钮文本；
- 媒体：扫描目录、按分组加载视频流、时长与分辨率、释放与重设；
- 每帧推进：先判活动端时码是否被拖动并定位，再按帧率节流解码，并把当前时码写回活动端；重混线程回收、进度与时间条同步；
- 事件处理：标题栏、浏览、打开、选择、添加片段、预览跳转、起止毫秒、播放、倍速、重混、重设。

### 函数

| 函数 | 说明 |
| --- | --- |
| `Init` | 初始化业务状态（须在 `Dispatch::Init` 之后、宿主启动之前调用）。 |
| `Release` | 释放业务状态（幂等）。 |
| `Frame` | 每帧入口：先推进业务，再转交 `Dispatch::Frame`。 |
| `OnTitleBar` | 标题栏事件（拖动 / 关闭；双击最大化阻塞）。拖动按触发控件所属布局定位；内嵌(flex)布局不采用标题栏回传的 `param`（它以宿主窗口位置为基准），改由鼠标客户区坐标与按下帧记录的抓取偏移结算。内嵌浮层的关闭钮只收起浮层，基座标题栏的关闭钮才退出应用。 |
| `OnBrowse` | 浏览文件夹。 |
| `OnOpenFolder` | 打开文件夹。 |
| `OnSelectVideo` | 视频分组下拉框变更。 |
| `OnAddSpan` | 添加剪辑片段到时间片列表。 |
| `OnSpanSlider` | 双手柄范围滑块变更。 |
| `OnEditBegin` | 起点时码提交。 |
| `OnEditEnd` | 终点时码提交。 |
| `OnListOperation` | 时间片列表行操作（删除 / 选中）。 |
| `OnPlay` | 播放 / 暂停。 |
| `OnSpeed` | 倍速提交。 |
| `OnRemix` | 执行重混导出。 |
| `OnReset` | 重设目录并回到准备界面（准备浮层重新显示）。 |

### 控件缓冲约定

| 控件标签 | 类型 | 缓冲 |
| --- | --- | --- |
| `##INPUT_SRC` | `CTRL_TYPE_INPUT_SINGLE` | 业务替换为 `MAX_PATH * 3` 字节 |
| `##TIMEBAR_BEGIN` / `##TIMEBAR_END` | `CTRL_TYPE_TIMEBAR` | `CtrlTimeBarData`（提示文本 + 起止毫秒 + 上限） |
| `##TIMESPANSLIDER` | `CTRL_TYPE_SLIDER_RANGE` | `CtrlSliderRangeData`（`vMin` / `vMax` = 起止；播放推进时活动端跟随播放头） |
| `##CLIPLIST` | `CTRL_TYPE_LIST_TIMESPAN` | `ListBuffer` + `TimeSpanItem`（容量 128） |
| `##COMBO_VIDEO` | `CTRL_TYPE_COMBO` | `CtrlComboData`（`items` 指向组织器名称） |
| `##PROG_REMIX` | `CTRL_TYPE_PROG` | `CtrlProgData` |
| `##VIDEORENDER` | `CTRL_TYPE_RENDER` | `CtrlRenderData`（设备就绪后传纹理） |

---

## Init 函数

### 语法

```cpp
BOOL Init();
```

### 返回值

`TRUE` = 状态块建立成功；`FALSE` = 状态块分配失败。

### 注解

调用前布局必须已装配；函数会按标签查找编辑布局的控件并补缓冲。启动时把准备布局登记为内嵌(flex)成员并置为
不可见（默认停留编辑界面）：编辑界面作为基座承载窗口段，准备界面是叠在其上的浮层，由 `OnReset` 显示。
业务事件在准备界面显示时只处理准备界面自身的操作（浏览 / 打开），编辑界面控件的事件由 `IsEditorActive` 拦下。

---

## Frame 函数

### 语法

```cpp
VOID Frame(VOID* pUser);
```

### 注解

顺序固定：预览推进（先判活动端时码与当前预览时间是否一致：不一致即视为刚刚调整过滑块或时间条，定位到该端并解码一帧；一致时按帧率节流推进并把当前时码写回活动端）→ 重混线程回收 → 进度同步 → 起止时间同步 → `Dispatch::Frame`。数值输入缓冲不在每帧写回，避免覆盖正在编辑的文本。

拖动双手柄滑块或提交时间条时，处理函数只写起止数值；定位解码统一由每帧的预览推进完成（与源项目 AdjustSpanBySlider / AdjustSpanByTimeBar 只改数值、由 DecoderNext 检测差异后定位的写法一致）。

---

## OnRemix 函数

### 注解

把片段按起点排序并合并重叠区间，按微秒构造工作线程所需的片段块，输出路径为 `<源目录>\<分组名>[remix].mp4`。片段块与输出路径归业务侧所有，线程只读；线程结束后由 `JoinRemix` 回收。重混进行中拒绝切换目录、重设与再次重混。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `AppState.h`(`src\app\AppState.h`) |
| 实现 | `AppState.cpp` |
| 命名空间 | 全局作用域（类静态函数） |
| 依赖 | `framework/dispatch/Dispatch.h` · `framework/base/ControlSpec.h` · `framework/base/CtrlBuffer.h` · `framework/base/mem/AllocatorHub.h` · `AppRender.h` · `MediaFileUtils.h` · `MediaOrganizer.h` · `VideoStream.h` |
| 平台 | Windows |
| 线程安全 | 否（UI 线程为主；重混工作线程只读写约定的进度与完成标记） |
