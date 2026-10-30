#pragma once
#include <minwindef.h>
#include <stdint.h>
#include <stddef.h>
#include <basetsd.h>
#include <string.h>
#include "framework/base/mem/MemoryManager.h"
#include "framework/base/FuncPtr.h"

#define CTRL_TYPE_TABLE(ENTRY)                 \
	ENTRY(CTRL_TYPE_INVALID,       "CTRL_TYPE_INVALID")       \
	ENTRY(CTRL_TYPE_UNKNOWN,       "CTRL_TYPE_UNKNOWN")       \
	ENTRY(CTRL_TYPE_WND,           "CTRL_TYPE_WND")           \
	ENTRY(CTRL_TYPE_CNTR,          "CTRL_TYPE_CNTR")          \
	ENTRY(CTRL_TYPE_TITLE,         "CTRL_TYPE_TITLE")         \
	ENTRY(CTRL_TYPE_RECT,          "CTRL_TYPE_RECT")          \
	ENTRY(CTRL_TYPE_TEXT,          "CTRL_TYPE_TEXT")          \
	ENTRY(CTRL_TYPE_INPUT_SINGLE,  "CTRL_TYPE_INPUT_SINGLE")  \
	ENTRY(CTRL_TYPE_INPUT_MULTI,   "CTRL_TYPE_INPUT_MULTI")   \
	ENTRY(CTRL_TYPE_INPUT_NUMERIC, "CTRL_TYPE_INPUT_NUMERIC") \
	ENTRY(CTRL_TYPE_INPUT_REAL,    "CTRL_TYPE_INPUT_REAL")    \
	ENTRY(CTRL_TYPE_INPUT_ACTION,  "CTRL_TYPE_INPUT_ACTION")  \
	ENTRY(CTRL_TYPE_BUTTON,        "CTRL_TYPE_BUTTON")        \
	ENTRY(CTRL_TYPE_LIST,          "CTRL_TYPE_LIST")          \
	ENTRY(CTRL_TYPE_LIST_INPUT,    "CTRL_TYPE_LIST_INPUT")    \
	ENTRY(CTRL_TYPE_LIST_COMBO,    "CTRL_TYPE_LIST_COMBO")    \
	ENTRY(CTRL_TYPE_TIMEBAR,       "CTRL_TYPE_TIMEBAR")       \
	ENTRY(CTRL_TYPE_RENDER,        "CTRL_TYPE_RENDER")        \
	ENTRY(CTRL_TYPE_SLIDER,        "CTRL_TYPE_SLIDER")        \
	ENTRY(CTRL_TYPE_COMBO,         "CTRL_TYPE_COMBO")         \
	ENTRY(CTRL_TYPE_COMBO_ACTION,  "CTRL_TYPE_COMBO_ACTION")  \
	ENTRY(CTRL_TYPE_PROG,          "CTRL_TYPE_PROG")           \
	ENTRY(CTRL_TYPE_CHECKBOX,      "CTRL_TYPE_CHECKBOX")       \
	ENTRY(CTRL_TYPE_LIST_TIMESPAN, "CTRL_TYPE_LIST_TIMESPAN")   \
	ENTRY(CTRL_TYPE_SLIDER_RANGE,  "CTRL_TYPE_SLIDER_RANGE")

enum CtrlType
{
#define CTRL_TYPE_ENUM(name, str) name,
	CTRL_TYPE_TABLE(CTRL_TYPE_ENUM)
#undef CTRL_TYPE_ENUM
	CTRL_TYPE_COUNT
};

struct vec2
{
	float x;
	float y;
	vec2() : x(0.0f), y(0.0f) {}
	vec2(float _x, float _y) : x(_x), y(_y) {}
	vec2 operator+() const { return *this; }
	vec2 operator-() const { return vec2(-x, -y); }
	vec2 operator+(const vec2& other) const { return vec2(x + other.x, y + other.y); }
	vec2 operator-(const vec2& other) const { return vec2(x - other.x, y - other.y); }
	vec2 operator*(const vec2& other) const { return vec2(x * other.x, y * other.y); }
	vec2 operator/(const vec2& other) const { return vec2(x / other.x, y / other.y); }
	BOOL operator!=(const vec2& other) const { return (x != other.x || y != other.y); }
	BOOL operator==(const vec2& other) const { return (x == other.x && y == other.y); }
};

