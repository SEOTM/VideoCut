# VideoCut 迁移报告（Kanat 派生）

## 1. 派生基线

| 项 | 值 |
| --- | --- |
| 来源仓库 | 工作区 `Kanat`（分支 `main`，提交 `7699caa`） |
| 派生目录 | `VideoCut-Kanat` |
| 派生方式 | 复制 Kanat 源码树（`src/`、`docs/`、`third_party/`、`tools/`、构建工程），移除 Kanat 的 Git 元数据与 `out/`，重新初始化独立仓库 |
| 旧 VideoCut | `G:\Projects\CPP\VideoCut` 与工作区 `VideoCut/` 作参考源，不修改、不作为派生基础 |

框架层 `src/framework/` 的改动均来自用户 2026-10-24 的明确同意（G1 / G2 / G3 / G4 / G7），G5 / G6 暂缓。

**当前基线（2026-10-25 起）**：分支 `main`、提交 `980065c`（第 8 次提交「准备界面改为编辑界面内嵌浮层布局」）。该提交含用户亲手完成的标题栏 / 关闭钮绘制微调，随后以 amend 合入同一提交（见 §5「2026-10-25 人工修改（第六轮）」）。后续会话必须以此提交为起点继续开发，不得回退到更早提交，也不得用旧版文件或 AI 重新生成的版本覆盖该提交中的实现。

## 2. 迁移范围

### 2.1 已迁移

| 旧文件 | 新位置 | 处理 |
| --- | --- | --- |
| `VideoEncoder.cpp/h` | `src/app/` | 原样搬运；仅有 FFmpeg 依赖 |
| `StreamPipe.hpp` | `src/app/` | 原样搬运 |
| `MediaFileUtils.cpp/h` | `src/app/` | 搬运；删除未使用的 `Application.h` 引用 |
| `MediaOrganizer.cpp/h` | `src/app/` | 搬运；`SecureBuffer` 改为 `AppBuffer` |
| `VideoStream.cpp/h` | `src/app/` | 搬运；设备由 `AppRender` 提供，纹理与解码共用宿主设备 |
| `LayoutConfig.json` | `VideoCut.json` | 改写为 Kanat 布局格式：两份布局 + `autosize` + 原控件语义 |
| `resource.h` / `Resource.rc` / `AppIcon.ico` | `src/app/`、根 | 资源脚本移到根并编译；窗口图标经宿主能力表生效 |
| `vcpkg.json` | 根 | manifest 模式，构建时自动安装 FFmpeg |

### 2.2 由框架替代

| 旧框架件 | Kanat 对应 | 说明 |
| --- | --- | --- |
| `BasicDesign` / `MatchCR` | `framework/base/BasicDesign` / `ProjCR` | 配色与参考尺寸由框架内建 |
| `Layout` / `LayoutSolver` | `framework/layout/*` | 布局模型、解析、几何、布局对象 |
| `Controls` | `framework/render/imgui/control/*` | 控件绘制与部件层 |
| `D3D11Manager` | `framework/render/imgui/D3D11Manager` | 渲染设施由宿主持有 |
| `SecureBuffer` | `framework/base/mem/MemoryManager` + `AppBuffer` | 框架小对象池 + 应用大块池 |
| `Application` | `main` + `Dispatch` + `AppState` | 装配、切换、业务状态 |
| `UIEventHandler` | `app/AppEvents` | 事件模型重写为操作码四参 |
| `main`（手写 Win32/ImGui/D3D11） | Kanat `main` + `ImGuiHost` | 消息循环、建窗、渲染设施由宿主承担 |

### 2.3 未迁移（暂缓项）

| 旧功能 | 状态 | 说明 |
| --- | --- | --- |
| 双击最大化 | 暂缓（G5） | 运行期参考尺寸重算未提供；双击只记日志 |
| 拖边调整窗口 | 暂缓（G6） | 宿主窗口为 `WS_POPUP`，无调整边框与 hit test |
| 通用 `CTRL_TYPE_LIST` | 未实现 | 业务改用 `LIST_TIMESPAN`，通用列表仍无绘制 |

## 3. 控件映射

