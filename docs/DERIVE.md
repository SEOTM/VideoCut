# 派生新应用

Kanat 是“框架 + 最小 Demo”的源码包。派生新应用时，把 `src/` 与 `docs/` 复制出去，替换应用身份与业务事件即可；框架层不需要改动，构建工程由派生方按 [BUILD.md](BUILD.md) 自建。

## 1. 复制与改名

```powershell
New-Item -ItemType Directory -Force MyApp | Out-Null
Copy-Item -Recurse Kanat\src MyApp\src
Copy-Item -Recurse Kanat\docs MyApp\docs
Copy-Item Kanat\README.md MyApp\README.md
```

在新目录初始化自己的 Git 仓库：

```powershell
cd MyApp
git init
```

## 2. 必改项

| 项 | 位置 | 现值 | 说明 |
| :--- | :--- | :--- | :--- |
| 项目名 / 目标名 | 你自己工程的可执行文件名 | `Kanat` | 决定 exe 名称；与框架代码无关，按产品命名即可 |
| 布局文档名 | `src/Identity.h` 的 `IDENTITY_DOC_NAME` + 实际 JSON 文件名 + 构建后复制到 exe 同目录的文件名 | `Kanat.json` | 三处必须一致；运行期按该名查找 |
| 应用名 | `src/Identity.h` 的 `IDENTITY_APP_NAME` | `Kanat` | 派生窗口类名 `<APPNAME>_IMGUI_CLASS` 与默认窗口标题；窗口类名须进程外唯一 |
| 错误弹窗标题 | `IDENTITY_ERR_TITLE` | `暗渠框架 Kanat 错误` | 启动失败时显示 |
| 文档覆盖环境变量 | `IDENTITY_DOC_ENV` | `DEMO_DOC` | 与同机其他工具的环境变量约定冲突时改 |
| 自动退出环境变量 | `IDENTITY_ENV_AUTOCLOSE` | `DEMO_AUTOCLOSE_MS` | 同上 |
| 日志前缀 | `IDENTITY_LOG_PREFIX` | `DEMO` | 日志行首 `[DEMO]` |
| 拖动窗口事件名 | `IDENTITY_EVENT_DRAG_WINDOW` + 布局文档 `event` | `DEMO_DRAG_WINDOW` | 只改一侧会静默失绑 |
| 布局名 / 标签 | 布局文档的 `layouts[0].name` / `label` | `DemoMain` / `暗渠框架 Demo` | 只影响日志与标识 |
| 业务事件表 | `src/demo/DemoEvents.cpp` | 一条演示事件(拖动窗口) | 换成自己的处理函数 |

`src/Identity.h` 共 7 个常量：3 个文档 / 运行开关，3 个应用与日志身份，1 个事件名。

## 3. 建议改动

- 把 `src/demo/` 改名为业务名（如 `src/app/`），并同步 `#include` 与 `Dispatch::Init` 传入的绑定表函数；
- 把 `Kanat.json` 改名为 `App.json`，重写布局与控件；改名时同步 `Identity.h` 的 `IDENTITY_DOC_NAME` 与构建后的文档复制步骤；
- 更新 `README.md` 与 `docs/` 中的项目名、示例路径；
- 事件绑定表只保留需要的业务事件;删除事件时,布局文档的 `event` 与绑定表条目成对移除,避免静默失绑;
- 如果产品只需要单窗口，保留 `DispatchConfig::dwWndCount = 1`；多窗口时按 `ImGuiHost::MAX_WND`（8）声明。

## 4. 派生后自证

1. **构建**：Visual Studio 或 `msbuild` 生成，零警告、零错误；
2. **启动**：在 exe 目录运行，退出码 0；
3. **文档**：控制台“场景载入”行的路径与新文档名一致；
4. **控件**：日志中的控件数、层次与坐标符合预期；
5. **事件**：点击 / 拖动绑定了事件的控件，处理函数被调用；
6. **交互与外观**：拖动、悬停、输入法需本机人工复核。

启动日志示例：

```text
[DEMO] 场景载入: ...\Kanat.json (布局=DemoMain 控件=3 窗口=...)
[DEMO] 事件绑定: 1 枚控件已绑定处理器(其中委托父容器 0 枚, 已解析 0 枚)
[DEMO] 控件层次: 0=0 1=0 2=0
[DEMO] 控件坐标: 0=(180,120) 1=(0,0) 2=(0,54) ...
```

## 5. 不要改的部分

- `src/framework/` 的分层与路径：派生时整体带走；
- 依赖版本与包含/链接配置：按 [BUILD.md](BUILD.md) 保持不变，除非升级 ImGui 或 RapidJSON；
- 28 份模块接口文档：路径与模块名按 `src/` 相对路径书写，改名工程不需要逐份修改。

## 6. 常见错误

| 现象 | 原因 |
| :--- | :--- |
| 启动弹窗“应用初始化失败” | 实际 JSON 文件名、`IDENTITY_DOC_NAME`、复制到 exe 目录的文件名不一致，或文档不在 exe 同目录 |
| 控件点击无反应 | 布局 `event` 与绑定表事件名不一致；检查大小写与拼写 |
| 拖动窗口无反应 | 标题栏 `event` 未绑定 `IDENTITY_EVENT_DRAG_WINDOW`，或布局中没有 `CTRL_TYPE_WND` |
| 窗口类注册失败 | `IDENTITY_APP_NAME` 与同机其他程序重名；窗口类名须进程外唯一 |
| 日志前缀仍是 `[DEMO]` | 只改了 `Identity.h` 但未重新构建，或改的是错误副本 |