struct CtrlInfo
{
	BufferInfo bufferInfo;
	INT32 nLayer;
	pfnCtrlEvent pEventHandler;
	vec2 size;
	vec2 position;
	vec2 posDeclared;
	LPCSTR label;
	CtrlType type;
	int id;
	int parentID;

	// 函数: 构造函数
	// 描述: 按无效控件初值清零构造(未使用槽位 / 无缓冲 / 无事件处理器 / 无父容器)
	CtrlInfo()
		: id(0), type(CTRL_TYPE_INVALID), label(nullptr),
		position(0.0f, 0.0f), size(0.0f, 0.0f), posDeclared(0.0f, 0.0f),
		bufferInfo{ nullptr, 0 },
		nLayer(0),
		pEventHandler(nullptr),
		parentID(-1)
	{}

	// 函数: IsKnownType
	// 描述: 判断渲染层是否认识本控件类型(未知类型与未使用槽位都不认识)
	BOOL IsKnownType() const;
};

static_assert(sizeof(CtrlInfo) == 80, "CtrlInfo 尺寸变了: 请核对按 sizeof 分配/索引的代码");
static_assert(offsetof(CtrlInfo, nLayer) == 16, "CtrlInfo::nLayer 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, posDeclared) == 48, "CtrlInfo::posDeclared 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, type) == 64, "CtrlInfo::type 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, id) == 68, "CtrlInfo::id 偏移变了: 请核对字段顺序");
static_assert(offsetof(CtrlInfo, parentID) == 72, "CtrlInfo::parentID 偏移变了: 请核对字段顺序");

constexpr LPCSTR CTRL_EVENT_ENTRUST_PARENT = "CTRL_EVENT_ENTRUST_PARENT";

// 函数: CtrlEventIsEntrustParent
// 描述: 判定事件名是否为"委托父容器"特殊事件
inline BOOL CtrlEventIsEntrustParent(LPCSTR eventname)
{
	if (eventname == NULL)
	{
		return FALSE;
	}
	return (strcmp(eventname, CTRL_EVENT_ENTRUST_PARENT) == 0) ? TRUE : FALSE;
}

constexpr INT32 PROJ_LAYER_BACKGROUND = 8;
constexpr INT32 PROJ_LAYER_MIN = 1;
constexpr INT32 PROJ_LAYER_MAX = 16;

// 函数: CtrlLayerOf
// 描述: 由控件自身的层次值得出投影层号(全库唯一换算口径)
inline INT32 CtrlLayerOf(INT32 nLayer)
{
	INT32 nRet = PROJ_LAYER_BACKGROUND + nLayer;
	if (nRet > PROJ_LAYER_MAX)
	{
		nRet = PROJ_LAYER_MAX;
	}
	else if (nRet < PROJ_LAYER_MIN)
	{
		nRet = PROJ_LAYER_MIN;
	}
	return nRet;
}

enum CTRL_LAYER_STEP
{
	CTRL_LAYER_STEP_DOWN = -1,
	CTRL_LAYER_STEP_FLAT = 0,
	CTRL_LAYER_STEP_UP = 1
};