| 旧控件 | 新控件 | 缓冲 | 说明 |
| --- | --- | --- | --- |
| `CTRL_TYPE_WND` | 同名 | 无 | 标签为窗口标题文本 |
| `CTRL_TYPE_TITLE` | 同名 | 文本 | 拖动 / 关闭由操作码回传 |
| `CTRL_TYPE_CNTR` | 同名 | 无 | 内容容器 |
| `CTRL_TYPE_TEXT` | 同名 | 文本 | 原样 |
| `CTRL_TYPE_INPUT_SINGLE` | 同名 | 文本（256） | 路径缓冲由业务替换为 `MAX_PATH * 3` |
| `CTRL_TYPE_BUTTON` | 同名 | 文本 | 原样 |
| `CTRL_TYPE_SLIDER`（双手柄） | `CTRL_TYPE_SLIDER_RANGE` | `CtrlSliderRangeData` | 新增控件，左右手柄分别回传 |
| `CTRL_TYPE_TIMEBAR`（可编辑时码） | 同名 | `CtrlTimeBarData` | 含义订正：时间条即可编辑时码条，绘制仿旧项目；不再另设类型 |
| 旧时间片列表 | `CTRL_TYPE_LIST_TIMESPAN` | `ListBuffer` + `TimeSpanItem` | 新增实现：行显示 + 删除钮 + 行选中 + 滚轮 |
| `CTRL_TYPE_INPUT_REAL` | 同名 | `DOUBLE` | 倍速 |
| `CTRL_TYPE_COMBO` | 同名 | `CtrlComboData` | 视频分组 |
| `CTRL_TYPE_PROG` | 同名 | `CtrlProgData` | 重混进度 |
| `CTRL_TYPE_RENDER` | 同名 | `CtrlRenderData` | 纹理来自宿主持有设备 |

## 4. 事件映射

| 旧事件 | 新事件 | 处理 |
| --- | --- | --- |
| `TITLEBAR_EVENT` | `VIDEOCUT_TITLEBAR` | 拖动 / 关闭；双击记日志（G5） |
| `FILEBROWSE` | `VIDEOCUT_FILEBROWSE` | 文件夹选择 |
| `OPENFOLDER` | `VIDEOCUT_OPENFOLDER` | 扫描目录并进入编辑布局 |
| `APPENDSPAN` | `VIDEOCUT_APPENDSPAN` | 写入时间片列表 |
| `REMIXCLIPS` | `VIDEOCUT_REMIX` | 启动重混工作线程 |
| `ADJUSTSPANBYSLIDER` | `VIDEOCUT_ADJUSTSPAN` | 双手柄范围滑块，按专属段区分左右 |
| `ADJUSTSPANBYTIMEBARL` | `VIDEOCUT_EDIT_BEGIN` | 起点时码提交 |
| `ADJUSTSPANBYTIMEBARR` | `VIDEOCUT_EDIT_END` | 终点时码提交 |
| `LISTOPERATION` | `VIDEOCUT_LISTOPERATION` | 行删除（行件操作码经 payload 段回传） |
| `SETPLAYSTATE` | `VIDEOCUT_PLAY` | 播放 / 暂停 |
| `SELECTVIDEO` | `VIDEOCUT_SELECTVIDEO` | 切换视频分组 |
| `CHECKSPEED` | `VIDEOCUT_SPEED` | 倍速钳制并应用 |
| `RESETFOLDER` | `VIDEOCUT_RESET` | 释放媒体并回到准备布局 |
| `DECODERNEXT` / `CHECKSTATE` | 无 | 由 `AppState::Frame` 每帧推进 |

### 4.1 时间轴与预览联动（业务段）

`DECODERNEXT` 的联动职责由 `AppState` 每帧的预览推进承接，写法与源项目一致：

- 拖动双手柄滑块（`VIDEOCUT_ADJUSTSPAN`）与提交时间条（`VIDEOCUT_EDIT_BEGIN` / `VIDEOCUT_EDIT_END`）只写起止毫秒并记录活动端，不在事件内定位解码；
- 每帧先比较活动端时码与当前预览时间：不一致即视为用户刚调整过时间轴，调用 `SetCurrentTick` 定位并解码一帧（等价于源项目 `DecoderNext` 的跳转分支）；
- 播放推进时把当前时码写回活动端，滑块手柄与对应时间条随播放移动（等价于源项目播放分支的滑块值与时间条回写）；
- 活动端标记随最近一次拖动或时间条提交切换，打开视频时复位到起点（等价于源项目 `RefreshUI` 的 `slider.oper = 0`）；
- 该逻辑全部位于 `src/app/`，未改动 `src/framework/` 与 `VideoStream` 解码器。

