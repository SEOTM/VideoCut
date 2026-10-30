#include <windows.h>
#include <new>
#include <stdio.h>
#include <string.h>

#include "framework/dispatch/Dispatch.h"
#include "framework/base/mem/AllocatorHub.h"
#include "framework/dispatch/ResourceLoader.h"
#include "framework/base/ControlSpec.h"
#include "framework/layout/GeometrySolver.h"
#include "framework/layout/LayoutBuilder.h"
#include "framework/layout/LayoutSolver.h"

constexpr size_t DISPATCH_ARENA_SIZE = 32 * 1024 * 1024;
constexpr size_t ENV_NAME_CAPACITY = 64;
constexpr DWORD DISPATCH_SLOT_INIT = 8;
constexpr DWORD DISPATCH_SLOT_STEP = 8;
constexpr DWORD DISPATCH_MEM_MAX = 8 * 1024 * 1024;

// 函数: ReleaseOwnedLayout
// 描述: 回收一枚归调度层所有的布局对象(控件缓冲回池 → 析构; 不归还分配器内存)
static VOID ReleaseOwnedLayout(Layout* pLayout);

// 函数: ReleaseOwnedLayout
// 描述: 回收一枚归调度层所有的布局对象(控件缓冲回池 → 析构; 不归还分配器内存)
static VOID ReleaseOwnedLayout(Layout* pLayout)
{
	if (pLayout == NULL)
	{
		return;
	}
	LayoutBuilder::FreeLayoutCtrlBuffers(*pLayout);
	Layout::DestroyLayout(pLayout);
}

// 函数: MakeBinName
// 描述: 由文档名派生同名 bin 名(扩展名替换为 .bin; 无扩展名时追加)
static LPCSTR MakeBinName(LPCSTR docName, LPSTR szBin, size_t cbBin)
{
	if (docName == NULL || szBin == NULL || cbBin == 0)
	{
		return NULL;
	}
	size_t cbName = 0;
	while ((docName[cbName] != '\0') && (cbName + 1 < cbBin))
	{
		szBin[cbName] = docName[cbName];
		cbName++;
	}
	if (docName[cbName] != '\0')
	{
		return NULL;
	}
	size_t cbDot = cbName;
	for (size_t i = cbName; i > 0; i--)
	{
		CHAR c = szBin[i - 1];
		if (c == '\\' || c == '/')
		{
			break;
		}
		if (c == '.')
		{
			cbDot = i - 1;
			break;
		}
	}
	CONST CHAR BinExt[] = ".bin";
	if (cbDot + sizeof(BinExt) > cbBin)
	{
		return NULL;
	}
	szBin[cbDot] = '\0';
	for (size_t i = 0; i < sizeof(BinExt); i++)
	{
		szBin[cbDot + i] = BinExt[i];
	}
	return szBin;
}

// 函数: ResolveDocPath
// 描述: 按加载链解析布局文档路径(同名 bin 优先, 其次文档名本身)
static LPCSTR ResolveDocPath(LPCSTR docName)
{
	LPCSTR lpszDoc = NULL;
	CHAR szBin[MAX_PATH]{};
	if (docName == NULL)
	{
		return lpszDoc;
	}
	if (MakeBinName(docName, szBin, sizeof(szBin)) != NULL)
	{
		LPCSTR lpszBin = ResourceLoader::GetFilePath(szBin);
		if (lpszBin != NULL && ResourceLoader::FileExists(lpszBin) != FALSE)
		{
			lpszDoc = lpszBin;
		}
	}
	if (lpszDoc == NULL)
	{
		lpszDoc = ResourceLoader::GetFilePath(docName);
	}
	return lpszDoc;
}

