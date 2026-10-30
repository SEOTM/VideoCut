#pragma once
#include <new>
#include <string.h>
#include "framework/layout/Layout.h"

Arena* Layout::memArena = NULL;
MemPool* Layout::memPool = NULL;
Layout* Layout::NULLOBJ = NULL;
CtrlInfo Layout::NULLINFO = {};

// 函数: 构造函数
// 描述: 初始化布局对象(按 IsLink 选定储存模式, 并按 num 预置控件)
Layout::Layout(size_t num, BOOL IsLink) :
	IsLinkmode(FALSE),
	qwNumCtrl(0)
{
	ctrls.pCtrlInfos = nullptr;
	if (memArena == NULL)
	{
		return;
	}
	if (IsLink == FALSE)
	{
		BufferInfo bufInfo = memArena->Allocate(sizeof(CtrlInfo) * num);
		ctrls.pCtrlInfos = static_cast<CtrlInfo*>(bufInfo.buffer);
		if (ctrls.pCtrlInfos == nullptr)
		{
			return;
		}
		for (size_t i = 0; i < num; ++i)
		{
			new (&ctrls.pCtrlInfos[i]) CtrlInfo();
		}
		qwNumCtrl = num;
		return;
	}
	BufferInfo vbuf = memArena->Allocate(sizeof(linkinfo));
	linkinfo* pLink = static_cast<linkinfo*>(vbuf.buffer);
	if (pLink == nullptr || vbuf.size < sizeof(linkinfo))
	{
		return;
	}
	pLink->pFirst = NULL;
	pLink->pLast = NULL;
	pLink->pCursor = NULL;
	pLink->qwCursor = 0;
	ctrls.link = pLink;
	IsLinkmode = TRUE;
	CtrlInfo empty{};
	for (size_t i = 0; i < num; ++i)
	{
		linknode* pNode = NewNode(empty);
		if (pNode == NULL)
		{
			linknode* p = ctrls.link->pFirst;
			while (p != NULL)
			{
				linknode* pNext = p->next;
				DeleteNode(p);
				p = pNext;
			}
			ctrls.pCtrlInfos = nullptr;
			qwNumCtrl = 0;
			IsLinkmode = FALSE;
			return;
		}
		pNode->prev = ctrls.link->pLast;
		if (ctrls.link->pLast != NULL)
		{
			ctrls.link->pLast->next = pNode;
		}
		else
		{
			ctrls.link->pFirst = pNode;
		}
		ctrls.link->pLast = pNode;
		qwNumCtrl++;
	}
}
// 函数: NULL 对象构造函数
// 描述: 构造空布局对象(储存视图置空, 模式与计数归零)
Layout::Layout()
{
	ctrls.pCtrlInfos = NULL;
	IsLinkmode = FALSE;
	qwNumCtrl = 0;
}
// 函数: SetAllocators
// 描述: 注入两个分配器(静态, 全进程一份)
VOID Layout::SetAllocators(Arena* arena, MemPool* pool)
{
	memArena = arena;
	memPool = pool;
}
// 函数: CreateLayout
// 描述: 创建布局对象(分配对象内存, 并校验预置数量是否达成)
Layout* Layout::CreateLayout(size_t num, BOOL IsLink)
{
	if (memArena == NULL)
	{
		return nullptr;
	}
	BufferInfo bufInfo = memArena->Allocate(sizeof(Layout));
	if (bufInfo.buffer == nullptr || bufInfo.size < sizeof(Layout))
	{
		return nullptr;
	}
	Layout* layout = new (bufInfo.buffer) Layout(num, IsLink);
	if (layout->GetTotalNum() != num)
	{
		layout->~Layout();
		return nullptr;
	}
	return layout;
}
// 函数: GetNullObject
// 描述: 取空布局引用(静态实例, 首次调用时登记)
Layout& Layout::GetNullObject()
{
	static Layout nullobj;
	NULLOBJ = &nullobj;
	return nullobj;
}
// 函数: DestroyLayout
// 描述: 析构布局对象(入参 NULL 安全; 不归还分配器)
VOID Layout::DestroyLayout(Layout* layout)
{
	if (layout)
	{
		layout->~Layout();
	}
	return;
}