## 5. 框架改动（已获同意）

### G1 渲染设备访问（已实现）

`HostApi` 增加 `GetNativeDevice` / `GetNativeDeviceContext`；`ImGuiHost` 暴露首个标准窗口的 D3D11 设备与上下文；业务 `AppRender` 经能力表取设备，`VideoStream` 在设备就绪后创建纹理并上传帧。

### G2 LIST_TIMESPAN（已实现）

- 缓冲：`ListBuffer` + `LIST_TIMESPAN_CAPACITY`（128）行 `TimeSpanItem`；`ListBuffer` 增加 `dwTop` 支持滚动；
- 绘制：逐行显示 `HH:MM:SS.mmm - HH:MM:SS.mmm`，行尾删除钮，行点击选中（选中行高亮），滚轮翻动；2026-10-24 晚行件 `TimeSpanRow` 几乎重建（全自绘三层、文本删除钮、抬起触发），见下文「2026-10-24 人工订正（第三轮）」；
- 回传：行件操作码经 `ListCollectRowOper` 折算（payload 低 16 位为行操作），选中变更由控件写回 `dwSelected` 并派发 `OPER_LIST_ROW_SELECTION_CHANGED`。

### G3 TIMEBAR（已订正为可编辑）

旧项目的 `CTRL_TYPE_TIMEBAR` 本身就是可编辑时码条：提示文本 + 圆角底条 + 时/分/秒/毫秒四段数字输入。
派生初期曾把 Kanat 上游的只读 `TIMEBAR` 当成时间条原义，另造 `CTRL_TYPE_TIMEBAR_EDIT` 承载编辑；
2026-10-24 按用户口径订正含义后，`CTRL_TYPE_TIMEBAR` 恢复为可编辑时间条（部件 `Widgets::Base::TimeBar`），
`CTRL_TYPE_TIMEBAR_EDIT` 撤销，布局与业务统一改用 `CTRL_TYPE_TIMEBAR`。

缓冲 `CtrlTimeBarData`（提示文本 + 当前毫秒 + 上限毫秒）；控件按 2:2:2:3 宽度排四个数字输入，任一段变更
（含悬停滚轮微调）即合成毫秒写回缓冲并派发 `MakeCodeOwn(OPER_EDIT_SUBMIT)`；业务读取缓冲的 `valueMs`，
并把起止毫秒与上限写回同一缓冲。

### G4 SLIDER_RANGE（已实现）

缓冲 `CtrlSliderRangeData`（`vMin` / `vMax` / `lMin` / `lMax`）；`Widgets::Base::RangeSlider` 自绘轨道、选中区间与两个手柄；拖动时按最近手柄选中，左右手柄分别回传 `OPER_SLIDER_THUMB_LEFT` / `OPER_SLIDER_THUMB_RIGHT`，并带公共段 `OPER_EDIT_CHANGED`。

### G7 窗口图标（已实现）

`HostApi` 增加 `SetAppIcon`；`ImGuiHost` 在注册窗口类与建窗时使用登记的 `HICON`，并发送 `WM_SETICON`；`main` 从 `IDI_ICON1` 加载图标后登记。

### G5 / G6（暂缓）

| 缺口 | 现状 | 后续建议 |
| --- | --- | --- |
| G5 运行期布局重算 / 最大化 | `AutoSize` 在装配期固定，双击只记日志 | `Dispatch` 增参考尺寸更新与几何重算入口 |
| G6 拖边调整窗口 | 窗口为 `WS_POPUP`，无 hit test | 按 `IsResizeable` 加边框与 `WM_NCHITTEST` 处理 |

### 2026-10-24 人工订正（第三轮）

本节记录用户 2026-10-24 晚在本机完成的一轮界面人工订正。提交 `f7d7ab2` 由 AI 创作（创作时间 16:11）、用户亲手提交（23:27）；相对上一版 `7a9f9e4` 的净改动限于布局参数、时间片行件与时间条绘制，不改变控件契约与事件模型。

