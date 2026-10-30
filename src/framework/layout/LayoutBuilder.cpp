#include <windows.h>
#include <string.h>
#include "framework/layout/LayoutBuilder.h"
#include "framework/base/mem/AllocatorHub.h"
#include "framework/base/BasicDesign.h"
#include "framework/base/CtrlBuffer.h"
#include "framework/dispatch/UIEventHandler.h"

// 函数: ElementSpecAt
// 描述: 按索引取元素链上的元素规格(越界返回 NULL)
static CONST ElementSpec* ElementSpecAt(CONST LayoutNode* pNode, DWORD index)
{
	CONST ElementSpec* pRet = NULL;
	DWORD dwIndex = 0;
	for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
	{
		if (dwIndex == index)
		{
			pRet = pElem->data;
			break;
		}
	}
	return pRet;
}

// 函数: ElementIndexByID
// 描述: 按元素 ID 在元素链上取链序索引(未找到返回 FALSE)
static BOOL ElementIndexByID(CONST LayoutNode* pNode, int id, DWORD* pIndex)
{
	BOOL bRet = FALSE;
	DWORD dwIndex = 0;
	for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
	{
		CONST ElementSpec* es = pElem->data;
		if (es != NULL && es->id == id)
		{
			if (pIndex != NULL)
			{
				*pIndex = dwIndex;
			}
			bRet = TRUE;
			break;
		}
	}
	return bRet;
}

// 函数: BuildLayoutFromNode
// 描述: 依中间态布局节点构建一枚布局对象(逐元素装配控件信息与类型缓冲)
Layout* LayoutBuilder::BuildLayoutFromNode(CONST LayoutNode* pNode, BOOL IsLink)
{
	Layout* ret = NULL;
	Layout* layout = NULL;
	if (pNode == NULL || pNode->info == NULL || pNode->info->dwElements == 0)
	{
		goto END;
	}
	layout = Layout::CreateLayout(pNode->info->dwElements, IsLink);
	if (layout == NULL)
	{
		goto END;
	}
	{
		DWORD j = 0;
		for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, j++)
		{
			CONST ElementSpec* es = pElem->data;
			if (es == NULL)
			{
				continue;
			}
			CtrlInfo info{};
			if (!LayoutBuilder::BuildCtrlInfoFromSpec(info, es, TRUE))
			{
				CtrlBuffer::FreeCtrlBuffer(&info);
				LayoutBuilder::FreeLayoutCtrlBuffers(*layout);
				Layout::DestroyLayout(layout);
				layout = NULL;
				goto END;
			}
			layout->SetCtrlInfo(j, info);
		}
	}
	layout->DeriveCtrlLayers();
	layout->DeriveCtrlPositions();
	LayoutBuilder::ResolveEntrustedEvents(*layout, pNode);
	ret = layout;
END:
	return ret;
}

// 函数: BuildCtrlInfoFromSpec
// 描述: 依中间态元素规格装配一枚控件信息(控件字段 / 事件绑定 / 类型缓冲; 见 LayoutBuilder.h)
BOOL LayoutBuilder::BuildCtrlInfoFromSpec(CtrlInfo& info, CONST ElementSpec* es, BOOL bBindEvent)
{
	BOOL bRet = FALSE;
	if (es == NULL)
	{
		return bRet;
	}
	info.id = es->id;
	info.parentID = es->parentID;
	info.type = GetTypeEnum(es->type);
	info.label = es->label;
	if (bBindEvent && es->event[0] && CtrlEventIsEntrustParent(es->event) == FALSE)
	{
		EventInfo ev = UIEventHandler::BindEventHandler(es->event);
		info.pEventHandler = ev.pfnEvent;
		if (ev.bufferSize > 0)
		{
			info.bufferInfo = AllocatorHub::AllocBuffer(ev.bufferSize);
			if (info.bufferInfo.buffer)
			{
				RtlZeroMemory(info.bufferInfo.buffer, info.bufferInfo.size);
			}
		}
	}
	if (!CtrlBuffer::PrepareCtrlBuffer(&info, es->text))
	{
		return bRet;
	}
	bRet = TRUE;
	return bRet;
}

