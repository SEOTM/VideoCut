# 布局文档格式

Kanat 的界面由布局文档声明。文档有两种等价形态：

- **JSON**：工程源，便于阅读与编辑；
- **bin**：二进制镜像，加载更快，带 CRC32 校验；发布包不附带转换工具，格式见第 9 节。

运行期先看同名 `.bin` 是否存在，再回退 `.json`。形态由内容嗅探：跳过 UTF-8 BOM 与空白后，首个字符是 `{` 即按 JSON 解析，否则按 bin 解析。

## 1. 顶层结构

```json
{
  "layouts": [
    {
      "name": "DemoMain",
      "label": "Kanat Demo",
      "IsMaxiable": false,
      "IsResizeable": false,
      "width": { "coeff": [0, 0, 1, 0, 0, 0, 0, 0], "offset": 0 },
      "height": { "coeff": [0, 0, 0, 1, 0, 0, 0, 0], "offset": 0 },
      "autosize": { "...": "..." },
      "ctrls": [ { "...": "..." } ]
    }
  ]
}
```

| 字段 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| `layouts` | array | 是 | 1 到 1024 个布局；单文档至少一个布局 |

## 2. 布局字段

| 字段 | 类型 | 缺省 | 说明 |
| :--- | :--- | :--- | :--- |
| `name` | string | `""` | 布局名，最长 63 字节；用于日志与标识 |
| `label` | string | `""` | 布局标签，最长 127 字节 |
| `IsMaxiable` | bool | `false` | 是否允许最大化（窗口行为由宿主解释） |
| `IsResizeable` | bool | `false` | 是否允许调整大小 |
| `wndX` / `wndY` | int | `0` | 窗口桌面位置；布局文档层面的初值 |
| `width` | SizeFactor | 是 | 布局宽度因子；相对 `autosize` 参考量解算 |
| `height` | SizeFactor | 是 | 布局高度因子 |
| `autosize` | object | 见下 | 参考尺寸与经验参数 |
| `ctrls` | array | 是 | 控件数组；单布局最多 8192 枚 |

`width` / `height` 缺失或不是合法 SizeFactor 时，整份文档拒绝加载。

## 3. autosize

```json
"autosize": {
  "mode": "AUTO_SIZE_BY_SCREEN_HEIGHT",
  "unit": 0,
  "width": 570,
  "height": 500,
  "controlHeight": 25,
  "fontSize": 13,
  "space": 10,
  "padding": 4
}
```

| 字段 | 类型 | 缺省 | 说明 |
| :--- | :--- | :--- | :--- |
| `mode` | string 或 int | 屏高模式 | 参考基准：屏幕宽 / 屏幕高 / 窗口宽 / 窗口高 |
| `unit` | int | `0` | `0` = 千分比，`1` = 像素 |
| `width` | int | `622` | 参考宽度 |
| `height` | int | `432` | 参考高度 |
| `controlHeight` | int | `25` | 参考控件高度 |
| `fontSize` | int | `17` | 参考字号 |
| `space` | int | `10` | 参考控件间隔 |
| `padding` | int | `4` | 参考内边距 |

`mode` 的四个合法字符串：

| 字符串 | 基准 |
| :--- | :--- |
| `AUTO_SIZE_BY_SCREEN_WIDTH` | 主显示器宽度 |
| `AUTO_SIZE_BY_SCREEN_HEIGHT` | 主显示器高度 |
| `AUTO_SIZE_BY_WINDOW_WIDTH` | 窗口客户区宽度 |
| `AUTO_SIZE_BY_WINDOW_HEIGHT` | 窗口客户区高度 |

六个参考值非负且上限 10000。未知字符串回落屏高模式；越界整数回落索引 0（屏宽模式）。**首枚布局的 autosize 同时决定窗口几何与全局建面字号。**

## 4. 尺寸因子

控件的 `x` / `y` / `width` / `height`，以及布局的 `width` / `height`，都是同一个结构：

```json
{ "coeff": [0, 0, 1, 0, 0, 0, 0, 0], "offset": 0 }
```