// 函数: CtrlLayerStep
// 描述: 取某控件类型在层次递推中的步进(相对"父控件层次"的调档量)
inline INT32 CtrlLayerStep(CtrlType type)
{
	switch (type)
	{
	case CTRL_TYPE_BUTTON:
	case CTRL_TYPE_CHECKBOX:
		return (INT32)CTRL_LAYER_STEP_UP;
	case CTRL_TYPE_RECT:
	case CTRL_TYPE_INPUT_SINGLE:
	case CTRL_TYPE_INPUT_MULTI:
	case CTRL_TYPE_INPUT_NUMERIC:
	case CTRL_TYPE_INPUT_REAL:
	case CTRL_TYPE_INPUT_ACTION:
	case CTRL_TYPE_LIST:
	case CTRL_TYPE_LIST_INPUT:
	case CTRL_TYPE_LIST_COMBO:
	case CTRL_TYPE_LIST_TIMESPAN:
	case CTRL_TYPE_COMBO:
	case CTRL_TYPE_COMBO_ACTION:
	case CTRL_TYPE_RENDER:
	case CTRL_TYPE_SLIDER:
	case CTRL_TYPE_SLIDER_RANGE:
		return (INT32)CTRL_LAYER_STEP_DOWN;
	default:
		return (INT32)CTRL_LAYER_STEP_FLAT;
	}
}

inline constexpr LPCSTR CtrlTypeNames[] = {
#define CTRL_TYPE_NAME(name, str) str,
	CTRL_TYPE_TABLE(CTRL_TYPE_NAME)
#undef CTRL_TYPE_NAME
};
static_assert(sizeof(CtrlTypeNames) / sizeof(CtrlTypeNames[0]) == (size_t)CTRL_TYPE_COUNT,
	"控件类型表与枚举不同步");

// 函数: GetTypeName
// 描述: 由类型枚举取规范类型串(越界或非法枚举回退未使用槽位的串)
inline LPCSTR GetTypeName(CtrlType type)
{
	if (type < 0 || type >= CTRL_TYPE_COUNT)
	{
		return CtrlTypeNames[CTRL_TYPE_INVALID];
	}
	return CtrlTypeNames[type];
}

// 函数: GetTypeEnum
// 描述: 由类型串取类型枚举(精确匹配; 空串或未命中一律归未知类型)
inline CtrlType GetTypeEnum(LPCSTR type)
{
	if (type == NULL)
	{
		return CTRL_TYPE_UNKNOWN;
	}
	for (size_t i = 0; i < (size_t)CTRL_TYPE_COUNT; i++)
	{
		if (strcmp(type, CtrlTypeNames[i]) == 0)
		{
			return (CtrlType)i;
		}
	}
	return CTRL_TYPE_UNKNOWN;
}

// 函数: CtrlTypeIsList
// 描述: 是否"列表类控件"(布局列表 LIST_INPUT / 控件列表 LIST_COMBO;
inline BOOL CtrlTypeIsList(CtrlType type)
{
	return (type == CTRL_TYPE_LIST_INPUT || type == CTRL_TYPE_LIST_COMBO);
}

// 函数: CtrlTypeIsCompound
// 描述: 是否"复合控件"(单个控件由多枚不相交的可交互子项合成; 如输入动作 = 左段输入 + 右段按钮,
inline BOOL CtrlTypeIsCompound(CtrlType type)
{
	return ((type == CTRL_TYPE_INPUT_ACTION) ||
		(type == CTRL_TYPE_COMBO_ACTION) ||
		(CtrlTypeIsList(type) == TRUE)) ? TRUE : FALSE;
}

// 函数: CtrlTypeIsRawGraphic
// 描述: 是否"纯图元控件"(控件库只产出像素、不自带可交互 ImGui 项的控件类型)
inline BOOL CtrlTypeIsRawGraphic(CtrlType type)
{
	return (type == CTRL_TYPE_TEXT || type == CTRL_TYPE_RECT || type == CTRL_TYPE_UNKNOWN) ? TRUE : FALSE;
}

// 函数: CtrlTypeHandlesClickItself
// 描述: 该控件类型是否由控件库独占其输入项的点击处理(⇒ 渲染出口不得再按通用点击判一次;
//       本判据既含"点击即派发 CLICKED"的类型, 也含"点击只按自身语义派发提交/变更码"的类型)
inline BOOL CtrlTypeHandlesClickItself(CtrlType type)
{
	return ((type == CTRL_TYPE_TEXT) || (type == CTRL_TYPE_RECT) || (type == CTRL_TYPE_BUTTON) ||
		(type == CTRL_TYPE_LIST_TIMESPAN) || (type == CTRL_TYPE_TIMEBAR) ||
		(type == CTRL_TYPE_SLIDER_RANGE)) ? TRUE : FALSE;
}