// 函数: 析构函数
// 描述: 释放布局对象(链式节点回池, 线性控件逐个析构; 数组与链视图不归还)
Layout::~Layout()
{
	if (IsLinkmode && ctrls.link != NULL)
	{
		linknode* p = ctrls.link->pFirst;
		while (p != NULL)
		{
			linknode* pNext = p->next;
			DeleteNode(p);
			p = pNext;
		}
		ctrls.link->pFirst = NULL;
		ctrls.link->pLast = NULL;
		ctrls.link->pCursor = NULL;
		ctrls.link->qwCursor = 0;
		qwNumCtrl = 0;
	}
	else if (ctrls.pCtrlInfos != NULL)
	{
		for (size_t i = 0; i < qwNumCtrl; ++i)
		{
			ctrls.pCtrlInfos[i].~CtrlInfo();
		}
	}
}

// 函数: NewNode
// 描述: 新建一枚链节点, 并按入参值构造内嵌的控件信息
Layout::linknode* Layout::NewNode(const CtrlInfo& src)
{
	linknode* pNode = NULL;
	BufferInfo buf{ nullptr, 0 };
	if (memPool == NULL || sizeof(linknode) > MemPool::MAX_SMALL_SIZE)
	{
		return NULL;
	}
	buf = memPool->Allocate(sizeof(linknode));
	if (buf.buffer == NULL || buf.size < sizeof(linknode))
	{
		return NULL;
	}
	pNode = (linknode*)buf.buffer;
	new (&pNode->info) CtrlInfo(src);
	pNode->prev = NULL;
	pNode->next = NULL;
	return pNode;
}
// 函数: DeleteNode
// 描述: 析构节点内嵌的控件信息, 并把节点交还小对象池
VOID Layout::DeleteNode(linknode* pNode)
{
	if (pNode == NULL)
	{
		return;
	}
	pNode->info.~CtrlInfo();
	if (memPool != NULL && memPool->Contains(pNode) == TRUE)
	{
		memPool->Free((VOID*)pNode);
	}
}
// 函数: UnlinkNode
// 描述: 把节点从链上摘下(只改链接, 不动节点内存)
VOID Layout::UnlinkNode(linknode* pNode)
{
	if (pNode == NULL)
	{
		return;
	}
	if (pNode->prev != NULL)
	{
		pNode->prev->next = pNode->next;
	}
	else
	{
		ctrls.link->pFirst = pNode->next;
	}
	if (pNode->next != NULL)
	{
		pNode->next->prev = pNode->prev;
	}
	else
	{
		ctrls.link->pLast = pNode->prev;
	}
	pNode->prev = NULL;
	pNode->next = NULL;
}

static constexpr INT32 LAYOUT_LAYER_UNRESOLVED = INT32_MIN;

// 函数: IsShellCtrl
// 描述: 判定是否为窗口非客户区控件(层次恒 0, 无视 parent 声明)
static BOOL IsShellCtrl(CtrlType type)
{
	if (type == CTRL_TYPE_WND || type == CTRL_TYPE_TITLE || type == CTRL_TYPE_CNTR)
	{
		return TRUE;
	}
	return FALSE;
}

// 函数: InitCtrlLayer
// 描述: 层次推导的逐枚初值(窗口非客户区控件归 0 / 其余标为未解析)
VOID Layout::InitCtrlLayer(CtrlInfo& info)
{
	if (IsShellCtrl(info.type) != FALSE)
	{
		info.nLayer = 0;
		return;
	}
	info.nLayer = LAYOUT_LAYER_UNRESOLVED;
}

// 函数: SettleCtrlLayer
// 描述: 为单枚未解析控件定层次(父已定或无父即定值; 父未解析则等下一趟)
BOOL Layout::SettleCtrlLayer(CtrlInfo& info)
{
	BOOL bRet = FALSE;
	CtrlInfo* pParent = NULL;
	INT32 nBase = 0;
	if (info.nLayer != LAYOUT_LAYER_UNRESOLVED)
	{
		return TRUE;
	}
	pParent = (info.parentID >= 0) ? GetCtrlInfoByID(info.parentID) : NULL;
	if (pParent == NULL || pParent == &info)
	{
		nBase = 0;
		bRet = TRUE;
	}
	else if (pParent->nLayer == LAYOUT_LAYER_UNRESOLVED)
	{
		bRet = FALSE;
	}
	else
	{
		nBase = pParent->nLayer;
		bRet = TRUE;
	}
	if (bRet != FALSE)
	{
		info.nLayer = nBase + CtrlLayerStep(info.type);
	}
	return bRet;
}