- 布局 `VideoCut.json`：`autosize.fontSize` 由 17 改为 13；补 `wndX` / `wndY`（准备 866 / 691，编辑 94 / 86）；右侧栏整体由 11.5 个字号单位移到 13.5（`coeff[5]` 系数），时间条 / 列表 / 预览宽度同步；标题栏 `x` / `y` 归零；控件 `parent` 字段改为紧随 `id`（只影响书写顺序，解析语义不变）。
- 时间片行件 `TimeSpanRow`：几乎重建（结构、绘制与交互判定均重写，不再复用 `Widgets::Base::Text` / `ExitSign`）。新结构先按行矩形算出 `base` / `hint` / `btn` 三块（`Geo` 联合体），再用 `ChannelsSplit(3)` 分层：底层画行底与删除钮底，中层画文本，顶层画边框；行激活与删除各用一枚隐式按钮承载，判定由按下触发（`IsItemClicked`）改为抬起触发（`IsItemDeactivated() && IsItemHovered()`）；删除钮由叉线改为文本 `-`；两个按钮的 hover / active / disable 状态分别合并后取色。列表 `ListTimeSpan` 同步调整：行距改为内边距 + 行高，可见行数按 `(size.y - 内边距) / 行高` 计算，行宽不再为滚动条让位。
- 时间条 `TimeBar`：四段输入的纵坐标由屏幕坐标 `boot.y` 改为窗口本地坐标 `pos.y`；段间分隔符按字号垂直居中；输入文字固定取基准前景色，不再随禁用态变灰。

本轮遗留（未改代码，记录待后续处理）：

- 四段输入的横坐标仍取屏幕绝对坐标 `barBoot.x`，而 `TimeBarSegment` 内部走 `ImGui::SetCursorPos`（窗口本地坐标）；窗口不在屏幕原点时四段输入会整体右偏。修正方向：段定位自 `pos.x + 提示文本宽 + 内边距` 起算，绘制继续使用屏幕坐标。
- `TimeSpanRow` 重建后每行先铺一层不透明行底（`crBackground.crBase`，alpha 为 `0xFF`），且该层绘制在 `ListTimeSpan` 的选中高亮矩形之后，会把选中行的高亮整块盖住；选中态目前仍写回 `dwSelected` 并派发事件，但没有可见高亮。修正方向：把选中态并入行底取色（或让选中行跳过行底），也可把高亮改到行件内部按 `dwSelected` 绘制。
- `TimeSpanRow` 内 `crAlpha` 为未使用局部变量。
- `ListTimeSpan` 在列表高度不是行距整倍数时，最后一行可能越过列表可视区被裁剪（溢出量取决于高度余数）。

本轮未重新执行构建与运行验证；以上为代码审阅结论，视觉细节仍待本地人工复核。

### 2026-10-25 业务段调整（第五轮）：准备界面改为编辑界面的内嵌(flex)布局

用户口径：准备界面不再作为独立的 Base 布局，而是编辑界面的 Flex 布局。按 Kanat 的槽位语义落地如下：

- 布局文档 `VideoCut.json`：槽序调整为「编辑界面（`Editor`，基座）」在前、「准备界面（`Prepare`，内嵌成员）」在后；
  两枚布局的中文标签、`autosize`、控件与事件绑定均未改动。首枚布局决定窗口几何与建面字号，两枚布局的
  `autosize` 相同，故字号不变；窗口初始几何改由编辑布局决定。
- 行为变化：准备浮层不再启动即显示（默认停留编辑界面，由「重设目录」的 `OnReset` 打开），且进入 / 退出
  准备界面不再调整窗口几何（此前两枚基座布局各自结算窗口大小与位置，切换时会跳变）；窗口始终按编辑布局的
  几何呈现，准备界面只是窗口内的一层浮层。