// 函数: CtrlTypeIsTextKind
// 描述: 是否"文本缓冲控件"(缓冲内容为 C 字符串, 可通过文本通道编辑)
inline BOOL CtrlTypeIsTextKind(CtrlType type)
{
	return (type == CTRL_TYPE_TEXT || type == CTRL_TYPE_BUTTON ||
		type == CTRL_TYPE_INPUT_SINGLE || type == CTRL_TYPE_INPUT_MULTI ||
		type == CTRL_TYPE_INPUT_ACTION || type == CTRL_TYPE_TITLE);
}

namespace OperCode
{
	enum OPER_SEG
	{
		OPER_SEG_COMMON = 0,
		OPER_SEG_OWN = 1,
		OPER_SEG_PAYLOAD = 2,
		OPER_SEG_COUNT
	};

	// 函数: GetSegment
	// 描述: 取操作码某一段的段内值(已右移到最低位; 非法段返回 0)
	constexpr uint64_t GetSegment(uint64_t code, OPER_SEG seg)
	{
		switch (seg)
		{
		case OPER_SEG_COMMON:	return (code & 0x000000000000FFFF);
		case OPER_SEG_OWN:		return ((code >> 16) & 0x000000000000FFFF);
		case OPER_SEG_PAYLOAD:	return (code >> 32);
		default:				return 0;
		}
	}

	// 函数: IsWithin
	// 描述: 判断操作码某一段是否"有码"(段内值非 0; 非法段判无码)
	constexpr BOOL IsWithin(uint64_t code, OPER_SEG seg)
	{
		return (GetSegment(code, seg) != 0) ? TRUE : FALSE;
	}

	// 函数: MakeCode
	// 描述: 把三段段内值拼成整字操作码(公共在低 16 位 · 专属在次 16 位 · 搭载在高 32 位)
	constexpr uint64_t MakeCode(uint16_t common, uint16_t own, uint32_t payload)
	{
		return (uint64_t)common | ((uint64_t)own << 16) | ((uint64_t)payload << 32);
	}

	// 函数: MakeCodeCommon
	// 描述: 构造只含公共段的整字操作码(专属段与搭载段置空)
	constexpr uint64_t MakeCodeCommon(uint16_t common)
	{
		return MakeCode(common, 0, 0);
	}

	// 函数: MakeCodeOwn
	// 描述: 构造只含专属段的整字操作码(公共段与搭载段置空)
	constexpr uint64_t MakeCodeOwn(uint16_t own)
	{
		return MakeCode(0, own, 0);
	}
}

constexpr uint64_t OPER_SEG_MASK_COMMON = 0x000000000000FFFF;
constexpr uint64_t OPER_SEG_MASK_OWN = 0x00000000FFFF0000;
constexpr uint64_t OPER_SEG_MASK_PAYLOAD = 0xFFFFFFFF00000000;

constexpr uint64_t OPER_NONE = 0x0;

constexpr uint16_t OPER_COMMON_PAYLOAD_SHIFT = 14;
constexpr uint16_t OPER_COMMON_PAYLOAD_MASK = 0xC000;
constexpr uint16_t OPER_COMMON_CODE_MASK = 0x3FFF;

constexpr uint16_t OPER_PAYLOAD_NONE = 0;
constexpr uint16_t OPER_PAYLOAD_OPCODE = 1;
constexpr uint16_t OPER_PAYLOAD_DATA = 2;
constexpr uint16_t OPER_PAYLOAD_MIXED = 3;