// 函数: CopyLayout
// 描述: 深拷贝一枚布局对象(缓冲与标签另建副本, 储存模式随源)
Layout* LayoutBuilder::CopyLayout(Layout& src)
{
	Layout* dst = NULL;
	if (src.IsInvalid() || src.GetTotalNum() == 0)
	{
		return NULL;
	}
	dst = Layout::CreateLayout(src.GetTotalNum(), src.IsLinkMode());
	if (dst == NULL)
	{
		return NULL;
	}
	for (size_t i = 0; i < src.GetTotalNum(); i++)
	{
		CtrlInfo& s = src.GetCtrlInfoRef(i);
		CtrlInfo d = s;
		if (s.bufferInfo.buffer != NULL && s.bufferInfo.size > 0)
		{
			d.bufferInfo = AllocatorHub::AllocSmall(s.bufferInfo.size);
			if (d.bufferInfo.buffer == NULL)
			{
				LayoutBuilder::FreeLayoutCtrlBuffers(*dst);
				Layout::DestroyLayout(dst);
				return NULL;
			}
			memcpy(d.bufferInfo.buffer, s.bufferInfo.buffer, s.bufferInfo.size);
		}
		else
		{
			d.bufferInfo = BufferInfo{ NULL, 0 };
		}
		d.label = (s.label != NULL) ? AllocatorHub::CopyString(s.label) : NULL;
		if (dst->SetCtrlInfo(i, d) == FALSE)
		{
			CtrlBuffer::FreeCtrlBuffer(&d);
			LayoutBuilder::FreeLayoutCtrlBuffers(*dst);
			Layout::DestroyLayout(dst);
			return NULL;
		}
	}
	return dst;
}

// 函数: ResolveEntrustedEvents
// 描述: 解析"委托父容器"的事件绑定(沿父容器 ID 上溯: 委托父继续上溯; 无事件且无处理函数的父即中断)
BOOL LayoutBuilder::ResolveEntrustedEvents(Layout& layout, CONST LayoutNode* pNode)
{
	BOOL bRet = FALSE;
	CONST DWORD dwTotal = (pNode != NULL && pNode->info != NULL) ? pNode->info->dwElements : 0;
	DWORD j = 0;
	if ((dwTotal == 0) || (layout.GetTotalNum() < dwTotal))
	{
		return bRet;
	}
	bRet = TRUE;
	for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, j++)
	{
		CONST ElementSpec* es = pElem->data;
		CtrlInfo* pCtrl = NULL;
		INT nParent = -1;
		DWORD dwGuard = 0;
		if (es == NULL || j >= dwTotal || CtrlEventIsEntrustParent(es->event) == FALSE)
		{
			continue;
		}
		pCtrl = layout.GetCtrlInfo(j);
		if (pCtrl == NULL)
		{
			bRet = FALSE;
			continue;
		}
		pCtrl->pEventHandler = NULL;
		nParent = es->parentID;
		while (nParent >= 0 && dwGuard < dwTotal)
		{
			DWORD dwParentIndex = 0;
			CONST ElementSpec* pParentSpec = NULL;
			CtrlInfo* pParentCtrl = NULL;
			if (ElementIndexByID(pNode, nParent, &dwParentIndex) == FALSE || dwParentIndex >= dwTotal)
			{
				break;
			}
			pParentSpec = ElementSpecAt(pNode, dwParentIndex);
			if (pParentSpec == NULL)
			{
				break;
			}
			if (CtrlEventIsEntrustParent(pParentSpec->event) != FALSE)
			{
				nParent = pParentSpec->parentID;
				dwGuard++;
				continue;
			}
			pParentCtrl = layout.GetCtrlInfo(dwParentIndex);
			if (pParentCtrl == NULL)
			{
				break;
			}
			if (pParentCtrl->pEventHandler == NULL && pParentSpec->event[0] == '\0')
			{
				break;
			}
			pCtrl->pEventHandler = pParentCtrl->pEventHandler;
			break;
		}
		if (pCtrl->pEventHandler == NULL)
		{
			bRet = FALSE;
		}
	}
	return bRet;
}

// 函数: FreeLayoutCtrlBuffers
// 描述: 回收一枚布局内全部控件的自身缓冲(布局对象析构之前调用; 见 LayoutBuilder.h)
VOID LayoutBuilder::FreeLayoutCtrlBuffers(Layout& layout)
{
	if (layout.IsInvalid())
	{
		return;
	}
	CONST size_t qwTotal = layout.GetTotalNum();
	for (size_t i = 0; i < qwTotal; i++)
	{
		CtrlBuffer::FreeCtrlBuffer(layout.GetCtrlInfo(i));
	}
}