// 函数: DeriveCtrlLayers
// 描述: 整体重推全部控件的层次(按 parentID 链求值; 两种储存模式都支持)
BOOL Layout::DeriveCtrlLayers()
{
	BOOL bRet = FALSE;
	BOOL bChanged = FALSE;
	DWORD dwPass = 0;
	size_t i = 0;
	if (IsLinkmode)
	{
		linknode* p = NULL;
		if (ctrls.link == NULL || ctrls.link->pFirst == NULL)
		{
			return bRet;
		}
		for (p = ctrls.link->pFirst; p != NULL; p = p->next)
		{
			InitCtrlLayer(p->info);
		}
		for (dwPass = 0; dwPass < (DWORD)qwNumCtrl; ++dwPass)
		{
			bChanged = FALSE;
			for (p = ctrls.link->pFirst; p != NULL; p = p->next)
			{
				if (p->info.nLayer != LAYOUT_LAYER_UNRESOLVED)
				{
					continue;
				}
				if (SettleCtrlLayer(p->info) != FALSE)
				{
					bChanged = TRUE;
				}
			}
			if (bChanged == FALSE)
			{
				break;
			}
		}
		for (p = ctrls.link->pFirst; p != NULL; p = p->next)
		{
			if (p->info.nLayer == LAYOUT_LAYER_UNRESOLVED)
			{
				p->info.nLayer = CtrlLayerStep(p->info.type);
			}
		}
		bRet = TRUE;
		return bRet;
	}
	if (ctrls.pCtrlInfos == NULL || qwNumCtrl == 0)
	{
		return bRet;
	}
	for (i = 0; i < qwNumCtrl; ++i)
	{
		InitCtrlLayer(ctrls.pCtrlInfos[i]);
	}
	for (dwPass = 0; dwPass < (DWORD)qwNumCtrl; ++dwPass)
	{
		bChanged = FALSE;
		for (i = 0; i < qwNumCtrl; ++i)
		{
			if (ctrls.pCtrlInfos[i].nLayer != LAYOUT_LAYER_UNRESOLVED)
			{
				continue;
			}
			if (SettleCtrlLayer(ctrls.pCtrlInfos[i]) != FALSE)
			{
				bChanged = TRUE;
			}
		}
		if (bChanged == FALSE)
		{
			break;
		}
	}
	for (i = 0; i < qwNumCtrl; ++i)
	{
		if (ctrls.pCtrlInfos[i].nLayer == LAYOUT_LAYER_UNRESOLVED)
		{
			ctrls.pCtrlInfos[i].nLayer = CtrlLayerStep(ctrls.pCtrlInfos[i].type);
		}
	}
	bRet = TRUE;
	return bRet;
}

// 函数: ParentPosSum
// 描述: 求一枚控件父链上的声明坐标之和(跳过 WND; 自指与成环一律按无父处理)
static vec2 ParentPosSum(Layout& layout, CONST CtrlInfo& info)
{
	vec2 sum(0.0f, 0.0f);
	CONST size_t qwTotal = layout.GetTotalNum();
	size_t qwGuard = 0;
	BOOL IsCyclic = FALSE;
	int nParent = info.parentID;
	while (nParent >= 0)
	{
		CtrlInfo* pParent = layout.GetCtrlInfoByID(nParent);
		if (pParent == NULL || pParent->type == CTRL_TYPE_WND)
		{
			break;
		}
		if (pParent == (CONST CtrlInfo*)&info || qwGuard >= qwTotal)
		{
			IsCyclic = TRUE;
			break;
		}
		sum = sum + pParent->posDeclared;
		nParent = pParent->parentID;
		qwGuard++;
	}
	if (IsCyclic != FALSE)
	{
		sum = vec2(0.0f, 0.0f);
	}
	return sum;
}

// 函数: DeriveCtrlPositions
// 描述: 整体重推全部内容控件的有效坐标(按 parentID 链累加声明坐标; 两种储存模式同一口径)
BOOL Layout::DeriveCtrlPositions()
{
	BOOL bRet = FALSE;
	size_t i = 0;
	if (IsLinkmode)
	{
		linknode* p = NULL;
		if (ctrls.link == NULL || ctrls.link->pFirst == NULL)
		{
			return bRet;
		}
		for (p = ctrls.link->pFirst; p != NULL; p = p->next)
		{
			if (IsShellCtrl(p->info.type) != FALSE)
			{
				continue;
			}
			p->info.position = p->info.posDeclared + ParentPosSum(*this, p->info);
		}
		bRet = TRUE;
		return bRet;
	}
	if (ctrls.pCtrlInfos == NULL || qwNumCtrl == 0)
	{
		return bRet;
	}
	for (i = 0; i < qwNumCtrl; ++i)
	{
		if (IsShellCtrl(ctrls.pCtrlInfos[i].type) != FALSE)
		{
			continue;
		}
		ctrls.pCtrlInfos[i].position = ctrls.pCtrlInfos[i].posDeclared + ParentPosSum(*this, ctrls.pCtrlInfos[i]);
	}
	bRet = TRUE;
	return bRet;
}

