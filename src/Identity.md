# Identity 模块 (src\Identity.h)

## 概述

本工程的项目身份常量集中登记于此: 布局文档名、运行期环境开关、应用名(窗口身份派生源)、日志前缀、业务事件名都集中在此。
派生新项目时, 代码侧改名只需改本文件; 构建侧身份由派生方工程的目标名与文档复制步骤决定,
派生清单与步骤见 `docs\DERIVE.md`。

本模块是纯头文件, 只含 `constexpr` 常量, 不含函数、类型与运行期状态; 框架层(framework)
不引用本文件, 身份只出现在场景层(经 `DispatchConfig` 注入调度层)与构建定义里。日志前缀(形如 `[VIDEOCUT]`)与
`src\app\` 目录名不属功能身份, 派生时可选改动; 日志前缀已配置化(`IDENTITY_LOG_PREFIX` ⇒ `DispatchConfig.szLogPrefix`),
派生时改这一个常量即可, 不必改调度层实现。

本模块包含以下编程接口:

### 常量

**布局文档身份**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `IDENTITY_DOC_NAME` | `"VideoCut.json"` | 缺省布局文档文件名。运行期按 exe 同目录解释; 加载链 = 同名 `.bin` 优先, 其次该文件本身。构建后由工程把 `VideoCut.json` 复制到产物目录; `.bin` 为可选项。 |
| `IDENTITY_DOC_ENV` | `"VIDEOCUT_DOC"` | 覆盖布局文档文件名的环境变量名。未设置或取值过长 ⇒ 回退 `IDENTITY_DOC_NAME`。 |

**运行期开关**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `IDENTITY_ENV_AUTOCLOSE` | `"VIDEOCUT_AUTOCLOSE_MS"` | 启动后自动退出的毫秒数, 供无人值守验证使用; 未设置 ⇒ 常驻不自动退出。 |

**窗口身份**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `IDENTITY_APP_NAME` | `"VideoCut"` | 应用名。窗口类名 = `<APPNAME>_IMGUI_CLASS`, 窗口标题默认 = 应用名(由 `ImGuiHost::Start` 派生)。窗口类名须在进程外也保持唯一(同名窗口类会与同机其它程序冲突), 派生新项目务必改。 |
| `IDENTITY_ERR_TITLE` | `L"视频拼接剪切辅助编辑器 错误"` | 启动期错误弹窗标题。 |

**日志身份**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `IDENTITY_LOG_PREFIX` | `"VIDEOCUT"` | 调度层日志前缀: 经 `DispatchConfig.szLogPrefix` 交给调度层, 每行打印为 `[VIDEOCUT] `。前缀不属功能身份, 派生新项目时可选改动(改后日志行数不变, 仅前缀文本变)。 |

**事件名**

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `IDENTITY_EVENT_DRAG_WINDOW` | `"VIDEOCUT_DRAG_WINDOW"` | 标题栏按住拖动窗口的业务事件名。事件名是"布局文档 `event` 字段 ↔ 绑定表"的成对契约: 改名须同时改布局文档, 只改一侧即静默失绑。 |

---

## 要求

| 要求 | 值 |
| --- | --- |
| 标头 | `Identity.h`(`src\Identity.h`) |
| 实现 | 无(纯头文件, 常量均为编译期值) |
| 命名空间 | 全局作用域(`constexpr`) |
| 依赖 | `minwindef.h`(`LPCSTR` / `LPCWSTR`) |
| 平台 | Windows 全版本 |
| 线程安全 | 是(编译期常量, 无运行期状态) |
