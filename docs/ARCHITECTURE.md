# 架构

本文说明 Kanat 的分层、数据流、渲染抽象、事件模型与扩展点。逐模块的接口契约以 `src/` 下同名 `.md` 为准；本文只描述模块之间的关系与设计取舍。

## 1. 定位与边界

Kanat 负责四件事：

1. 把布局文档解析为内存中的布局模型；
2. 把布局模型展开为布局对象，解算像素几何、控件层次与有效坐标；
3. 按能力表渲染布局与控件；
4. 把控件交互以事件回调交回应用。

Kanat 不负责：

- 应用业务状态与数据持久化；
- 编辑界面（增删控件、改类型、写回文档）；
- 文件对话框、多文档协调；
- 具体 UI 技术的窗口过程与图形设施（这两项由宿主实现承担）。

框架层不包含应用身份。应用名、文档名、环境变量名、日志前缀与事件名集中在 `src/Identity.h`；构建侧的项目名与文档复制由派生方的工程决定。

## 2. 分层与依赖方向

```text
demo（应用事件）
   │  绑定表
   ▼
dispatch（调度、场景装配、事件）
   │
   ├──────────────► layout（模型 / 解析 / 几何 / 布局对象 / 渲染出口）
   │                     │
   │                     ▼
   └──────────────► render（能力接口；imgui 实现、控件库、宿主）
                         │
                         ▼
                       base（设计量 / 控件契约 / 缓冲 / 文本 / 内存 / 校验）
```

依赖方向：`base` 不依赖其他层；`layout` 依赖 `base` 与 `render/Render.h` 的接口契约；`render` 的实现依赖 `base`，宿主在装配期调用 `LayoutRender` 注入能力表（这是唯一的回向编排依赖，不形成头文件环）；`dispatch` 依赖 `layout`、`render/Render.h` 与 `base`；`demo` 只依赖 `dispatch`、`base` 与 `Identity.h`。`main.cpp` 只依赖 `dispatch`、宿主实现与 `demo`。

| 目录 | 内容 | 关键约束 |
| :--- | :--- | :--- |
| `base/` | `BasicDesign`（参考量、投影配色、屏幕参考）、`ControlSpec`（控件类型、操作码、父链契约）、`CtrlBuffer`、`TextWidth`、`AppEnv` | 不依赖具体 UI 库；不持有窗口 |
| `base/mem/` | `MemoryManager`（`Arena` / `MemPool`）、`AllocatorHub`（会话分配器） | 不使用 `new` / `delete`；不提供隐式线程安全 |
| `base/util/` | `FileCodec`（CRC32） | 纯字节算法 |
| `layout/` | `LayoutModel`、`LayoutSolver`、`LayoutBuilder`、`GeometrySolver`、`Layout`、`LayoutRender` | 不包含 ImGui 类型；渲染只经能力表 |
| `render/` | `Render.h`：`RenderApi`、`CtrlApi`、`HostApi` 与状态位 | 只有 `render/imgui/` 包含 ImGui |
| `render/imgui/` | `ImGuiHost`（窗口 / 上下文 / 帧循环）、`D3D11Manager`（设备 / 交换链）、`ability/ImGuiRender`（能力表填充）、`control/`（控件与部件） | 宿主独占窗口过程与图形设施 |
| `dispatch/` | `Dispatch`、`DispatchScene`、`FrameEvent`、`UIEventHandler`、`ResourceLoader` | 不持有窗口句柄；只经 `HostApi` 访问宿主 |

## 3. 启动与数据流

`main.cpp` 的固定顺序：

1. `ImGuiHost::Register()` 取宿主接口表 `HostApi`；
2. 填 `DispatchConfig`（文档名、环境变量名、日志前缀、窗口数、亮暗策略）；
3. `Dispatch::Init`：控制台 UTF-8 → DPI 感知 → 注入事件绑定表 → 场景装配 → 登记宿主样式 → 读取无人值守参数；
4. `ImGuiHost::SetFontSize(Dispatch::GetFontSize())` 登记建面字号；
5. `HostApi::Start` 建窗并建立 D3D11 / ImGui 设施；
6. `HostApi::Run` 进入消息循环，每帧调用一次 `Dispatch::Frame`；
7. 退出后依次 `Release`、`DestroyWnds`、`Dispatch::Release`。

场景装配（`DispatchScene.cpp`）：

```text
文档名 ──► 同名 .bin 存在？ ──是──► 读 bin
                              │
                              否
                              ▼
                         读同名 .json
                              │
                              ▼
              LayoutSolver ──► FileMidState（布局链 + 元素规格）
                              │
                              ▼
              LayoutBuilder ──► Layout + CtrlInfo + 控件缓冲
                              │
                              ▼
             GeometrySolver ──► 像素几何 / 窗口位置 / 层次 / 有效坐标
```

缺省加载链是先 `.bin` 后 `.json`，形态由内容嗅探判定。`Dispatch` 也接受内存缓冲、已有布局模型、已有布局对象数组与延迟装配四种来源；延迟装配下业务代码在 `Init` 之后用 `AttachLayouts` / `AddLayout` 提交布局，并用 `SetLayoutVisible` / `SetLayoutNested` 控制显示与嵌套。