constexpr uint16_t OPER_COMMON_NONE = 0x0000;
constexpr uint16_t OPER_COMMON_CLICKED = 0x0001;
constexpr uint16_t OPER_COMMON_DOUBLECLICK = 0x0002;
constexpr uint16_t OPER_COMMON_RIGHTCLICK = 0x0004;
constexpr uint16_t OPER_COMMON_ACTIVE = 0x0008;
constexpr uint16_t OPER_COMMON_DEACTIVATED = 0x0010;
constexpr uint16_t OPER_COMMON_HOLD = 0x0020;
constexpr uint16_t OPER_COMMON_DRAG = 0x0040;
constexpr uint16_t OPER_COMMON_DROP = 0x0080;

namespace OperCode
{
	// 函数: GetCommonCode
	// 描述: 取公共码(剔除搭载标志与预留位, 只留公共码位域)
	constexpr uint16_t GetCommonCode(uint64_t code)
	{
		return (uint16_t)(GetSegment(code, OPER_SEG_COMMON) & OPER_COMMON_CODE_MASK);
	}

	// 函数: HasCommon
	// 描述: 测试公共段的位标志(单个位或若干位组合; 已剔除搭载标志)
	constexpr BOOL HasCommon(uint64_t code, uint16_t bit)
	{
		return ((GetCommonCode(code) & bit) != 0) ? TRUE : FALSE;
	}

	// 函数: GetPayloadKind
	// 描述: 取搭载标志(OPER_PAYLOAD_*; 判"有无搭载"只看本标志, 不看搭载段是否为 0)
	constexpr uint16_t GetPayloadKind(uint64_t code)
	{
		return (uint16_t)((GetSegment(code, OPER_SEG_COMMON) & OPER_COMMON_PAYLOAD_MASK) >> OPER_COMMON_PAYLOAD_SHIFT);
	}

	// 函数: MakeCommon
	// 描述: 拼公共段段内值(公共码位域 + 搭载标志; 高位标志位先掩掉再合成)
	constexpr uint16_t MakeCommon(uint16_t code, uint16_t kind)
	{
		return (uint16_t)((code & OPER_COMMON_CODE_MASK) | ((kind & 0x3) << OPER_COMMON_PAYLOAD_SHIFT));
	}
}

constexpr uint16_t OPER_EDIT_SUBMIT = 0x0001;
constexpr uint16_t OPER_EDIT_CHANGED = 0x0002;
constexpr uint16_t OPER_EDIT_CANCEL = 0x0003;

constexpr uint16_t OPER_CUSTOM_IA_INPUT_SUBMIT = 0x0001;
constexpr uint16_t OPER_CUSTOM_IA_BTN_CLICKED = 0x0002;
constexpr uint16_t OPER_CUSTOM_CA_COMBO_CHANGED = 0x0001;
constexpr uint16_t OPER_CUSTOM_CA_BTN_CLICKED = 0x0002;

constexpr uint16_t OPER_LIST_ROW_ACTIVATED = 0x0001;
constexpr uint16_t OPER_LIST_ROW_SUBMIT = 0x0002;
constexpr uint16_t OPER_LIST_ROW_BTN_CLICKED = 0x0003;
constexpr uint16_t OPER_LIST_ROW_SELECTION_CHANGED = 0x0004;
constexpr uint16_t OPER_LIST_ROW_MOVED = 0x0005;
constexpr uint16_t OPER_LIST_ADD = 0x0006;
constexpr uint16_t OPER_LIST_SCROLLED = 0x0007;

constexpr uint16_t OPER_STRUCT_TITLE_CLOSE = 0x0001;

constexpr uint16_t OPER_PROG_UPDATED = 0x0001;
constexpr uint16_t OPER_COMBO_OPENED = 0x0001;
constexpr uint16_t OPER_COMBO_CLOSED = 0x0002;
constexpr uint16_t OPER_SLIDER_THUMB_LEFT = 0x0001;
constexpr uint16_t OPER_SLIDER_THUMB_RIGHT = 0x0002;

