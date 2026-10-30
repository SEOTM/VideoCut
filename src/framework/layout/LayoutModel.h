#pragma once
#include <minwindef.h>
#include <stddef.h>

class Arena;
class MemPool;
class Layout;

constexpr int STR_NAME  = 64;
constexpr int STR_TYPE  = 64;
constexpr int STR_LABEL = 128;
constexpr int STR_TEXT  = 128;
constexpr int STR_EVENT = 64;
constexpr int STR_MODE  = 64;

struct SizeFactor
{
	union
	{
		struct
		{
			FLOAT screenWidth;
			FLOAT screenHeight;
			FLOAT windowWidth;
			FLOAT windowHeight;
			FLOAT controlHeight;
			FLOAT fontSize;
			FLOAT space;
			FLOAT padding;
		};
		FLOAT RefCoeffs[8];
	} coeff;
	int offset;
};

constexpr int GENFACTOR_NUM = 8;

struct SizeFactorSet
{
	SizeFactor x;
	SizeFactor y;
	SizeFactor width;
	SizeFactor height;
};

struct RefSize
{
	char mode[STR_MODE];
	int  unit;
	int  width;
	int  height;
	int  controlHeight;
	int  fontSize;
	int  space;
	int  padding;
};

struct ElementSpec
{
	char        label[STR_LABEL];
	char        text [STR_TEXT ];
	char        type [STR_TYPE ];
	char        event[STR_EVENT];

	SizeFactor  x;
	SizeFactor  y;
	SizeFactor  width;
	SizeFactor  height;

	int         id;
	int         parentID;
};

struct LayoutInfo
{
	char        label[STR_LABEL];
	char        name [STR_NAME ];
	RefSize     refSize;
	SizeFactor  width;
	SizeFactor  height;
	BOOL        IsMaxiable;
	BOOL        IsResizeable;
	LONG        wndX;
	LONG        wndY;
	DWORD       dwElements;
};

struct ElementNode
{
	ElementSpec* data;
	ElementNode* next;
};

struct LayoutNode
{
	LayoutInfo*  info;
	ElementNode* elements;
	LayoutNode*  next;
};

struct FileMidState
{
	LayoutNode   layouts;
	DWORD        dwLayouts;
	DWORD        dwElements;
};

constexpr DWORD BIN_VERSION         = 0x00000200;
constexpr DWORD BIN_VER_GEN_MASK    = 0xFFFFFF00;
constexpr DWORD BIN_VER_FLAG_MASK   = 0x000000FF;
constexpr DWORD BIN_VER_FLAG_COMPRESS = 0x00000001;
constexpr DWORD BIN_VER_FLAG_ENCRYPT  = 0x00000002;
constexpr int   BIN_FILE_HEAD       = 12;
constexpr int   BIN_CRC_SIZE        = 4;
constexpr int   BIN_SEG_HEAD        = 4;
constexpr int   BIN_LAYOUT_SIZE     = (int)sizeof(LayoutInfo);
constexpr int   BIN_LAYOUT_NELEM    = (int)offsetof(LayoutInfo, dwElements);
constexpr int   BIN_CTRL_SIZE       = (int)sizeof(ElementSpec);

static_assert(sizeof(LayoutInfo) == 376, "bin 头部区须与 LayoutInfo 布局一致(结构体变更请同步 BIN_VERSION)");
static_assert(offsetof(LayoutInfo, dwElements) == 372, "bin 头部区内的控件数量偏移须与 LayoutInfo 布局一致");
static_assert(sizeof(ElementSpec) == 536, "bin 控件记录须与 ElementSpec 布局一致(结构体变更请同步 BIN_VERSION)");
static_assert(offsetof(ElementSpec, parentID) == 532, "bin 控件记录内的父容器ID偏移须与 ElementSpec 布局一致");