- 业务侧 `AppState`：启动时 `Dispatch::SetLayoutNested(LAYOUT_PREPARE, TRUE)` 把准备布局登记为内嵌(flex)成员，
  并置为不可见；「重设目录」（`##RESET`）的 `OnReset` 显示准备浮层，打开目录成功后隐藏。
  `SwitchLayout` 只切换准备布局的可见性，编辑布局（基座）恒可见 —— 内嵌成员必须沿用基座已开启的窗口段。
  准备界面显示时（`dwLayout == LAYOUT_PREPARE`）编辑界面的业务事件由 `IsEditorActive` 拦下，保持旧的
  「一次只操作一个界面」语义。
- 标题栏拖动：准备与编辑同窗后不能再看 `dwLayout`；`OnTitleBar` 改由触发控件反查所属布局槽。内嵌布局不采用
  标题栏回传的 `param`（其基准是宿主窗口位置，与面板坐标原点不一致，按下首帧会把面板甩到错误位置），
  改由鼠标客户区坐标与按下帧记录的抓取偏移结算，抬起时复位拖动状态。
- 关闭语义：内嵌浮层的标题栏关闭钮只收起浮层（`SwitchLayout(LAYOUT_EDITOR)`），基座（编辑界面）标题栏的
  关闭钮仍退出应用，避免浮层打开后没有退路。
- 第五轮的 AI 改动未触及 `src/framework/`：内嵌(flex)渲染、槽位可见性 / 嵌套状态与窗口段沿用均为框架既有能力；同一提交随附的用户人工控件绘制微调（`Controls.cpp` / `Widgets.cpp`）见下一节「2026-10-25 人工修改（第六轮）」。
- 已知取舍：`cfg.dwWndCount` 同时是「标准窗口数」与「模型来源装配上界」，两枚布局必须装配，因此仍声明
  2 个标准窗口，运行期只显示第 1 个；第二个窗口不参与渲染（无渲染段）。
- 待本地人工复核：准备浮层在编辑窗口内的位置与尺寸沿用准备布局的原声明几何（窗体区 `x` / `y` 与宽高因子），
  视觉位置是否符合预期；准备浮层显示时编辑界面控件仍可见（业务已拦事件），是否需要进一步弱化视觉；
  标题栏拖动改为鼠标客户区坐标 + 抓取偏移后的跟手程度（远程会话无法合成鼠标交互）。

### 2026-10-25 人工修改（第六轮，随第 8 次提交合并）

用户在提交 `980065c` 上亲手微调了标题栏与关闭钮的绘制，随后以 `git commit --amend` 合入同一提交（提交信息同时改为「准备界面改为编辑界面内嵌浮层布局」）。本节把这次人工修改的内容、依据与基线固定下来：后续会话应把它当作权威实现，不得以旧提交、备份或 AI 重新生成的版本覆盖。

改动位置与内容：

- `src/framework/render/imgui/control/Controls.cpp`，`TitleBar`：
  - 坐标来源改为屏幕坐标。旧写法直接拿布局本地坐标 `info->position` 去调 `ImDrawList::AddRectFilled`，位置会随窗口 / 容器原点偏移；新写法先 `ImGui::SetCursorPos(pos)` 再 `ImGui::GetCursorScreenPos()` 取屏幕坐标 `position`，底条与关闭钮都用它定位。
  - 标题底条改为内缩 1 像素：`position + (1, 1)` 到 `position + size - (1, 0)`；圆角取 `sizeinfo.space + 1`，只圆上角（`ImDrawFlags_RoundCornersTop`）；颜色直接取 `wayTitle.palette.crBackground.crBase`，不再经 `StateColorU32` 折算。
  - 关闭钮 `ExitSign` 改传屏幕坐标 `position + (size.x - size.y * exitwidth, 0)`（该部件内部走 `SetCursorScreenPos`）。
  - 标题文本 `Widgets::Base::Text` 仍传布局本地坐标（该部件内部走 `SetCursorPos`），两种坐标按部件各自的入口区分，不是遗漏。
  - 保留了三行被注释掉的调试绘制（两行 `AddRect` 与一行红色 `AddRectFilled`）以及「此时会破坏原边框，所以补画」的说明，作为人工核对时的留痕；是否清理待用户决定。