constexpr uint64_t OPER_SEG_PROBE = OperCode::MakeCode(0x1234, 0x5678, 0x9ABCDEF0);
static_assert(OperCode::GetSegment(OPER_SEG_PROBE, OperCode::OPER_SEG_COMMON) == 0x1234, "操作码公共段位置/宽度错");
static_assert(OperCode::GetSegment(OPER_SEG_PROBE, OperCode::OPER_SEG_OWN) == 0x5678, "操作码专属段位置/宽度错");
static_assert(OperCode::GetSegment(OPER_SEG_PROBE, OperCode::OPER_SEG_PAYLOAD) == 0x9ABCDEF0, "操作码搭载段位置/宽度错");
static_assert(OperCode::MakeCode(0, 0, 0) == OPER_NONE, "操作码三段全空必须等于 OPER_NONE");
static_assert(OperCode::IsWithin(OPER_SEG_PROBE, OperCode::OPER_SEG_OWN) != FALSE, "操作码 IsWithin 判据错");
static_assert(OperCode::IsWithin(OPER_SEG_PROBE, OperCode::OPER_SEG_COUNT) == FALSE, "操作码非法段应判无码");
static_assert(OperCode::GetCommonCode(OperCode::MakeCommon(OPER_COMMON_CLICKED, OPER_PAYLOAD_NONE)) == OPER_COMMON_CLICKED, "公共码取码错");
static_assert(OperCode::HasCommon(OperCode::MakeCommon(OPER_COMMON_CLICKED, OPER_PAYLOAD_NONE), OPER_COMMON_CLICKED) != FALSE, "公共位测试错");
static_assert(OperCode::GetPayloadKind(OperCode::MakeCommon(OPER_COMMON_CLICKED, OPER_PAYLOAD_DATA)) == OPER_PAYLOAD_DATA, "搭载标志取码错");
static_assert(OperCode::MakeCodeCommon(0x1234) == OperCode::MakeCode(0x1234, 0, 0), "MakeCodeCommon 与 MakeCode 不等价");
static_assert(OperCode::MakeCodeOwn(0x5678) == OperCode::MakeCode(0, 0x5678, 0), "MakeCodeOwn 与 MakeCode 不等价");
static_assert(OperCode::MakeCodeOwn(OPER_EDIT_SUBMIT) ==
	OperCode::MakeCode(OperCode::MakeCommon(OPER_COMMON_NONE, OPER_PAYLOAD_NONE), OPER_EDIT_SUBMIT, 0),
	"编辑族提交码构造须与 MakeCode(MakeCommon(NONE,NONE), OPER_EDIT_SUBMIT, 0) 一致");

// 函数: PackWndPos
// 描述: 把窗口期望位置打包进事件的通用参数槽(低 32 位 = x, 高 32 位 = y)
inline uint64_t PackWndPos(CONST vec2& pos)
{
	CONST INT32 nX = (INT32)pos.x;
	CONST INT32 nY = (INT32)pos.y;
	return (uint64_t)(uint32_t)nX | ((uint64_t)(uint32_t)nY << 32);
}

// 函数: UnpackWndPos
// 描述: 从事件的通用参数槽解出窗口期望位置(与 PackWndPos 互逆)
inline vec2 UnpackWndPos(uint64_t param)
{
	CONST INT32 nX = (INT32)(uint32_t)(param & 0xFFFFFFFFull);
	CONST INT32 nY = (INT32)(uint32_t)(param >> 32);
	return vec2((FLOAT)nX, (FLOAT)nY);
}

struct ListBuffer
{
	uint32_t dwSelected;
	uint32_t dwCount;
	uint32_t dwCapacity;
	uint32_t cbItem;
	uint32_t dwTop;
};

constexpr uint32_t LIST_SEL_NONE = 0xFFFFFFFF;
constexpr uint32_t LIST_SPECIAL_FIRST = 0;
constexpr uint32_t LIST_SPECIAL_LAST = 1;

// ========================
// 即时状态
// ========================