## 4. 布局对象

`Layout` 是一份布局的控件容器，有两种存储模式：

- **线性模式**：控件信息放在一块连续数组中，按索引访问；
- **链式模式**：控件以双向链表存储，支持中间插入与删除。

模式在 `Layout::CreateLayout(num, IsLink)` 时确定。布局对象、控件数组与链视图由注入的 `Arena` 分配；链节点来自 `MemPool`。布局对象不在构造时持有窗口或渲染资源。

父链是三种推导的共同输入：

- **层次**：`CtrlInfo::nLayer` 由 `Layout::DeriveCtrlLayers` 按 `parentID` 链递推；窗口非客户区控件恒为 0；域类控件逐层下沉，按钮与复选框逐层上浮，其余继承父层。
- **坐标**：内容控件的有效坐标 = 声明坐标 + 父链上各控件的声明坐标之和；`Layout::DeriveCtrlPositions` 可反复重推，因为 `posDeclared` 在几何解算后不再改写。
- **事件**：`CTRL_EVENT_ENTRUST_PARENT` 让子控件的事件绑定到父容器（按 `parentID` 上溯）的处理函数。

## 5. 几何模型

每个控件的 `x` / `y` / `width` / `height` 是一个 `SizeFactor`：8 个 `float` 系数加 1 个整数像素偏移。

| 序号 | 系数 | 参考量 |
| ---: | :--- | :--- |
| 0 | `screenWidth` | 主显示器宽度 |
| 1 | `screenHeight` | 主显示器高度 |
| 2 | `windowWidth` | 窗口客户区宽度 |
| 3 | `windowHeight` | 窗口客户区高度 |
| 4 | `controlHeight` | 控件高度基准 |
| 5 | `fontSize` | 字号 |
| 6 | `space` | 控件间隔 |
| 7 | `padding` | 内边距 |

解算式：

```text
像素 = coeff[0] × screenWidth  + coeff[1] × screenHeight
     + coeff[2] × windowWidth  + coeff[3] × windowHeight
     + coeff[4] × controlHeight + coeff[5] × fontSize
     + coeff[6] × space        + coeff[7] × padding
     + offset
```

布局级 `autosize` 决定参考量本身如何由屏幕或窗口推导：

- `mode`：`AUTO_SIZE_BY_SCREEN_WIDTH` / `..._SCREEN_HEIGHT` / `..._WINDOW_WIDTH` / `..._WINDOW_HEIGHT`；
- `unit`：`0` = 千分比，`1` = 像素；
- `width` / `height` / `controlHeight` / `fontSize` / `space` / `padding`：参考值。

窗口尺寸由布局的 `width` / `height` 因子相对参考量解算。窗口控件（`CTRL_TYPE_WND`）的 `position` 是**窗口位置记录**：解析期按声明值、屏内判断与越屏回退确定初值，运行期由标题栏事件改写；渲染侧在取得该窗口的渲染目标时以记录为参数结算几何。

## 6. 渲染抽象

`Render.h` 定义三张函数指针表：

| 表 | 项数 | 提供方 | 消费方 | 用途 |
| :--- | ---: | :--- | :--- | :--- |
| `RenderApi` | 16 | `ImGuiRender` | `LayoutRender` | 窗口 / 内容容器 / 光标 / ID 命名空间 / 命中判定 / 绘制量 |
| `CtrlApi` | 3 | `Controls` | `LayoutRender` | 控件上下文、逐控件绘制、编辑中输入查询 |
| `HostApi` | 13 | `ImGuiHost` | `Dispatch` | 窗口生命周期、主循环、样式、可见性、窗口矩形、自证统计 |

`ImGuiHost::Acquire` 建立每个窗口的 D3D11 与 ImGui 面，并把 `RenderApi` 与 `CtrlApi` 注入 `LayoutRender`。因此：

- `LayoutRender` 不包含任何 ImGui 头，也不出现窗口句柄；
- 更换 UI 技术 = 提供另一份 `RenderApi` + `CtrlApi` 实现，并由新宿主注入；
- 布局、控件契约与文档格式不变。

`RenderApi` 的能力顺序是典型调用顺序：`AcquireWnd` → `OpenWnd` / `OpenPanel` → `SetCursor` → 绘制控件 → `ClosePanel` / `CloseWnd`。窗口状态位 `RENDER_WND_BASE` 与 `RENDER_WND_NOINPUT` 由实现方翻译为目标 UI 库的窗口标志。

## 7. 控件与部件

控件库分两层：

- **控件层**（`Controls`）：把一枚 `CtrlInfo` 按类型分派为具体控件，负责矩形定位、事件回传与控件级语义；
- **部件层**（`Widgets`）：可复用的绘制单元，如边框、文本、按钮面、滑块、列表行。

控件层逐枚无状态被调用，不持有控件数据，不分配控件对象，不遍历布局。控件缓冲的装配与回收在 `CtrlBuffer` 与 `LayoutBuilder`，不在绘制路径。