- `src/framework/render/imgui/control/Widgets.cpp`，`ExitSign`：悬停底不再满铺 `pos .. btn_end`，改为 `pos + (0, 1)` 到 `btn_end - (1, 0)`，圆角 `sizeinfo.space + 1`、只圆右上角，颜色直接取 `palette.crBackground.crHovered`；与标题栏底条保持同一内缩、圆角与取色口径。
- 文档同步（用户手写，随同批提交）：`Widgets.md` 补「段分隔符按设计字号垂直居中」「四段输入文字固定取基准前景色」；`Controls.md` 更新 `ListTimeSpan` 行件描述（`TimeSpanRow` 于 2026-10-24 几乎重建，含自算矩形、三层自绘、双隐式按钮、抬起触发）。
- `src/framework/render/imgui/ability/ImGuiRender.cpp` 的修改时间有更新，但与上一提交相比净内容一致（`git diff` 为空），视为改后撤销，不计入本轮人工修改。

判断依据：

- 时间线：`Controls.cpp` 修改时间 11:33、`Widgets.cpp` 11:34、`git commit --amend` 11:36，三者连续；
- 风格：新代码带「获取真实坐标」「获取绘制列表」「此时会破坏原边框，所以补画」等手写注释与被注释掉的调试绘制，与同文件 AI 生成的注释风格不同；
- 差异范围：相对 amend 前的 `f7d7ab2`，`Controls.cpp` 只有 `TitleBar` 的位置 / 底条 / 关闭钮坐标与 `TimeSpanRow` 的一处空格格式，`Widgets.cpp` 只有 `ExitSign` 的悬停底。

基线核对（2026-10-25 记录时实测）：

| 文件 | Git blob（HEAD:路径） | 工作区 SHA256 |
| --- | --- | --- |
| `src/framework/render/imgui/control/Controls.cpp` | `dbbe20c4661968385c21370bd46922e61a289d0c` | `88111241717977AF1CB95345B00C26E78B75B9ED24DDBA339292353872EAE2F7` |
| `src/framework/render/imgui/control/Widgets.cpp` | `2bd1b0b28177183bbc3bc90e22fbb2b1042cabc3` | `7CCB449C2ADEE4E84FB9480C23378D79E3C1E6FB6B6122BD6EC318D634A3CFD7` |
| `VideoCut.json` | `76de6b6a1661b32cca3e16c8b15fce80f0defac7` | `AC0881F684CD9E3312B18D73A6F9FC93F72CE4DFADE4F2CAB14240D3ED0AA1D2` |
| `src/app/AppState.cpp` | `25375103220468be269ff7d91946d40e4dda94e0` | `34AE49D403B9BDD7D97ECB87B5890B66211A6E314D9703A0145378FA867A65DA` |

核对方式：`git status` 干净；`git hash-object <文件>` 与 `git rev-parse HEAD:<路径>` 相同；`Get-FileHash -Algorithm SHA256` 与表内值相同。三条同时成立才算基线未被改动。

本轮只做记录与核对，未修改任何源码，也未重新构建 / 运行。

### 后续可优化：两段结构的反向重构（暂不实施）

`TimeSpanRow` 重建后形成的写法（先算 `boot` / `mid` / `end` 三块矩形、两段各自收集状态、`ChannelsSplit(3)` 分层、按钮用隐式项 + 自绘）可反向用于 `Widgets::Base::InputAction` 与 `ComboAction`。2026-10-24 决定暂不实施，先记录方向与现状问题。

现状问题：

- `InputAction`：`state` 只取左段输入项，右段按钮仍是原生 `ImGui::Button`，按钮 hover / active 不参与外框取色；整块底只有一种状态色，按钮区靠原生不透明底覆盖。
- `ComboAction`：已分 `state.combo` / `state.button`，但用 `ChannelsSplit(2)`，底由原生 Combo / Button 不透明绘制、边框在通道 1；底色靠 8 个 `PushStyleColor`，未走统一状态位取色。
- 两处的按钮宽度夹取、`boot` / `mid` / `end` 拼装与按钮绘制逻辑重复；按钮均为原生 `Button`，与 `TimeSpanRow` 的隐式按钮 + 自绘口径不一致。

可复用模式（实施时的落点）：