// 函数: ReserveSlots
// 描述: 保证槽数组至少容纳指定数量(不足时按步进扩容; 旧块随会话分配器整体释放)
BOOL Dispatch::ReserveSlots(DWORD dwCapacity)
{
	BOOL bRet = FALSE;
	DWORD dwWant = 0;
	BufferInfo bufSlots{};
	LayoutSlot* pNew = NULL;
	if (data == NULL || dwCapacity == 0)
	{
		return bRet;
	}
	if (dwCapacity <= data->dwSlotCapacity)
	{
		return TRUE;
	}
	if (dwCapacity > DISPATCH_SLOT_MAX)
	{
		return bRet;
	}
	dwWant = (data->dwSlotCapacity > 0) ? data->dwSlotCapacity : DISPATCH_SLOT_INIT;
	while (dwWant < dwCapacity)
	{
		dwWant += DISPATCH_SLOT_STEP;
	}
	if (dwWant > DISPATCH_SLOT_MAX)
	{
		dwWant = DISPATCH_SLOT_MAX;
	}
	bufSlots = AllocatorHub::GetArena()->Allocate(sizeof(LayoutSlot) * dwWant);
	if (bufSlots.buffer == NULL || bufSlots.size < sizeof(LayoutSlot) * dwWant)
	{
		return bRet;
	}
	pNew = (LayoutSlot*)bufSlots.buffer;
	for (DWORD i = 0; i < dwWant; i++)
	{
		pNew[i] = LayoutSlot{};
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		pNew[i] = data->pSlots[i];
	}
	data->pSlots = pNew;
	data->dwSlotCapacity = dwWant;
	bRet = TRUE;
	return bRet;
}

// 函数: ResetSlots
// 描述: 清空布局槽(归调度层所有的布局对象先回收控件缓冲再析构; 槽数归零)
BOOL Dispatch::ResetSlots()
{
	if (data == NULL)
	{
		return FALSE;
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		LayoutSlot& slot = data->pSlots[i];
		if (slot.pLayout != NULL && slot.bOwned != FALSE)
		{
			ReleaseOwnedLayout(slot.pLayout);
		}
		slot.pLayout = NULL;
		slot.bVisible = FALSE;
		slot.bNested = FALSE;
		slot.bOwned = FALSE;
	}
	data->dwSlotCount = 0;
	data->bReady = FALSE;
	return TRUE;
}

// 函数: AppendSlot
// 描述: 在槽尾追加一枚布局(容量不足时先扩容)
BOOL Dispatch::AppendSlot(Layout* pLayout, CONST AutoSize& ref, BOOL bOwned, BOOL bVisible, BOOL bNested)
{
	if (data == NULL || pLayout == NULL)
	{
		return FALSE;
	}
	if (data->dwSlotCount >= DISPATCH_SLOT_MAX)
	{
		Log("布局提交失败: 已达槽位上限(%u 枚)\n", (unsigned)DISPATCH_SLOT_MAX);
		return FALSE;
	}
	if (!ReserveSlots(data->dwSlotCount + 1))
	{
		Log("布局提交失败: 布局槽扩容失败\n");
		return FALSE;
	}
	{
		LayoutSlot& slot = data->pSlots[data->dwSlotCount];
		slot.pLayout = pLayout;
		slot.ref = ref;
		slot.bVisible = (bVisible != FALSE) ? TRUE : FALSE;
		slot.bNested = (bNested != FALSE) ? TRUE : FALSE;
		slot.bOwned = (bOwned != FALSE) ? TRUE : FALSE;
	}
	data->dwSlotCount++;
	data->bReady = TRUE;
	return TRUE;
}