`coeff` 必须恰好 8 项，依次乘以下列参考量：

| 下标 | 参考量 | 含义 |
| ---: | :--- | :--- |
| 0 | `screenWidth` | 主显示器宽度 |
| 1 | `screenHeight` | 主显示器高度 |
| 2 | `windowWidth` | 窗口客户区宽度 |
| 3 | `windowHeight` | 窗口客户区高度 |
| 4 | `controlHeight` | 控件高度基准 |
| 5 | `fontSize` | 字号 |
| 6 | `space` | 控件间隔 |
| 7 | `padding` | 内边距 |

解算：

```text
像素 = Σ(coeff[i] × 参考量[i]) + offset
```

`offset` 以像素为单位，用于对齐或微调。系数为 0 的项不参与。

## 5. 控件字段

| 字段 | 类型 | 缺省 | 说明 |
| :--- | :--- | :--- | :--- |
| `id` | int | 数组下标 | 控件 ID；同一布局内应唯一 |
| `type` | string | 必填 | 控件类型名，见下节 |
| `parent` | int | `-1` | 父容器 ID；`-1` 表示无父容器；小于 `-1` 按 `-1` 处理 |
| `label` | string | `""` | 控件标签，最长 127 字节；部分控件用作内部标识 |
| `text` | string | `""` | 文本内容，最长 127 字节；输入类控件作为初始值 |
| `event` | string | `""` | 事件名，最长 63 字节；空串表示不绑定 |
| `x` / `y` | SizeFactor | 必填 | 位置因子 |
| `width` / `height` | SizeFactor | 必填 | 尺寸因子 |

`type` 未命中类型表时按 `CTRL_TYPE_UNKNOWN` 处理，绘制为占位矩形，不中断加载。`x` / `y` / `width` / `height` 任一缺失或非法，整份文档拒绝加载。

字符串超长会被截断并打印诊断；不会静默写入越界。

## 6. 控件类型

类型名区分大小写，完整清单见 `src/framework/base/ControlSpec.h` 的 `CTRL_TYPE_TABLE`。

| 类型 | 用途 | 数据 |
| :--- | :--- | :--- |
| `CTRL_TYPE_WND` | 窗口根；`position` 兼作窗口位置记录 | 无缓冲 |
| `CTRL_TYPE_CNTR` | 内容容器；承载内容控件并提供原点与裁剪 | 无缓冲 |
| `CTRL_TYPE_TITLE` | 标题栏；标题文本 + 右上关闭钮 | 文本缓冲 |
| `CTRL_TYPE_RECT` | 纯矩形面 | 无缓冲 |
| `CTRL_TYPE_TEXT` | 静态文本 | 文本缓冲 |
| `CTRL_TYPE_INPUT_SINGLE` | 单行文本输入 | 文本缓冲，容量 256 |
| `CTRL_TYPE_INPUT_MULTI` | 多行文本输入 | 文本缓冲，容量 4096 |
| `CTRL_TYPE_INPUT_NUMERIC` | 整数输入 | `INT` |
| `CTRL_TYPE_INPUT_REAL` | 实数输入 | `DOUBLE` |
| `CTRL_TYPE_INPUT_ACTION` | 输入段 + 右段按钮 | `CtrlInputActionData` |
| `CTRL_TYPE_BUTTON` | 按钮 | 文本缓冲 |
| `CTRL_TYPE_LIST` | 列表（已登记；当前无缓冲、无绘制实现） | 无缓冲 |
| `CTRL_TYPE_LIST_INPUT` | 输入动作行组成的列表 | `ListBuffer`，行块为 `INPUT_ACTION` |
| `CTRL_TYPE_LIST_COMBO` | 下拉行组成的列表 | `ListBuffer`，行块为文本 |
| `CTRL_TYPE_LIST_TIMESPAN` | 时间段列表；逐行显示起止时码，行尾删除钮与行选中可交互 | `ListBuffer`，行块为 `TimeSpanItem` |
| `CTRL_TYPE_TIMEBAR` | 可编辑时间条；提示文本 + 时/分/秒/毫秒四段数字输入，提交时回传编辑提交码 | `CtrlTimeBarData` |
| `CTRL_TYPE_RENDER` | 渲染面；有纹理等比居中，无纹理画占位面 | `CtrlRenderData` |
| `CTRL_TYPE_SLIDER` | 单手柄滑动条 | `CtrlSliderData` |
| `CTRL_TYPE_SLIDER_RANGE` | 双手柄范围滑动条；左/右手柄分别回传专属段 | `CtrlSliderRangeData` |
| `CTRL_TYPE_COMBO` | 下拉框 | `CtrlComboData` |
| `CTRL_TYPE_COMBO_ACTION` | 下拉段 + 右段按钮 | `CtrlComboData` |
| `CTRL_TYPE_PROG` | 进度显示 | `CtrlProgData` |
| `CTRL_TYPE_CHECKBOX` | 复选框 | `CtrlCheckData` |