1. `TwoPartGeo`：输入 `pos` / `size` / `fBtnW` / `fGap`，输出内容段、按钮段与整体矩形（屏幕坐标），替换两处重复的宽度夹取与拼装。
2. `ActionButton`：`InvisibleButton` + 状态位 + 自绘底 / 文字 / 框，抬起触发（与原生 Button 语义一致），供两处共用。
3. 统一分层：通道 0 分块底（内容段按 `crBackground`、按钮段按 `crSurface`，各自按状态取色）、通道 1 原生内容（透明 FrameBg）或按钮文本、通道 2 分块框（外框取两段合并态）。

影响面：`CTRL_TYPE_INPUT_ACTION` / `CTRL_TYPE_COMBO_ACTION` 以及复用它们的列表行（`LIST_INPUT` / `LIST_COMBO` 的行块）都会受影响；VideoCut 当前布局未使用这两种控件，应用侧无直接变化。改为隐式按钮时，原生 Button 的键盘 / 手柄导航与默认交互语义需要在实施时保留或显式补回。

## 6. 构建与运行

### 依赖自动安装

工程使用 vcpkg manifest 模式：`vcpkg.json` 声明 `ffmpeg[nvcodec,qsv,x264]`，`VideoCut.vcxproj` 导入 `$(VcpkgRoot)\scripts\buildsystems\msbuild\vcpkg.props/targets`，构建时自动执行：

```text
vcpkg install --triplet x64-windows-static --x-manifest-root=... --x-install-root=...\vcpkg_installed\x64-windows-static
```

无需手动安装；首次构建会下载并编译 FFmpeg（耗时较长）。`vcpkg-configuration.json` 固定 vcpkg registry 基线。

### 状态

| 项 | 结果 |
| --- | --- |
| 语法检查（`cl /Zs`） | 全部改动文件通过 |
| 工程构建（本地 vcpkg 缓存） | 通过：0 错误、0 警告；产物 `out\Debug\VideoCut.exe` |
| 运行验证 | 通过：启动加载 Prepare 布局，自动打开测试视频（4K 30fps），切换编辑布局并连续渲染 700+ 帧；事件绑定 Prepare 3 枚、Editor 11 枚 |
| vcpkg manifest 自动安装 | 配置完成（`vcpkg.json` + `vcpkg-configuration.json` + props/targets 导入）；本机首次 `git fetch` GitHub registry 长时间无进展，自动安装未在本轮完成，需要可用的网络条件 |
| 时间条订正后重建（2026-10-24） | 通过：`msbuild`（经 IPCHost，`-p:VcpkgEnableManifest=false` + 本机已有 FFmpeg 安装目录）0 错误，本次改动文件 0 警告；产物 `out\Debug\VideoCut.exe` |
| 时间条订正后运行（2026-10-24） | 通过：打开测试视频（4K 30fps，100 ms）并渲染 1081 帧无崩溃；Editor 布局 18 控件 / 11 绑定加载成功，说明 `TIMEBAR` 缓冲装配正常；时码输入的点击编辑、滚轮微调与视觉细节待本地人工复核 |
| 人工订正后（2026-10-24 晚） | 未重新构建 / 运行（本轮只补文档记录）；代码审阅发现四段输入横坐标遗留，见 §5「2026-10-24 人工订正（第三轮）」 |
| 时间轴联动修复（2026-10-24 深夜） | 通过：`msbuild`（经 IPCHost，关闭 manifest 安装 + 本机 `vcpkg_installed\x64-windows-static` 作为安装根）0 错误 0 警告；运行冒烟自动打开测试视频（4K 30fps）渲染 451 帧、退出码 0。播放时时间条与滑块跟随、拖动滑块或提交时间条后预览跳转属鼠标交互，待本地人工复核 |
| 准备界面改为内嵌(flex)（2026-10-25） | 通过：`msbuild`（经 IPCHost）Debug 与 Release 均 0 错误（Debug 0 警告；Release 的 5 条警告全在既有 `VideoEncoder.cpp`）。Release 冒烟三组退出码均为 0：默认启动（准备隐藏：帧=31 段=31 绘制命令=62）、`VIDEOCUT_START_LAYOUT=prepare`（准备浮层显示：帧=41 段=41 绘制命令=164）、打开 4K 测试视频（准备隐藏：帧=73 段=73 绘制命令=146）。渲染段恒为每帧 1 段；日志确认默认「准备=槽 1(可见=0 内嵌=1)」、prepare 时「可见=1 内嵌=1」。准备浮层标题栏拖动（已改为鼠标客户区坐标 + 按下帧抓取偏移）、浮层位置观感、准备阶段对编辑界面控件的点击拦截、浮层关闭钮仅收起浮层，均待本地人工复核 |
| 人工修改记录（2026-10-25） | 仅记录，不改源码、不重新构建：用户在第 8 次提交 `980065c` 上人工微调标题栏底条与关闭钮（改用屏幕坐标、1 像素内缩、只圆上角、直接取 `crBase` / `crHovered`），并 amend 合入同一提交。记录时 `git status` 干净，关键文件 blob 与工作区 SHA256 一致（见 §5 表）；该实现为后续开发基线 |