// 函数: UseLayouts
// 描述: 整体替换布局槽(先清空), 逐枚接管调用方给出的布局对象(不取得所有权)
BOOL Dispatch::UseLayouts(Layout* CONST* ppLayouts, CONST AutoSize* pRefs, DWORD dwCount, BOOL bFirstOnly)
{
	DWORD dwValid = 0;
	DWORD dwWndW = 0;
	DWORD dwWndH = 0;
	if (data == NULL || ppLayouts == NULL || pRefs == NULL)
	{
		return FALSE;
	}
	if (dwCount == 0 || dwCount > DISPATCH_SLOT_MAX)
	{
		Log("布局提交失败: 数量非法(%u, 允许 1..%u)\n", (unsigned)dwCount, (unsigned)DISPATCH_SLOT_MAX);
		return FALSE;
	}
	for (DWORD i = 0; i < dwCount; i++)
	{
		if (ppLayouts[i] == NULL || ppLayouts[i]->GetTotalNum() == 0)
		{
			Log("布局提交失败: 第 %u 枚为空(空指针或零控件)\n", (unsigned)i);
			return FALSE;
		}
		for (DWORD j = 0; j < data->dwSlotCount; j++)
		{
			if (data->pSlots[j].bOwned != FALSE && data->pSlots[j].pLayout == ppLayouts[i])
			{
				Log("布局提交失败: 第 %u 枚与现有归属槽重叠(重置会析构该对象)\n", (unsigned)i);
				return FALSE;
			}
		}
	}
	if (!ResetSlots())
	{
		return FALSE;
	}
	for (DWORD i = 0; i < dwCount; i++)
	{
		CONST BOOL bVisible = ((bFirstOnly != FALSE) && (i > 0)) ? FALSE : TRUE;
		if (!AppendSlot(ppLayouts[i], pRefs[i], FALSE, bVisible, FALSE))
		{
			ResetSlots();
			return FALSE;
		}
	}
	{
		CtrlInfo* pWnd = ppLayouts[0]->GetLayoutBase(CTRL_TYPE_WND);
		if (pWnd != NULL && pWnd->size.x > 0.0f && pWnd->size.y > 0.0f)
		{
			dwWndW = (DWORD)pWnd->size.x;
			dwWndH = (DWORD)pWnd->size.y;
		}
		if (dwWndW == 0 || dwWndH == 0)
		{
			dwWndW = pRefs[0].GetWindowWidth<DWORD>();
			dwWndH = pRefs[0].GetWindowHeight<DWORD>();
		}
		if (dwWndW < 16) { dwWndW = 16; }
		if (dwWndH < 16) { dwWndH = 16; }
		data->dwWndW = dwWndW;
		data->dwWndH = dwWndH;
	}
	for (DWORD i = 0; i < dwCount; i++)
	{
		if (ppLayouts[i]->IsInvalid() == FALSE)
		{
			dwValid++;
		}
	}
	Log("布局校验: %u/%u 枚结构件齐备(无效布局在渲染期被跳过)\n", (unsigned)dwValid, (unsigned)dwCount);
	return TRUE;
}

// 函数: ApplyWndGeometry
// 描述: 按布局参考参数推导首枚窗口几何与该布局的参考尺寸(AutoSize)
BOOL Dispatch::ApplyWndGeometry(CONST RefSize& refSize, AutoSize& refOut, DWORD* pdwWndW, DWORD* pdwWndH)
{
	DWORD dwWndW = 0;
	DWORD dwWndH = 0;
	DWORD dwBase = 0;
	INT nMode = 0;
	nMode = GeometrySolver::AutoModeIndex(refSize.mode);
	dwBase = GeometrySolver::EntryBasePx(nMode);
	if (dwBase < 1)
	{
		dwBase = 1;
	}
	dwWndW = (DWORD)((unsigned __int64)dwBase * (DWORD)refSize.width / 1000);
	dwWndH = (DWORD)((unsigned __int64)dwBase * (DWORD)refSize.height / 1000);
	if (dwWndW < 16) { dwWndW = 16; }
	if (dwWndH < 16) { dwWndH = 16; }
	{
		AutoSize::SizeInfo<DWORD> si = GeometrySolver::EntryRefSizeInfo(refSize, dwBase, dwWndW, dwWndH);
		refOut = AutoSize::CreateByFixedSize(si, (AutoSize::AutoSizeMode)nMode);
	}
	if (pdwWndW != NULL) { *pdwWndW = dwWndW; }
	if (pdwWndH != NULL) { *pdwWndH = dwWndH; }
	return TRUE;
}