另有 `CTRL_TYPE_UNKNOWN`（未命中类型表）与 `CTRL_TYPE_INVALID`（未使用槽位 / 无效值）两个哨兵，不应出现在文档中。

`CTRL_TYPE_TIMEBAR_EDIT` 已撤销（时间条本身即含编辑），旧文档写成该串时按未命中类型处理（占位矩形）。

控件缓冲在装配期由 `LayoutBuilder` 按类型建立，容量与布局见 `src/framework/base/CtrlBuffer.h`。

## 7. 父链、层次与坐标

`parent` 是一个布局内的控件 ID，构成父链。父链同时影响三件事：

1. **绘制层次**：`nLayer = 父层次 + CtrlLayerStep(自身类型)`。窗口非客户区控件恒为 0；矩形、输入、列表、下拉、渲染面、滑块下沉一档；按钮与复选框上浮一档；其余继承父层。父不存在、ID 不存在或成环时按 0 起步。
2. **有效坐标**：内容控件的有效坐标 = 声明坐标 + 父链上各控件的声明坐标之和。窗口非客户区控件不参与累加：`WND` 的 `position` 是桌面位置，`TITLE` / `CNTR` 是窗口本地坐标。
3. **事件委托**：`event` 为 `CTRL_EVENT_ENTRUST_PARENT` 时，控件事件绑定到父容器（按 `parent` 上溯）的处理函数。

父控件可以声明在子控件之后，也不要求几何包含关系；推导只认 `parent` 声明。

## 8. 事件

`event` 字段写入事件名，装配期在应用提供的绑定表中查找：

- 完整名：按字符串精确匹配；
- 前缀家族：绑定项给出前缀与一位数字范围，例如 `GENUI_EXP_R` + `0..5`；
- 未命中：空绑定，控件无响应；
- `CTRL_EVENT_ENTRUST_PARENT`：保留值，表示委托父容器，不查绑定表。

事件处理函数签名：

```cpp
VOID Handler(VOID* obj, uint64_t oper, uint64_t param, void* data);
```

`oper` 是 64 位操作码（公共段 / 专属段 / 搭载段），`param` 是通用参数槽。标题栏拖动使用 `PackWndPos` / `UnpackWndPos` 在 `param` 中传窗口期望坐标。

## 9. 二进制格式

bin 与 C++ 结构体的内存镜像一致：

```text
偏移 0   校验码     uint32   CRC32（计算时本字段按 0）
偏移 4   版本号     uint32   0x00000200（代际 2 + 明文标记）
偏移 8   段数量     uint32   1..1024
偏移 12  段 0
         ├─ 段大小   uint32   含本字段自身
         ├─ LayoutInfo            376 字节
         └─ ElementSpec × N       每条 536 字节
         段 1 …
```

### LayoutInfo（376 字节）