namespace ImmedState
{
	enum StateBit :uint32_t
	{
		// 基础状态
		IMMED_STATE_BASE = 0,
		IMMED_STATE_HOVERED = 1 << 0,
		IMMED_STATE_FOCUSED = 1 << 1,
		IMMED_STATE_ACTIVE = 1 << 2,
		IMMED_STATE_EDITED = 1 << 3,
		IMMED_STATE_SUBMITTED = 1 << 4,
		IMMED_STATE_DRAGGING = 1 << 5,
		IMMED_STATE_DISABLED = 1 << 6,
		IMMED_STATE_DEACTIVATED = 1 << 7,
		// 组合状态
		IMMED_STATE_CLICK_COMPLETED = IMMED_STATE_DEACTIVATED | IMMED_STATE_HOVERED,
	};

	static inline StateBit operator|(StateBit s1, StateBit s2) { return (StateBit)((uint32_t)s1 | (uint32_t)s2); }
	static inline StateBit operator&(StateBit s1, StateBit s2) { return (StateBit)((uint32_t)s1 & (uint32_t)s2); }
	static inline StateBit operator^(StateBit s1, StateBit s2) { return (StateBit)((uint32_t)s1 ^ (uint32_t)s2); }
	static inline StateBit operator~(StateBit s) { return (StateBit)~(uint32_t)s; }
	static inline StateBit& operator|=(StateBit& s1, StateBit s2) { s1 = s1 | s2; return s1; }
	static inline StateBit& operator&=(StateBit& s1, StateBit s2) { s1 = s1 & s2; return s1; }
	static inline StateBit& operator^=(StateBit& s1, StateBit s2) { s1 = s1 ^ s2; return s1; }
	static inline bool operator==(StateBit s1, StateBit s2) { return (uint32_t)s1 == (uint32_t)s2; }
	static inline bool operator!=(StateBit s1, StateBit s2) { return (uint32_t)s1 != (uint32_t)s2; }


	struct State
	{
	private:
		StateBit state;
	public:
		State() :state(IMMED_STATE_BASE) {}
		State(StateBit s) :state(s) {}

		State& operator=(StateBit s) { state = s; return *this; }

		State operator|(const StateBit s) const { State s2(*this); s2 |= s; return s2; }
		State operator|(const State s) const { State s2(*this); s2 |= s.state; return s2; }
		State& operator|=(StateBit s) { state |= s; return *this; }

		State operator&(StateBit s) const { State s2(*this); s2 &= s; return s2; }
		State operator&(const State s) const { State s2(*this); s2 &= s.state; return s2; }
		State& operator&=(StateBit s) { state &= s; return *this; }

		State operator^(StateBit s)  const { State s2(*this); s2 ^= s; return s2; }
		State operator^(const State s) const { State s2(*this); s2 ^= s.state; return s2; }
		State& operator^=(StateBit s) { state ^= s; return *this; }


		inline bool  With(StateBit s) const { return (state & s) != 0; }
		inline bool  Without(StateBit s) const { return (state & s) == 0; }
		inline bool  Is(StateBit s) const { return (state & s) == s; }
		inline bool  IsNot(StateBit s) const { return (state & s) != s; }

		inline bool   IsHovered() const { return With(IMMED_STATE_HOVERED); }
		inline bool   IsFocused() const { return With(IMMED_STATE_FOCUSED); }
		inline bool   IsActive() const { return With(IMMED_STATE_ACTIVE); }
		inline bool   IsEdited() const { return With(IMMED_STATE_EDITED); }
		inline bool IsSubmitted() const { return With(IMMED_STATE_SUBMITTED); }
		inline bool IsDragging() const { return With(IMMED_STATE_DRAGGING); }
		inline bool   IsDisabled() const { return With(IMMED_STATE_DISABLED); }
		inline bool   IsDeactivated() const { return With(IMMED_STATE_DEACTIVATED); }

		State& Set(StateBit s) { state |= s; return *this; }
		State& Clear(StateBit s) { state &= ~s; return *this; }
		State& Reset() { state = IMMED_STATE_BASE; return *this; }
	};
}
