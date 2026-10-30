# AppEvents 模块 (src\app\AppEvents.h)

## 概述

业务事件绑定表。布局文档的 `event` 字段经 `UIEventHandler::SetBindings` 注入到调度层；事件触发时按统一的 `pfnCtrlEvent` 签名调用处理函数，处理函数再转调 `AppState`。

事件名与布局文档必须成对修改：只改一侧会静默失绑（控件无响应）。

### 函数

| 函数 | 说明 |
| --- | --- |
| `GetBindings` | 取事件绑定表首址与条目数。 |

### 事件表

| 事件名 | 处理 | 用途 |
| --- | --- | --- |
| `VIDEOCUT_DRAG_WINDOW` | `OnTitleBarEvent` | 标题栏拖动（与 `IDENTITY_EVENT_DRAG_WINDOW` 同值） |
| `VIDEOCUT_TITLEBAR` | `OnTitleBarEvent` | 标题栏拖动 / 关闭 / 双击 |
| `VIDEOCUT_FILEBROWSE` | `OnBrowseEvent` | 选择源文件夹 |
| `VIDEOCUT_OPENFOLDER` | `OnOpenEvent` | 扫描目录并进入编辑布局 |
| `VIDEOCUT_SELECTVIDEO` | `OnSelectEvent` | 切换视频分组 |
| `VIDEOCUT_APPENDSPAN` | `OnAddEvent` | 添加剪辑片段 |
| `VIDEOCUT_ADJUSTSPAN` | `OnSpanEvent` | 双手柄范围滑块（起点 / 终点） |
| `VIDEOCUT_LISTOPERATION` | `OnListEvent` | 时间片列表行操作（删除 / 选中） |
| `VIDEOCUT_EDIT_BEGIN` | `OnEditBeginEvent` | 起点毫秒提交 |
| `VIDEOCUT_EDIT_END` | `OnEditEndEvent` | 终点毫秒提交 |
| `VIDEOCUT_PLAY` | `OnPlayEvent` | 播放 / 暂停 |
| `VIDEOCUT_SPEED` | `OnSpeedEvent` | 倍速提交 |
| `VIDEOCUT_REMIX` | `OnRemixEvent` | 执行重混 |
| `VIDEOCUT_RESET` | `OnResetEvent` | 重设目录 |

---

## GetBindings 函数

### 语法

```cpp
CONST UIEventHandler::EventBinding* GetBindings(size_t* pnCount);
```

### 参数

`[out] pnCount`

条目数输出；可为 `NULL`。

### 返回值

绑定表首址（函数内静态数组，进程生命周期有效）。

### 注解

绑定表由 `Dispatch::Init` 在场景装配之前注入；处理函数只做参数检查与转调，不保存状态。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `AppEvents.h`(`src\app\AppEvents.h`) |
| 实现 | `AppEvents.cpp` |
| 命名空间 | 全局作用域（类静态函数） |
| 依赖 | `framework/dispatch/UIEventHandler.h` · `AppState.h` · `Identity.h` |
| 平台 | Windows |
| 线程安全 | 是（绑定表只读；实际业务处理在 UI 线程） |