// 函数: NodeAt
// 描述: 取指定索引处的链节点(按游标就近定位)
Layout::linknode* Layout::NodeAt(size_t index)
{
	linknode* p = NULL;
	if (IsLinkmode == FALSE || index >= qwNumCtrl)
	{
		return NULL;
	}
	if (ctrls.link->pCursor != NULL)
	{
		p = ctrls.link->pCursor;
		size_t i = ctrls.link->qwCursor;
		while ((i < index) && (p != NULL))
		{
			p = p->next;
			i++;
		}
		while ((i > index) && (p != NULL))
		{
			p = p->prev;
			i--;
		}
	}
	else if (index < (qwNumCtrl / 2))
	{
		p = ctrls.link->pFirst;
		for (size_t i = 0; (i < index) && (p != NULL); ++i)
		{
			p = p->next;
		}
	}
	else
	{
		p = ctrls.link->pLast;
		for (size_t i = qwNumCtrl - 1; (i > index) && (p != NULL); --i)
		{
			p = p->prev;
		}
	}
	if (p != NULL)
	{
		ctrls.link->pCursor = p;
		ctrls.link->qwCursor = index;
	}
	return p;
}

// 函数: IsLinkMode
// 描述: 查询是否链式储存模式
BOOL Layout::IsLinkMode() const
{
	return IsLinkmode;
}

// 函数: SetCtrlInfo
// 描述: 按索引按值覆盖控件信息
BOOL Layout::SetCtrlInfo(size_t index, const CtrlInfo& info)
{
	BOOL ret = FALSE;
	CtrlInfo* p = NULL;
	if (index < qwNumCtrl)
	{
		if (IsLinkmode)
		{
			linknode* pNode = NodeAt(index);
			p = (pNode != NULL) ? &pNode->info : NULL;
		}
		else
		{
			p = &ctrls.pCtrlInfos[index];
		}
		if (p != NULL)
		{
			*p = info;
			ret = TRUE;
		}
	}
	return ret;
}

// 函数: GetCtrlInfo
// 描述: 按索引取控件信息指针
CtrlInfo* Layout::GetCtrlInfo(size_t index)
{
	CtrlInfo* ret = nullptr;
	if (index < qwNumCtrl)
	{
		if (IsLinkmode)
		{
			linknode* pNode = NodeAt(index);
			ret = (pNode != NULL) ? &pNode->info : nullptr;
		}
		else
		{
			ret = &ctrls.pCtrlInfos[index];
		}
	}
	return ret;
}