## 7. 决策记录

2026-10-24 用户反馈：

1. G1 同意 → 已实现；
2. G2 同意实现 → 已实现；
3. G3 同意新增 → 已实现（2026-10-24 第二轮订正：改为复用 `TIMEBAR` 承载编辑，见下文第 9 条）；
4. G4 同意新增 → 已实现；
5. G5 暂缓；
6. G6 暂缓；
7. G7 同意 → 已实现；
8. FFmpeg 不手动安装，配置 vcpkg 后由构建自动安装 → 已改为 manifest 自动安装。

2026-10-24 含义订正（第二轮）：

9. `TimeBar` 就是可编辑的时间条，绘制方式参考旧 VideoCut 源项目；`Timespan` 含义不变（只读时段显示）
   → `CTRL_TYPE_TIMEBAR` 改为可编辑时间条，撤销 `CTRL_TYPE_TIMEBAR_EDIT`。

2026-10-24 人工订正（第三轮，用户本机完成，提交 `f7d7ab2`）：

10. 界面密度按 13 号参考字号收紧（原 17），右侧栏加宽到 13.5 个字号单位；时间片行件 `TimeSpanRow` 几乎重建
    （自算三块矩形、三层自绘、双隐式按钮、文本删除钮、抬起触发）；时间条四段输入按窗口本地坐标定位。
    详见 §5「2026-10-24 人工订正（第三轮）」。
11. `TimeSpanRow` 的重建写法可反向优化 `InputAction` / `ComboAction`；2026-10-24 决定暂不实施，
    方向与现状问题记录在 §5「后续可优化：两段结构的反向重构（暂不实施）」。

2026-10-24 时间轴联动修复（第四轮，业务段）：

12. 修复 `TimeBar` / `SliderRange` 与预览面未联动：拖动滑块与提交时间条只写数值，定位解码统一由每帧预览推进
    按源项目 `DecoderNext` 的结构完成；播放推进时把当前时码写回活动端。改动限 `src/app/`，未改 `src/framework/`
    与 `VideoStream` 解码器。详见 §4.1 与 §6 状态表。

2026-10-25 业务段调整（第五轮）：

13. 准备界面不再作为独立的 Base 布局，改为编辑界面的内嵌(flex)布局：布局槽序改为编辑在前、准备在后；
    业务侧把准备布局登记为嵌套槽并默认隐藏，「重设目录」（`##RESET`）的 `OnReset` 才显示准备浮层、
    打开目录后隐藏；编辑界面事件在准备界面显示时由 `IsEditorActive` 拦下；标题栏拖动按触发控件反查布局，
    内嵌布局改由鼠标客户区坐标与抓取偏移结算（不采用以宿主窗口为基准的 `param`）。第五轮的 AI 改动未触及
    `src/framework/`（同一提交随附的人工微调见第 14 条）。
    详见 §5「2026-10-25 业务段调整（第五轮）」与 §6 状态表。

2026-10-25 人工修改（第六轮，用户本机完成并 amend 入第 8 次提交）：

14. 标题栏底条与关闭钮改用屏幕坐标定位（`SetCursorPos` 后取 `GetCursorScreenPos`）；底条内缩 1 像素、
    只圆上角、直接取 `crBackground.crBase`；关闭钮悬停底同样内缩并只圆右上角、直接取 `crHovered`。
    该实现为权威版本，后续不得还原为旧写法。详见 §5「2026-10-25 人工修改（第六轮）」与 §6 状态表。
