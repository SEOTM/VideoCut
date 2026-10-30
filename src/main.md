# main 模块 (src\main.cpp)

## 概述

VideoCut 的进程入口。本文件只做胶水: 应用初始化 → 业务状态初始化 → 渲染宿主启动(默认几何建窗 + 渲染设施) →
主循环 → 成对释放。启动失败时弹一条中文错误框并以非 `0` 退出码返回; 成功路径返回 `0`。

双布局模式: 调度层为 `framework\dispatch\Dispatch`(事件绑定注入 / 场景装配 / 投影配色 / 每帧流程 / 退出判据),
渲染宿主为 `framework\render\imgui\ImGuiHost`(窗口类与建窗 / ImGui 与 D3D11 设施 / 消息循环 / 窗口过程),
事件绑定表为 `app\AppEvents::GetBindings`。界面 = 两份布局对象的声明式渲染: 编辑布局是基座(承载唯一窗口),
准备布局是它的内嵌(flex)成员(同一窗口内叠加, 按可见性显隐); 业务状态与每帧推进由 `app\AppState` 承担。

本模块包含以下编程接口:

### 函数

| 函数 | 说明 |
| --- | --- |
| `main` | 进程入口: 初始化应用、启动渲染宿主、运行主循环并按逆序释放。 |

---

## main 函数 (main)

进程入口。

### 语法

```cpp
int main(int, char**);
```

### 参数

`[in] argc`

类型: **int**

命令行参数个数。本工程不解析命令行, 该参数被忽略。

`[in] argv`

类型: **char\*\***

命令行参数数组。本工程不解析命令行, 该参数被忽略。

### 返回值

类型: **int**

`0` = 正常退出; `1` = 应用初始化失败、业务状态初始化失败或渲染宿主启动失败(失败原因见错误框与标准输出)。

### 注解

调用次序固定为八步:

1. `ImGuiHost::Register()` —— 取宿主接口表 `CONST HostApi*`(调度层只经该表访问宿主);
2. 组装 `DispatchConfig` —— 文档名 / 环境变量名 / 日志前缀取自 `Identity.h` 常量, 声明两个标准窗口, 亮暗默认跟随系统;
3. `Dispatch::Init(pHost, cfg, AppEvents::GetBindings)` —— 事件绑定注入、场景载入、投影配色与样式登记、无人值守参数;
4. `AppState::Init()` —— 为框架未分配缓冲的控件补缓冲(路径输入), 初始化业务状态;
5. `ImGuiHost::SetFontSize(Dispatch::GetFontSize())` —— 把布局参考字号交给具体宿主登记(建窗时作为字号);
6. `pHost->Start(Dispatch::GetWndCount(), IDENTITY_APP_NAME, Dispatch::OnSegment, NULL)` ——
   DPI 感知、按默认几何建窗、建立 ImGui 与 D3D11 设施;
7. `ImGuiHost::SetActiveWndCount(1)` + `pHost->Run(AppState::Frame, NULL, Dispatch::GetExitAddress())` ——
   运行期只显示一个窗口; 每帧先推进业务(预览解码 / 重混回收 / 进度同步), 再调调度层处理流程;
8. `pHost->Release()` + `pHost->DestroyWnds()` + `AppState::Release()` + `Dispatch::Release()` ——
   关停渲染设施、销毁窗口与窗口类、释放业务媒体, 再释放投影配色、场景与会话分配器。

第三步失败时弹错误框并返回 `1`(Dispatch 已在 `Init` 内部按阶段回退); 第四步失败时补调 `Dispatch::Release()`;
第六步失败时补调 `AppState::Release()` 与 `Dispatch::Release()`。窗口类名由 `IDENTITY_APP_NAME` 派生
(`<APPNAME>_IMGUI_CLASS`), 窗口标题默认取应用名, 首帧再按布局窗口件的标签更新。

进程的全部运行参数走环境变量, 不走命令行: 布局文档名由 `IDENTITY_DOC_ENV`(`VIDEOCUT_DOC`)覆盖, 自动退出时长由
`IDENTITY_ENV_AUTOCLOSE`(`VIDEOCUT_AUTOCLOSE_MS`)设置。常量定义见 `Identity.h`。

正常退出方式为 `ESC`、系统关闭(`Alt+F4` / 任务栏)、标题栏关闭钮或无人值守到点。

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | 无(进程入口) |
| 实现 | `src\main.cpp` |
| 命名空间 | 全局作用域 |
| 依赖 | `Identity.h` · `app/AppEvents.h` · `app/AppState.h` · `framework/dispatch/Dispatch.h` · `framework/render/imgui/ImGuiHost.h` · `framework/render/Render.h` · `user32.dll` |
| 平台 | Windows(控制台子系统; 启动首件事为控制台代码页切 UTF-8) |
| 线程安全 | 不适用(进程入口, 单线程启动) |