// 函数: GetTotalNum
// 描述: 取控件总数
size_t Layout::GetTotalNum() const
{
	return qwNumCtrl;
}
// 函数: AddCtrlInfo
// 描述: 在指定位置插入一枚控件(链式专属; 线性模式恒失败)
BOOL Layout::AddCtrlInfo(size_t index, const CtrlInfo& info)
{
	BOOL bRet = FALSE;
	if (IsLinkmode && index <= qwNumCtrl)
	{
		CtrlInfo node = info;
		node.posDeclared = info.position;
		linknode* pNode = NewNode(node);
		if (pNode != NULL)
		{
			if (index == 0)
			{
				pNode->next = ctrls.link->pFirst;
				if (ctrls.link->pFirst != NULL)
				{
					ctrls.link->pFirst->prev = pNode;
				}
				ctrls.link->pFirst = pNode;
				if (ctrls.link->pLast == NULL)
				{
					ctrls.link->pLast = pNode;
				}
			}
			else if (index == qwNumCtrl)
			{
				pNode->prev = ctrls.link->pLast;
				if (ctrls.link->pLast != NULL)
				{
					ctrls.link->pLast->next = pNode;
				}
				ctrls.link->pLast = pNode;
				if (ctrls.link->pFirst == NULL)
				{
					ctrls.link->pFirst = pNode;
				}
			}
			else
			{
				linknode* pAt = NodeAt(index);
				if (pAt == NULL)
				{
					DeleteNode(pNode);
					return FALSE;
				}
				pNode->next = pAt;
				pNode->prev = pAt->prev;
				if (pAt->prev != NULL)
				{
					pAt->prev->next = pNode;
				}
				else
				{
					ctrls.link->pFirst = pNode;
				}
				pAt->prev = pNode;
			}
			if ((ctrls.link->pCursor != NULL) && (index <= ctrls.link->qwCursor))
			{
				ctrls.link->qwCursor++;
			}
			qwNumCtrl++;
			DeriveCtrlLayers();
			DeriveCtrlPositions();
			bRet = TRUE;
		}
	}
	return bRet;
}
// 函数: RemoveCtrlInfo
// 描述: 移除指定位置的控件(链式专属; 线性模式恒失败)
BOOL Layout::RemoveCtrlInfo(size_t index)
{
	BOOL bRet = FALSE;
	if (IsLinkmode && index < qwNumCtrl)
	{
		linknode* pNode = NodeAt(index);
		if (pNode != NULL)
		{
			if (ctrls.link->pCursor == pNode)
			{
				ctrls.link->pCursor = NULL;
				ctrls.link->qwCursor = 0;
			}
			else if ((ctrls.link->pCursor != NULL) && (index < ctrls.link->qwCursor))
			{
				ctrls.link->qwCursor--;
			}
			UnlinkNode(pNode);
			DeleteNode(pNode);
			qwNumCtrl--;
			bRet = TRUE;
		}
	}
	return bRet;
}
// 函数: GetCtrlInfoRef
// 描述: 按索引取控件信息引用(越界返回静态空信息)
CtrlInfo& Layout::GetCtrlInfoRef(size_t index)
{
	if (index < qwNumCtrl)
	{
		if (IsLinkmode)
		{
			linknode* pNode = NodeAt(index);
			if (pNode != NULL)
			{
				return pNode->info;
			}
			return NULLINFO;
		}
		return ctrls.pCtrlInfos[index];
	}
	else
	{
		return NULLINFO;
	}
}
// 函数: GetCtrlInfoByID
// 描述: 按控件 ID 取可写控件指针(链序首个命中; 未找到返回 NULL)
CtrlInfo* Layout::GetCtrlInfoByID(int id)
{
	for (size_t i = 0; i < qwNumCtrl; i++)
	{
		CtrlInfo* pCtrl = GetCtrlInfo(i);
		if (pCtrl != NULL && pCtrl->id == id)
		{
			return pCtrl;
		}
	}
	return NULL;
}
// 函数: GetLayoutBase
// 描述: 按类型取窗口非客户区控件(只在前三个槽位内查找)
CtrlInfo* Layout::GetLayoutBase(CtrlType type)
{
	CtrlInfo* ret = nullptr;
	const size_t qwScan = (qwNumCtrl < 3) ? qwNumCtrl : 3;
	for (size_t i = 0; (i < qwScan) && (ret == nullptr); ++i)
	{
		CtrlInfo* p = GetCtrlInfo(i);
		if (p != NULL && p->type == type)
		{
			ret = p;
		}
	}
	return ret;
}
// 函数: GetShellNum
// 描述: 统计窗口非客户区控件个数(WND / TITLE / CNTR 中存在者)
size_t Layout::GetShellNum()
{
	size_t ret = 0;
	if (this->GetLayoutBase(CTRL_TYPE_WND))
	{
		ret++;
	}
	if (this->GetLayoutBase(CTRL_TYPE_TITLE))
	{
		ret++;
	}
	if (this->GetLayoutBase(CTRL_TYPE_CNTR))
	{
		ret++;
	}
	return ret;
}
// 函数: IsInvalid
// 描述: 判定布局是否为无效引用
BOOL Layout::IsInvalid()
{
	BOOL ret = FALSE;
	CtrlInfo* pWnd = this->GetLayoutBase(CTRL_TYPE_WND);
	if (this == NULLOBJ)
	{
		ret = TRUE;
	}
	else if (!(pWnd && this->GetLayoutBase(CTRL_TYPE_CNTR)))
	{
		ret = TRUE;
	}
	else if (pWnd->size == vec2{ 0,0 })
	{
		ret = TRUE;
	}
	return ret;
}