// 函数: BuildFromModel
// 描述: 从布局模型逐枚展开布局对象并解算几何(前 dwWant 枚; 产物归调度层所有)
BOOL Dispatch::BuildFromModel(CONST FileMidState& state, DWORD dwWant)
{
	BOOL bRet = FALSE;
	DWORD dwTotal = 0;
	DWORD dwUse = 0;
	if (data == NULL)
	{
		return bRet;
	}
	dwTotal = LayoutModel::LayoutCount(state);
	if (dwTotal == 0)
	{
		Log("场景装配失败: 布局模型不含布局\n");
		goto END;
	}
	dwUse = (dwTotal < dwWant) ? dwTotal : dwWant;
	if (dwUse < dwTotal)
	{
		Log("布局模型: %u 枚布局, 本会话仅装配前 %u 枚(受窗口数 %u 限制)\n",
			(unsigned)dwTotal, (unsigned)dwUse, (unsigned)dwWant);
	}
	for (DWORD i = 0; i < dwUse; i++)
	{
		LayoutNode* pNode = LayoutModel::LayoutAt(state, i);
		Layout* pLayout = NULL;
		AutoSize ref;
		if (pNode == NULL || pNode->info == NULL || pNode->info->dwElements == 0)
		{
			Log("场景装配失败: 第 %u 枚布局不含可用布局信息\n", (unsigned)i);
			goto END;
		}
		if (!ApplyWndGeometry(pNode->info->refSize, ref,
			(i == 0) ? &data->dwWndW : NULL, (i == 0) ? &data->dwWndH : NULL))
		{
			goto END;
		}
		if (i == 0)
		{
			data->refSize = pNode->info->refSize;
		}
		pLayout = LayoutBuilder::BuildLayoutFromNode(pNode, FALSE);
		if (pLayout == NULL)
		{
			Log("场景装配失败: 第 %u 枚布局对象展开失败(含不支持控件或分配失败)\n", (unsigned)i);
			goto END;
		}
		{
			AutoSize::SizeInfo<FLOAT> refF = ref.GetSizeInfo<FLOAT>();
			DWORD dwIndex = 0;
			for (CONST ElementNode* pElem = pNode->elements; pElem != NULL; pElem = pElem->next, dwIndex++)
			{
				CtrlInfo* pCtrl = pLayout->GetCtrlInfo(dwIndex);
				if (pCtrl != NULL && pElem->data != NULL)
				{
					GeometrySolver::ApplySpecGeometry(*pCtrl, pElem->data, refF);
				}
			}
		}
		pLayout->DeriveCtrlPositions();
		{
			CtrlInfo* pWnd = pLayout->GetLayoutBase(CTRL_TYPE_WND);
			GeometrySolver::ResolveWndPos(pWnd);
		}
		if (!AppendSlot(pLayout, ref, TRUE, TRUE, FALSE))
		{
			ReleaseOwnedLayout(pLayout);
			goto END;
		}
	}
	bRet = TRUE;
END:
	return bRet;
}

// 函数: CountTotalCtrls
// 描述: 统计全部布局槽的控件数(自证用)
DWORD Dispatch::CountTotalCtrls()
{
	DWORD dwTotal = 0;
	if (data == NULL)
	{
		return dwTotal;
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		if (data->pSlots[i].pLayout != NULL)
		{
			dwTotal += (DWORD)data->pSlots[i].pLayout->GetTotalNum();
		}
	}
	return dwTotal;
}

// 函数: CountBoundCtrls
// 描述: 统计全部布局槽内已绑定事件处理器的控件数(自证用)
DWORD Dispatch::CountBoundCtrls()
{
	DWORD dwBound = 0;
	if (data == NULL)
	{
		return dwBound;
	}
	for (DWORD i = 0; i < data->dwSlotCount; i++)
	{
		Layout* pLayout = data->pSlots[i].pLayout;
		if (pLayout == NULL)
		{
			continue;
		}
		for (size_t j = 0; j < pLayout->GetTotalNum(); j++)
		{
			CtrlInfo* pCtrl = pLayout->GetCtrlInfo(j);
			if (pCtrl != NULL && pCtrl->pEventHandler != NULL)
			{
				dwBound++;
			}
		}
	}
	return dwBound;
}

