#pragma once
#include "framework/base/mem/MemoryManager.h"
#include "framework/base/ControlSpec.h"
#include <stdint.h>

class Layout
{
private:
	struct linknode
	{
		CtrlInfo info;
		linknode* prev;
		linknode* next;
	};
	struct linkinfo
	{
		linknode* pFirst;
		linknode* pLast;
		linknode* pCursor;
		size_t qwCursor;
	};
	union
	{
		CtrlInfo* pCtrlInfos;
		linkinfo* link;
	} ctrls;
	BOOL IsLinkmode;
	size_t qwNumCtrl;
	static Arena* memArena;
	static MemPool* memPool;
	static Layout* NULLOBJ;
	static CtrlInfo NULLINFO;
	// 函数: NodeAt
	// 描述: 取指定索引处的链节点(仅链式模式; 按游标就近定位)
	linknode* NodeAt(size_t index);
	// 函数: NewNode
	// 描述: 新建一枚链节点, 并按入参值构造内嵌的控件信息
	linknode* NewNode(const CtrlInfo& src);
	// 函数: DeleteNode
	// 描述: 析构节点内嵌的控件信息, 并把节点交还小对象池(仅当节点确属本池)
	VOID DeleteNode(linknode* pNode);
	// 函数: UnlinkNode
	// 描述: 把节点从链上摘下(双向链 O(1); 只改链接, 不动节点内存)
	VOID UnlinkNode(linknode* pNode);
	// 函数: InitCtrlLayer
	// 描述: 层次推导的逐枚初值(窗口非客户区控件归 0 / 其余标为未解析)
	VOID InitCtrlLayer(CtrlInfo& info);
	// 函数: SettleCtrlLayer
	// 描述: 为单枚未解析控件定层次(父已定或无父即定值; 父未解析则等下一趟)
	BOOL SettleCtrlLayer(CtrlInfo& info);
	// 函数: 构造函数
	// 描述: 初始化布局对象(按 IsLink 选定储存模式, 并按 num 预置控件)
	Layout(size_t num, BOOL IsLink);
	// 函数: 析构函数
	// 描述: 释放布局对象(链式: 逐节点析构并回池; 线性: 逐控件析构; 数组/链视图随 Arena 整体释放)
	~Layout();
	// 函数: NULL 对象构造函数
	// 描述: 构造空布局对象(储存视图置空, 模式与计数归零)
	Layout();
public:
	// 函数: CreateLayout
	// 描述: 创建布局对象(唯一的创建入口; 储存模式在此确定, 并按 num 预置控件)
	static Layout* CreateLayout(size_t num, BOOL IsLink = FALSE);
	// 函数: GetNullObject
	// 描述: 取空布局引用(无效引用的返回物; 函数内静态实例)
	static Layout& GetNullObject();
	// 函数: DestroyLayout
	// 描述: 析构布局对象(只析构, 不归还分配器)
	static VOID DestroyLayout(Layout* layout);
	// 函数: SetAllocators
	// 描述: 注入两个分配器(静态, 全进程一份; 传 NULL = 解绑)
	static VOID SetAllocators(Arena* arena, MemPool* pool);
	// 函数: IsLinkMode
	// 描述: 查询是否链式储存模式
	BOOL IsLinkMode() const;
	// 函数: SetCtrlInfo
	// 描述: 按索引按值覆盖控件信息
	BOOL SetCtrlInfo(size_t index, const CtrlInfo& info);
	// 函数: GetCtrlInfo
	// 描述: 按索引取控件信息指针
	CtrlInfo* GetCtrlInfo(size_t index);
	// 函数: GetCtrlInfoRef
	// 描述: 按索引取控件信息引用
	CtrlInfo& GetCtrlInfoRef(size_t index);
	// 函数: GetCtrlInfoByID
	// 描述: 按控件 ID 取可写控件指针(链序首个命中; 未找到返回 NULL)
	CtrlInfo* GetCtrlInfoByID(int id);
	// 函数: GetLayoutBase
	// 描述: 按类型取窗口非客户区控件(只在前三个槽位内查找)
	CtrlInfo* GetLayoutBase(CtrlType type);
	// 函数: GetShellNum
	// 描述: 统计窗口非客户区控件个数(WND / TITLE / CNTR 中存在者)
	size_t GetShellNum();
	// 函数: GetTotalNum
	// 描述: 取控件总数
	size_t GetTotalNum() const;
	// 函数: AddCtrlInfo
	// 描述: 在指定位置插入一枚控件(链式专属; 线性模式恒失败)
	BOOL AddCtrlInfo(size_t index, const CtrlInfo& info);
	// 函数: RemoveCtrlInfo
	// 描述: 移除指定位置的控件(链式专属; 线性模式恒失败)
	BOOL RemoveCtrlInfo(size_t index);
	// 函数: IsInvalid
	// 描述: 判定布局是否为无效引用
	BOOL IsInvalid();
	// 函数: DeriveCtrlLayers
	// 描述: 整体重推全部控件的层次(按 parentID 链求值)
	BOOL DeriveCtrlLayers();
	// 函数: DeriveCtrlPositions
	// 描述: 整体重推全部内容控件的有效坐标(按 parentID 链累加声明坐标)
	BOOL DeriveCtrlPositions();
};