| 偏移 | 字段 | 类型 | 说明 |
| ---: | :--- | :--- | :--- |
| 0 | `label` | `char[128]` | 布局标签 |
| 128 | `name` | `char[64]` | 布局名 |
| 192 | `refSize` | 92 字节 | `mode[64]` + `unit` + 六个 int |
| 284 | `width` | SizeFactor | 36 字节 |
| 320 | `height` | SizeFactor | 36 字节 |
| 356 | `IsMaxiable` | `BOOL` | |
| 360 | `IsResizeable` | `BOOL` | |
| 364 | `wndX` | `LONG` | |
| 368 | `wndY` | `LONG` | |
| 372 | `dwElements` | `DWORD` | 本布局控件数 |

### ElementSpec（536 字节）

| 偏移 | 字段 | 类型 |
| ---: | :--- | :--- |
| 0 | `label` | `char[128]` |
| 128 | `text` | `char[128]` |
| 256 | `type` | `char[64]` |
| 320 | `event` | `char[64]` |
| 384 | `x` | SizeFactor（36 字节） |
| 420 | `y` | SizeFactor |
| 456 | `width` | SizeFactor |
| 492 | `height` | SizeFactor |
| 528 | `id` | `int` |
| 532 | `parentID` | `int` |

`SizeFactor` 的 36 字节 = 8 个 `float` 系数 + 1 个 `int` 偏移。

### 校验规则

加载 bin 时依次检查：

1. 文件至少 12 字节；
2. CRC32 与文件头一致；
3. 代际版本号与当前一致；格式标记必须为 0（压缩与加密尚未实现）；
4. 段数量 1..1024；
5. 每段大小不小于 `4 + 376`，不越界，控件区是 536 的整数倍；
6. 段区总长等于文件长度。

任一检查失败即拒绝加载，并在控制台打印 `[BIN]` 诊断。

## 10. 限制与校验一览

| 项 | 上限 / 规则 |
| :--- | :--- |
| 布局数 | 1..1024 |
| 单布局控件数 | 0..8192 |
| 布局名 | 63 字节 |
| 布局标签 | 127 字节 |
| 控件标签 / 文本 | 127 字节 |
| 控件类型 | 63 字节 |
| 事件名 | 63 字节 |
| 尺寸因子系数 | 恰好 8 个 |
| 参考值 | 0..10000 |
| 控件 ID | 同布局内应唯一；重复时按链序首个命中 |
| 父链 | 允许前向声明；成环时按 0 起步，不中断 |

JSON 解析失败、必填字段缺失或段结构非法时，整份文档拒绝加载；调用方会看到初始化失败与 `[JSON]` / `[BIN]` 诊断。

## 11. 最小示例

```json
{
  "layouts": [
    {
      "name": "Main",
      "label": "Main",
      "width":  { "coeff": [0, 0, 1, 0, 0, 0, 0, 0], "offset": 0 },
      "height": { "coeff": [0, 0, 0, 1, 0, 0, 0, 0], "offset": 0 },
      "autosize": {
        "mode": "AUTO_SIZE_BY_SCREEN_HEIGHT",
        "unit": 0,
        "width": 570,
        "height": 500,
        "controlHeight": 25,
        "fontSize": 13,
        "space": 10,
        "padding": 4
      },
      "ctrls": [
        {
          "id": 0,
          "type": "CTRL_TYPE_WND",
          "label": "Main",
          "text": "",
          "event": "",
          "parent": -1,
          "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 180 },
          "y": { "coeff": [0,0,0,0,0,0,0,0], "offset": 120 },
          "width":  { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
          "height": { "coeff": [0,0,0,1,0,0,0,0], "offset": 0 }
        },
        {
          "id": 1,
          "type": "CTRL_TYPE_TITLE",
          "label": "##TITLEBAR",
          "text": "Main",
          "event": "",
          "parent": -1,
          "x": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
          "y": { "coeff": [0,0,0,0,0,0,0,0], "offset": 0 },
          "width":  { "coeff": [0,0,1,0,0,0,0,0], "offset": 0 },
          "height": { "coeff": [0,0,0,0,1,0,0,0], "offset": 0 }
        }
      ]
    }
  ]
}
```

把该 JSON 保存为 `Kanat.json` 放在 exe 同目录即可运行。发布包不附带 JSON → bin 的转换工具；需要二进制形态时按第 9 节格式自行生成。