// 函数: BinSegBytes
// 描述: 计算一个布局段的字节数(段头 + 头部区 + 控件记录区)
constexpr size_t BinSegBytes(DWORD dwCtrls)
{
	return (size_t)BIN_SEG_HEAD + (size_t)BIN_LAYOUT_SIZE + (size_t)dwCtrls * BIN_CTRL_SIZE;
}

constexpr DWORD LAYOUT_MAX = 1024;
constexpr DWORD CTRL_MAX   = 8192;

class LayoutModel
{
private:
	LayoutModel() = delete;
	~LayoutModel() = delete;
public:
	// 函数: SetAllocators
	// 描述: 注入块分配器(静态, 全进程一份; 传 NULL = 解绑)
	static VOID SetAllocators(Arena* arena, MemPool* pool);

	// 函数: StrCopy
	// 描述: 把字符串安全拷入定长数组(超长按容量截断)
	static BOOL StrCopy(char* dst, size_t cap, LPCSTR src);

	// 函数: StateInit
	// 描述: 初始化一份中间态(按布局数与各布局元素数逐块建立两条链)
	static BOOL StateInit(FileMidState& state, DWORD dwLayouts, CONST DWORD* ElementCounts);

	// 函数: StateClear
	// 描述: 释放中间态持有的全部块并把 state 复位为空态
	static VOID StateClear(FileMidState& state);

	// 函数: LayoutAdd
	// 描述: 在布局链尾追加一个空布局(分配一个布局块并尾插)
	static LayoutNode* LayoutAdd(FileMidState& state);

	// 函数: LayoutRemove
	// 描述: 移除指定布局(解链后回收其元素块与布局块)
	static BOOL LayoutRemove(FileMidState& state, DWORD layoutIndex);

	// 函数: LayoutCount
	// 描述: 遍历布局链取布局数量
	static DWORD LayoutCount(CONST FileMidState& state);

	// 函数: LayoutAt
	// 描述: 按布局索引取布局节点(越界返回 NULL)
	static LayoutNode* LayoutAt(CONST FileMidState& state, DWORD layoutIndex);

	// 函数: LayoutFirst
	// 描述: 遍历入口: 取首个布局节点(空文档返回 NULL)
	static LayoutNode* LayoutFirst(CONST FileMidState& state);

	// 函数: LayoutNext
	// 描述: 遍历推进: 取下一布局节点(链尾或入参 NULL 返回 NULL)
	static LayoutNode* LayoutNext(CONST LayoutNode* pNode);

	// 函数: ElementFirst
	// 描述: 遍历入口: 取指定布局的元素链首(无元素或入参 NULL 返回 NULL)
	static ElementNode* ElementFirst(CONST LayoutNode* pNode);

	// 函数: ElementNext
	// 描述: 遍历推进: 取下一元素节点(链尾或入参 NULL 返回 NULL)
	static ElementNode* ElementNext(CONST ElementNode* pElem);

	// 函数: ElementAt
	// 描述: 按布局索引与元素索引取元素规格(越界返回 NULL)
	static ElementSpec* ElementAt(CONST FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

	// 函数: ElementInsert
	// 描述: 在指定位置插入一枚空元素(分配元素块并就地链接)
	static BOOL ElementInsert(FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

	// 函数: ElementRemove
	// 描述: 移除指定元素(解链后回收其元素块)
	static BOOL ElementRemove(FileMidState& state, DWORD layoutIndex, DWORD elementIndex);

	// 函数: RefSizeValueAt
	// 描述: 按参考量序号读经验参数值(序号越界返回 0)
	static INT RefSizeValueAt(CONST RefSize& refSize, INT index);
	// 函数: SetRefSizeValueAt
	// 描述: 按参考量序号写经验参数值(序号越界不写入)
	static VOID SetRefSizeValueAt(RefSize& refSize, INT index, INT value);
	// 函数: DefaultRefSize
	// 描述: 取经验参数默认样例(屏高模式 + 千分比)
	static RefSize DefaultRefSize();
};