// 函数: LoadScene
// 描述: 建立会话分配器与成员块, 并按布局来源装配场景(缺省加载链 / 内存缓冲 / 布局模型 /
BOOL Dispatch::LoadScene(CONST DispatchConfig& cfg, CONST LayoutSource& source)
{
	BOOL bRet = FALSE;
	Arena* arena = NULL;
	MemPool* pool = NULL;
	BufferInfo bufData{};
	BufferInfo bufSolver{};
	BufferInfo bufMem{};
	LPCSTR lpszDocPath = NULL;
	CONST FileMidState* pModel = NULL;

	if (AllocatorHub::GetArena() == NULL && !AllocatorHub::Init(DISPATCH_ARENA_SIZE))
	{
		Log("场景载入失败: 会话分配器建立失败\n");
		goto END;
	}
	arena = AllocatorHub::GetArena();
	pool = AllocatorHub::GetSmallPool();
	bufData = arena->Allocate(sizeof(DispatchData));
	if (bufData.buffer == NULL || bufData.size < sizeof(DispatchData))
	{
		Log("场景载入失败: 成员块分配失败\n");
		goto END;
	}
	data = new (bufData.buffer) DispatchData();
	if (cfg.szDocName != NULL)
	{
		lstrcpynA(data->szDocName, cfg.szDocName, MAX_PATH);
	}
	if (cfg.szDocEnv != NULL)
	{
		lstrcpynA(data->szDocEnv, cfg.szDocEnv, (INT)ENV_NAME_CAPACITY);
	}
	if (cfg.szAutoCloseEnv != NULL)
	{
		lstrcpynA(data->szAutoCloseEnv, cfg.szAutoCloseEnv, (INT)ENV_NAME_CAPACITY);
	}
	data->dwWndCount = (cfg.dwWndCount > 0) ? cfg.dwWndCount : 1;
	data->bDarkFollowSystem = (cfg.bDarkFollowSystem != FALSE) ? TRUE : FALSE;
	data->bDarkMode = FALSE;
	LayoutModel::SetAllocators(arena, pool);
	Layout::SetAllocators(arena, pool);
	ProjCR::SetAllocators(arena, pool);
	{
		CHAR szEnvDoc[MAX_PATH]{};
		DWORD dwDocLen = 0;
		if (data->szDocEnv[0] != '\0')
		{
			dwDocLen = GetEnvironmentVariableA(data->szDocEnv, szEnvDoc, (DWORD)MAX_PATH);
		}
		if (dwDocLen > 0 && dwDocLen < (DWORD)MAX_PATH)
		{
			lstrcpynA(data->szDocName, szEnvDoc, MAX_PATH);
		}
	}
	ResourceLoader::Init();
	if (!ReserveSlots(DISPATCH_SLOT_INIT))
	{
		Log("场景载入失败: 布局槽分配失败\n");
		goto END;
	}
	switch (source.kind)
	{
	case LAYOUT_SOURCE_DEFAULT:
		{
			Log("布局来源: 缺省加载链\n");
			lpszDocPath = ResolveDocPath(data->szDocName);
			if (lpszDocPath == NULL)
			{
				Log("场景载入失败: 未能解析程序所在目录\n");
				goto END;
			}
			bufSolver = arena->Allocate(sizeof(LayoutSolver));
			if (bufSolver.buffer == NULL)
			{
				Log("场景载入失败: 解析器分配失败\n");
				goto END;
			}
			data->pSolver = new (bufSolver.buffer) LayoutSolver(arena);
			if (!data->pSolver->LoadLayoutsAuto(lpszDocPath))
			{
				Log("场景载入失败: 文档解析失败(%s)\n", lpszDocPath);
				goto END;
			}
			pModel = &data->pSolver->GetState();
		}
		break;
	case LAYOUT_SOURCE_MEMORY:
		{
			BOOL bParsed = FALSE;
			Log("布局来源: 内存缓冲(%u 字节)\n", (unsigned)source.cbMemSize);
			if (source.pMemData != NULL && source.cbMemSize > 0 && source.cbMemSize <= DISPATCH_MEM_MAX)
			{
				bufMem = arena->Allocate((size_t)source.cbMemSize + 1);
				if (bufMem.buffer != NULL && bufMem.size >= (size_t)source.cbMemSize + 1)
				{
					bufSolver = arena->Allocate(sizeof(LayoutSolver));
					if (bufSolver.buffer != NULL)
					{
						data->pSolver = new (bufSolver.buffer) LayoutSolver(arena);
						memcpy(bufMem.buffer, source.pMemData, (size_t)source.cbMemSize);
						((LPSTR)bufMem.buffer)[source.cbMemSize] = '\0';
						bParsed = data->pSolver->LoadLayoutsAuto(bufMem.buffer, source.cbMemSize);
					}
				}
			}
			if (!bParsed)
			{
				Log("内存缓冲解析失败: 回退缺省加载链\n");
				lpszDocPath = ResolveDocPath(data->szDocName);
				if (lpszDocPath == NULL)
				{
					Log("场景载入失败: 未能解析程序所在目录\n");
					goto END;
				}
				if (data->pSolver == NULL)
				{
					bufSolver = arena->Allocate(sizeof(LayoutSolver));
					if (bufSolver.buffer == NULL)
					{
						Log("场景载入失败: 解析器分配失败\n");
						goto END;
					}
					data->pSolver = new (bufSolver.buffer) LayoutSolver(arena);
				}
				if (!data->pSolver->LoadLayoutsAuto(lpszDocPath))
				{
					Log("场景载入失败: 文档解析失败(%s)\n", lpszDocPath);
					goto END;
				}
			}
			else
			{
				lpszDocPath = "(内存缓冲)";
			}
			pModel = &data->pSolver->GetState();
		}
		break;
	case LAYOUT_SOURCE_MODEL:
		{
			Log("布局来源: 布局模型\n");
			if (source.pModel == NULL || LayoutModel::LayoutCount(*source.pModel) == 0)
			{
				Log("场景载入失败: 布局模型为空\n");
				goto END;
			}
			pModel = source.pModel;
			lpszDocPath = "(布局模型)";
		}
		break;
	case LAYOUT_SOURCE_LAYOUTS:
		{
			Log("布局来源: 布局对象数组(%u 枚)\n", (unsigned)source.dwLayoutCount);
			if (!UseLayouts(source.ppLayouts, source.pRefs, source.dwLayoutCount, TRUE))
			{
				Log("场景载入失败: 布局对象数组不可用\n");
				goto END;
			}
			lpszDocPath = "(布局对象数组)";
		}
		break;
	case LAYOUT_SOURCE_DEFERRED:
		{
			Log("布局来源: 延迟装配(待业务代码提交)\n");
			lpszDocPath = "(延迟装配)";
		}
		break;
	default:
		{
			Log("场景载入失败: 未知的布局来源(%d)\n", (int)source.kind);
			goto END;
		}
	}
	if (pModel != NULL)
	{
		if (!BuildFromModel(*pModel, data->dwWndCount))
		{
			goto END;
		}
	}
	if (data->dwSlotCount == 0)
	{
		bRet = TRUE;
		goto END;
	}
	{
		Layout* pFirst = data->pSlots[0].pLayout;
		LPCSTR lpszLayoutName = "(布局对象)";
		if (pModel != NULL)
		{
			CONST LayoutNode* pNode = LayoutModel::LayoutAt(*pModel, 0);
			if (pNode != NULL && pNode->info != NULL)
			{
				lpszLayoutName = pNode->info->name;
			}
		}
		Log("场景载入: %s (布局=%s 控件=%zu 窗口=%ux%u)\n",
			(lpszDocPath != NULL) ? lpszDocPath : "(未知)",
			(lpszLayoutName != NULL) ? lpszLayoutName : "(未命名)",
			(size_t)CountTotalCtrls(), data->dwWndW, data->dwWndH);
	}
	{
		DWORD dwBound = 0;
		DWORD dwEntrust = 0;
		DWORD dwResolved = 0;
		Layout* pFirst = data->pSlots[0].pLayout;
		for (size_t i = 0; i < pFirst->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = pFirst->GetCtrlInfo(i);
			CONST ElementSpec* pSpec = (pModel != NULL)
				? LayoutModel::ElementAt(*pModel, 0, (DWORD)i) : NULL;
			if (pCtrl == NULL)
			{
				continue;
			}
			if (pCtrl->pEventHandler != NULL)
			{
				dwBound++;
			}
			if (pSpec != NULL && CtrlEventIsEntrustParent(pSpec->event) != FALSE)
			{
				dwEntrust++;
				if (pCtrl->pEventHandler != NULL)
				{
					dwResolved++;
				}
			}
		}
		if (pModel == NULL)
		{
			Log("事件绑定: %u 枚控件已绑定处理器(无布局模型, 委托父容器计数不适用)\n", (unsigned)dwBound);
		}
		else
		{
			Log("事件绑定: %u 枚控件已绑定处理器(其中委托父容器 %u 枚, 已解析 %u 枚)\n",
				(unsigned)dwBound, (unsigned)dwEntrust, (unsigned)dwResolved);
		}
		for (DWORD s = 0; s < data->dwSlotCount; s++)
		{
			Layout* pSlot = data->pSlots[s].pLayout;
			DWORD dwSlotBound = 0;
			if (pSlot != NULL)
			{
				for (size_t k = 0; k < pSlot->GetTotalNum(); k++)
				{
					CtrlInfo* pSlotCtrl = pSlot->GetCtrlInfo(k);
					if (pSlotCtrl != NULL && pSlotCtrl->pEventHandler != NULL)
					{
						dwSlotBound++;
					}
				}
				Log("布局槽 %u: 控件=%zu 绑定=%u\n", (unsigned)s, pSlot->GetTotalNum(), (unsigned)dwSlotBound);
			}
		}
	}
	{
		Layout* pFirst = data->pSlots[0].pLayout;
		Log("控件层次:");
		for (size_t i = 0; i < pFirst->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = pFirst->GetCtrlInfo(i);
			printf(" %d=%d", (pCtrl != NULL) ? pCtrl->id : -1, (pCtrl != NULL) ? (int)pCtrl->nLayer : 0);
		}
		printf("\n");
	}
	{
		Layout* pFirst = data->pSlots[0].pLayout;
		INT nOffset = 0;
		Log("控件坐标:");
		for (size_t i = 0; i < pFirst->GetTotalNum(); i++)
		{
			CtrlInfo* pCtrl = pFirst->GetCtrlInfo(i);
			if (pCtrl == NULL)
			{
				continue;
			}
			if ((pCtrl->position - pCtrl->posDeclared) != vec2(0.0f, 0.0f))
			{
				nOffset++;
			}
			printf(" %d=(%d,%d)", pCtrl->id, (int)pCtrl->position.x, (int)pCtrl->position.y);
		}
		printf("(其中含父链偏移 %d 枚)\n", (int)nOffset);
		fflush(stdout);
	}
	bRet = TRUE;

END:
	return bRet;
}

// 函数: UnloadScene
// 描述: 释放场景资源(布局槽与布局对象 → 解析器 → 成员块显式析构 → 领域分配器解绑 → 会话分配器)
VOID Dispatch::UnloadScene()
{
	if (data != NULL)
	{
		ResetSlots();
		data->pSlots = NULL;
		data->dwSlotCapacity = 0;
	}
	if (data != NULL && data->pSolver != NULL)
	{
		data->pSolver->~LayoutSolver();
		data->pSolver = NULL;
	}
	if (data != NULL)
	{
		data->bReady = FALSE;
	}
	Layout::SetAllocators(NULL, NULL);
	LayoutModel::SetAllocators(NULL, NULL);
	ProjCR::SetAllocators(NULL, NULL);
	if (data != NULL)
	{
		DispatchData* pData = data;
		data = NULL;
		pData->~DispatchData();
	}
	AllocatorHub::Release();
}