控件类型共 24 种公开类型（另有 `CTRL_TYPE_UNKNOWN` 与 `CTRL_TYPE_INVALID` 两个哨兵），其中 23 种有绘制实现；`CTRL_TYPE_LIST` 当前只登记类型身份，既无缓冲也无像素。完整清单、绘制语义与事件回传见 `src/framework/render/imgui/control/Controls.md`。

## 8. 事件模型

三段式：

```text
布局文档 event 字段
        │  装配期解析
        ▼
UIEventHandler 绑定表（应用侧提供）
        │  精确名或“前缀 + 一位数字”家族
        ▼
CtrlInfo::pEventHandler
        │  控件交互时调用
        ▼
应用处理函数：void Handler(void* obj, uint64_t oper, uint64_t param, void* data)
```

- 未命中绑定表的事件名解析为空绑定，控件无响应，不报错；
- `CTRL_EVENT_ENTRUST_PARENT` 不是绑定表中的名字，而是“把本控件的事件绑定到父容器处理函数”的保留值；
- `oper` 是 64 位操作码，分为公共段、控件专属段与搭载段，定义在 `ControlSpec.h` 的 `OperCode`；
- `param` 是通用参数槽，具体语义由控件类型约定，例如标题栏用 `PackWndPos` / `UnpackWndPos` 传窗口期望坐标。

`Dispatch::Init` 通过函数指针取得绑定表，`UIEventHandler` 只认 `pfnCtrlEvent` 契约，不引用任何应用模块，因此框架不反向依赖应用层。

## 9. 内存模型

- `MemoryManager::Arena`：线性分配器，按指针推进分配，只能整体 `Reset`；
- `MemoryManager::MemPool`：分级固定块池，支持任意顺序单独释放；
- `AllocatorHub`：进程级会话分配器的持有处，小对象优先走池，超限或失败回退线性分配器。

调度层把会话状态放在一块由 `Arena` 分配的 `DispatchData` 中，用 placement new 构造，`Release` 时显式析构，再整体释放分配器。布局对象、控件数组、控件缓冲、文档中间态都从会话分配器取内存；因此一次场景拆卸不需要逐对象归还系统堆。

内存设施不提供隐式线程安全；所有分配与释放都应在 UI 线程完成。

## 10. 每帧流程

`Dispatch::Frame` 每帧执行：

1. 首帧打印建窗几何（`Start` 的默认几何；文档几何在首帧渲染时结算）；
2. 刷新投影配色（跟随系统或调用方登记的亮暗值）；
3. 按布局槽顺序渲染：基座布局走 `LayoutRender::RenderBase`，嵌套布局走 `LayoutRender::RenderFlex`；
4. 缓存窗口显示状态；
5. 把“是否存在编辑中的输入控件”交给宿主，决定 IME 开关；
6. 按节流周期打印渲染自证（段数、绘制命令、顶点、索引、窗口调整次数、控件数、可见窗口数）；
7. 检查自动退出时限。

主消息循环、窗口过程、D3D11 呈现与 ImGui 帧边界都在 `ImGuiHost::Run` 内，调度层只提供每帧流程与退出判据地址。

## 11. 扩展点

### 新增控件类型

1. 在 `ControlSpec.h` 的 `CTRL_TYPE_TABLE` 增加枚举名与字符串名；
2. 在 `CtrlLayerStep` 决定层次步进（下沉 / 平坦 / 上浮）；
3. 在 `Controls` 的分派表增加绘制实现；
4. 如需控件缓冲，在 `CtrlBuffer.h` 定义布局并在 `LayoutBuilder` 装配；
5. 更新 `Controls.md` 与 `LAYOUT-FORMAT.md`。

### 替换渲染后端

1. 实现 `RenderApi` 的 16 项能力；
2. 实现一个宿主，填充 `HostApi` 的 13 项，并负责窗口、消息循环与图形设施；
3. 提供 `CtrlApi`（可复用现有 `Controls`，也可另写控件库）；
4. 在宿主初始化期把两张表注入 `LayoutRender`。

### 新增事件

1. 在 `Identity.h` 增加事件名常量（或直接使用文档字符串）；
2. 在应用绑定表增加 `{ 事件名, 处理函数, '\0', '\0' }`；
3. 在布局文档对应控件的 `event` 字段填入同一字符串。

### 新增布局来源

在 `LayoutSource` 增加工厂函数与判别值，并在 `DispatchScene.cpp` 的装配分支处理新来源。已有五种来源：缺省加载链、内存缓冲、布局模型、布局对象数组、延迟装配。

## 12. 约束与取舍

- **单线程**：框架不提供锁；所有接口在 UI 线程调用。
- **无异常**：代码不使用 C++ 异常、RTTI 与 STL 容器；错误以返回值与日志上报。
- **无抽象基类**：跨层解耦使用函数指针表，不引入虚函数。
- **Windows 专有**：直接使用 Win32、D3D11、DXGI、DWM 与 IMM32。
- **接口文档优先**：代码只保留“函数 / 描述”两行注释；参数、返回值、失败口径与副作用写在同目录接口文档中。
- **Demo 不是产品**：`src/demo` 只演示事件绑定与布局声明，不包含编辑器、持久化或多文档能力。
