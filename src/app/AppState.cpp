#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <new>
#include <thread>
#include <functional>

#include "AppState.h"
#include "AppRender.h"
#include "Identity.h"
#include "MediaFileUtils.h"
#include "MediaOrganizer.h"
#include "VideoStream.h"

#include "framework/dispatch/Dispatch.h"
#include "framework/base/CtrlBuffer.h"
#include "framework/base/mem/AllocatorHub.h"
#include "framework/render/imgui/ImGuiHost.h"

// ================================
// 常量定义区
// ================================
constexpr DWORD LIST_ITEMS = 126;					// 剪辑片段容量
constexpr DWORD FOLDER_BUFFER = MAX_PATH * 3;		// 源路径缓冲（UTF-8）
constexpr DWORD LAYOUT_EDITOR = 0;					// 编辑布局槽序号（基座：承载窗口）
constexpr DWORD LAYOUT_PREPARE = 1;					// 准备布局槽序号（内嵌(flex)：沿用编辑窗口）
constexpr DOUBLE SPEED_MIN = 1.0 / 4.0;				// 倍速下限
constexpr DOUBLE SPEED_MAX = 32.0 / 1.0;			// 倍速上限

// ================================
// 函数声明区
// ================================
static INT LayoutIndexOfTitle(VOID* obj);

// ================================
// 结构定义区
// ================================

// 结构: AppSpan
// 描述: 一枚剪辑片段（毫秒）
struct AppSpan
{
	int64_t left;						// 起点（毫秒）
	int64_t right;						// 终点（毫秒）
};

// ================================
// 类名: AppState
// 描述: VideoCut 业务状态与界面联动
// ================================

// 结构: AppData
// 描述: 业务状态集中块
struct AppState::AppData
{
	MediaOrganizer* organizer;			// 当前媒体组织器
	VideoStream* stream;				// 当前视频流
	CHAR szFolder[MAX_PATH * 3];		// 当前操作目录（UTF-8）
	CHAR szOutput[MAX_PATH * 3];		// 重混输出路径（UTF-8）
	DWORD dwGroup;						// 当前视频分组索引
	DWORD dwThumb;						// 活动端：最近拖动的滑块手柄（0 左 / 1 右；播放推进时该端跟随播放头）
	int64_t beginMs;					// 起止起点（毫秒）
	int64_t endMs;						// 起止终点（毫秒）
	int64_t durationMs;					// 视频总时长（毫秒）
	int64_t curTimeMs;					// 当前预览时间（毫秒）
	int64_t curTimeUs;					// 当前预览时间（微秒）
	DOUBLE speed;						// 播放与导出倍速
	BOOL IsPlaying;						// 播放状态
	DWORD dwLayout;						// 当前布局槽序号
	BOOL IsFlexDrag;					// 内嵌(flex)布局的标题栏拖动中
	DWORD dwFlexDragLayout;				// 拖动中的内嵌布局槽序号
	vec2 flexGrab;						// 内嵌布局的抓取偏移（鼠标客户区坐标 - 面板位置）

	std::thread worker;					// 重混工作线程
	VOID* pRemixClips;					// 传给工作线程的片段块
	int64_t remixCur;					// 重混当前进度（微秒）
	int64_t remixExpect;				// 重混预期总量（微秒）
	BOOL IsRemixing;					// 重混进行中标记

	ULONGLONG qwLastTick;				// 上一帧时间戳
	ULONGLONG qwNextTick;				// 下一帧时间戳
	ULONGLONG qwPlayFix;				// 帧间隔补偿
	DWORD dwPlayCount;					// 已播放帧计数
};

AppState::AppData* AppState::data = NULL;

// 函数: LayoutIndexOfTitle
// 描述: 由触发的标题控件反查所属布局槽序号(未命中返回 -1)
static INT LayoutIndexOfTitle(VOID* obj)
{
	INT nRet = -1;
	DWORD dwCount = Dispatch::GetLayoutCount();
	if (obj == NULL)
	{
		return nRet;
	}
	for (DWORD i = 0; i < dwCount && nRet < 0; i++)
	{
		if (Dispatch::GetLayoutAt(i).GetLayoutBase(CTRL_TYPE_TITLE) == (CtrlInfo*)obj)
		{
			nRet = (INT)i;
		}
	}
	return nRet;
}

// 函数: FindCtrl
// 描述: 按布局槽序号与控件标签查找控件(见 .h)
CtrlInfo* AppState::FindCtrl(DWORD index, LPCSTR label)
{
	if (label == NULL)
	{
		return NULL;
	}
	Layout& layout = Dispatch::GetLayoutAt(index);
	for (size_t i = 0; i < layout.GetTotalNum(); i++)
	{
		CtrlInfo* pCtrl = layout.GetCtrlInfo(i);
		if (pCtrl != NULL && pCtrl->label != NULL && strcmp(pCtrl->label, label) == 0)
		{
			return pCtrl;
		}
	}
	return NULL;
}

// 函数: BindBuffers
// 描述: 为框架未分配缓冲的控件补缓冲(见 .h)
BOOL AppState::BindBuffers()
{
	BOOL bRet = TRUE;
	// 1. 路径输入: 框架默认 256 字节, 不足以容纳 MAX_PATH * 3 的 UTF-8 路径
	{
		CtrlInfo* pInput = FindCtrl(LAYOUT_PREPARE, "##INPUT_SRC");
		if (pInput != NULL)
		{
			CtrlBuffer::FreeCtrlBuffer(pInput);
			pInput->bufferInfo = AllocatorHub::AllocSmall(FOLDER_BUFFER);
			if (pInput->bufferInfo.buffer == NULL)
			{
				bRet = FALSE;
			}
			else
			{
				RtlZeroMemory(pInput->bufferInfo.buffer, pInput->bufferInfo.size);
			}
		}
	}
	// 2. 进度初值: 隐藏
	{
		CtrlInfo* pProg = FindCtrl(LAYOUT_EDITOR, "##PROG_REMIX");
		if (pProg != NULL && pProg->bufferInfo.buffer != NULL &&
			pProg->bufferInfo.size >= sizeof(CtrlProgData))
		{
			CtrlProgData* pData = (CtrlProgData*)pProg->bufferInfo.buffer;
			pData->current = 0;
			pData->expect = 1;
			pData->IsVisible = FALSE;
		}
	}
	// 3. 倍速初值
	{
		CtrlInfo* pSpeed = FindCtrl(LAYOUT_EDITOR, "##INPUT_SPEED");
		if (pSpeed != NULL && pSpeed->bufferInfo.buffer != NULL &&
			pSpeed->bufferInfo.size >= sizeof(DOUBLE))
		{
			*(DOUBLE*)pSpeed->bufferInfo.buffer = 1.0;
		}
	}
	// 4. 双手柄范围滑块初值
	{
		CtrlInfo* pSlider = FindCtrl(LAYOUT_EDITOR, "##TIMESPANSLIDER");
		if (pSlider != NULL && pSlider->bufferInfo.buffer != NULL &&
			pSlider->bufferInfo.size >= sizeof(CtrlSliderRangeData))
		{
			CtrlSliderRangeData* pData = (CtrlSliderRangeData*)pSlider->bufferInfo.buffer;
			pData->vMin = 0;
			pData->vMax = 0;
			pData->lMin = 0;
			pData->lMax = 1;
		}
	}
	// 5. 播放按钮文本
	SetPlayText(FALSE);
	return bRet;
}

// 函数: SwitchLayout
// 描述: 切换当前业务界面: 编辑布局是基座(恒可见), 准备布局按内嵌(flex)成员可见性显隐(见 .h)
BOOL AppState::SwitchLayout(DWORD index)
{
	if (data == NULL)
	{
		return FALSE;
	}
	if (index >= Dispatch::GetLayoutCount())
	{
		printf("[VIDEOCUT] 布局切换失败: 槽 %u 未装配\n", (unsigned)index);
		return FALSE;
	}
	// 编辑界面是基座: 内嵌(flex)必须沿用它的窗口段, 因此它始终可见; 准备界面按需显示
	Dispatch::SetLayoutVisible(LAYOUT_EDITOR, TRUE);
	Dispatch::SetLayoutVisible(LAYOUT_PREPARE, (index == LAYOUT_PREPARE) ? TRUE : FALSE);
	data->IsFlexDrag = FALSE;			// 切换界面时结束可能残留的内嵌拖动状态
	data->dwLayout = index;
	return TRUE;
}

// 函数: IsEditorActive
// 描述: 编辑界面当前是否可操作(见 .h)
BOOL AppState::IsEditorActive()
{
	return (data != NULL && data->dwLayout == LAYOUT_EDITOR) ? TRUE : FALSE;
}

// 函数: SetPlayText
// 描述: 设置播放按钮文本(见 .h)
VOID AppState::SetPlayText(BOOL IsPlaying)
{
	CtrlInfo* pPlay = FindCtrl(LAYOUT_EDITOR, "##PLAY");
	if (pPlay == NULL || pPlay->bufferInfo.buffer == NULL)
	{
		return;
	}
	strcpy_s((LPSTR)pPlay->bufferInfo.buffer, pPlay->bufferInfo.size,
		(IsPlaying != FALSE) ? u8"暂停预览" : u8"播放预览");
}

// 函数: SyncSeek
// 描述: 把起止时间写回双手柄范围滑块(见 .h)
VOID AppState::SyncSeek()
{
	if (data == NULL)
	{
		return;
	}
	int64_t limit = (data->durationMs > 0) ? data->durationMs : 1;
	CtrlInfo* pSlider = FindCtrl(LAYOUT_EDITOR, "##TIMESPANSLIDER");
	if (pSlider != NULL && pSlider->bufferInfo.buffer != NULL &&
		pSlider->bufferInfo.size >= sizeof(CtrlSliderRangeData))
	{
		CtrlSliderRangeData* pData = (CtrlSliderRangeData*)pSlider->bufferInfo.buffer;
		pData->vMin = data->beginMs;
		pData->vMax = data->endMs;
		pData->lMin = 0;
		pData->lMax = limit;
	}
}

// 函数: SyncTimeControls
// 描述: 把起止时间写回双手柄滑块与两个可编辑时间条(见 .h)
VOID AppState::SyncTimeControls()
{
	if (data == NULL)
	{
		return;
	}
	// 1. 双手柄范围滑块
	SyncSeek();
	// 2. 可编辑时间条（起点 / 终点毫秒 + 上限 = 视频时长）
	{
		const LPCSTR Bars[2] = { "##TIMEBAR_BEGIN", "##TIMEBAR_END" };
		const int64_t Values[2] = { data->beginMs, data->endMs };
		const int64_t limit = (data->durationMs > 0) ? data->durationMs : 0;
		for (int i = 0; i < 2; i++)
		{
			CtrlInfo* pBar = FindCtrl(LAYOUT_EDITOR, Bars[i]);
			if (pBar != NULL && pBar->bufferInfo.buffer != NULL &&
				pBar->bufferInfo.size >= sizeof(CtrlTimeBarData))
			{
				CtrlTimeBarData* pData = (CtrlTimeBarData*)pBar->bufferInfo.buffer;
				pData->valueMs = (Values[i] > 0) ? Values[i] : 0;
				pData->limitMs = limit;
			}
		}
	}
}

// 函数: SyncProgress
// 描述: 把重混进度写回进度控件缓冲(见 .h)
VOID AppState::SyncProgress()
{
	if (data == NULL)
	{
		return;
	}
	CtrlInfo* pProg = FindCtrl(LAYOUT_EDITOR, "##PROG_REMIX");
	if (pProg == NULL || pProg->bufferInfo.buffer == NULL ||
		pProg->bufferInfo.size < sizeof(CtrlProgData))
	{
		return;
	}
	CtrlProgData* pData = (CtrlProgData*)pProg->bufferInfo.buffer;
	pData->current = data->remixCur;
	pData->expect = (data->remixExpect > 0) ? data->remixExpect : 1;
	pData->IsVisible = data->IsRemixing;
}

// 函数: RefreshUi
// 描述: 把视频时长与初值写回控件缓冲(见 .h)
VOID AppState::RefreshUi()
{
	if (data == NULL)
	{
		return;
	}
	// 1. 渲染面: 有设备时传纹理, 无设备时传空(控件画占位面)
	{
		CtrlInfo* pRender = FindCtrl(LAYOUT_EDITOR, "##VIDEORENDER");
		if (pRender != NULL && pRender->bufferInfo.buffer != NULL &&
			pRender->bufferInfo.size >= sizeof(CtrlRenderData))
		{
			CtrlRenderData* pData = (CtrlRenderData*)pRender->bufferInfo.buffer;
			pData->pTexture = NULL;
			pData->texWidth = 0;
			pData->texHeight = 0;
			if (data->stream != NULL)
			{
				SIZE size = data->stream->GetResolution();
				pData->pTexture = (VOID*)data->stream->GetSRV();
				pData->texWidth = (INT32)size.cx;
				pData->texHeight = (INT32)size.cy;
			}
		}
	}
	// 2. 时间控件
	SyncTimeControls();
	// 3. 进度与按钮
	SyncProgress();
	SetPlayText(data->IsPlaying);
}

// 函数: ReleaseMedia
// 描述: 释放视频流、文件组织器与组合框选项(见 .h)
VOID AppState::ReleaseMedia()
{
	if (data == NULL)
	{
		return;
	}
	if (data->stream != NULL)
	{
		delete data->stream;
		data->stream = NULL;
	}
	if (data->organizer != NULL)
	{
		delete data->organizer;
		data->organizer = NULL;
	}
	{
		CtrlInfo* pCombo = FindCtrl(LAYOUT_EDITOR, "##COMBO_VIDEO");
		if (pCombo != NULL && pCombo->bufferInfo.buffer != NULL &&
			pCombo->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* pData = (CtrlComboData*)pCombo->bufferInfo.buffer;
			if (pData->items != NULL)
			{
				AllocatorHub::FreeSmall(BufferInfo{ (VOID*)pData->items,
					sizeof(LPCSTR) * (size_t)((pData->count > 0) ? pData->count : 1) });
			}
			pData->items = NULL;
			pData->count = 0;
			pData->index = 0;
		}
	}
	// 渲染面缓冲清空(帧已随后端释放)
	{
		CtrlInfo* pRender = FindCtrl(LAYOUT_EDITOR, "##VIDEORENDER");
		if (pRender != NULL && pRender->bufferInfo.buffer != NULL &&
			pRender->bufferInfo.size >= sizeof(CtrlRenderData))
		{
			CtrlRenderData* pData = (CtrlRenderData*)pRender->bufferInfo.buffer;
			pData->pTexture = NULL;
			pData->texWidth = 0;
			pData->texHeight = 0;
		}
	}
	data->durationMs = 0;
	data->beginMs = 0;
	data->endMs = 0;
	data->curTimeMs = 0;
	data->curTimeUs = 0;
	data->IsPlaying = FALSE;
}

// 函数: OpenVideo
// 描述: 按分组索引加载视频流并刷新界面缓冲(见 .h)
BOOL AppState::OpenVideo(DWORD index)
{
	BOOL bRet = FALSE;
	VideoStream* pStream = NULL;
	DWORD dwFiles = 0;
	if (data == NULL || data->organizer == NULL)
	{
		return bRet;
	}
	if (index >= data->organizer->GetNamenum())
	{
		return bRet;
	}
	// 1. 释放旧流
	JoinRemix();
	if (data->stream != NULL)
	{
		delete data->stream;
		data->stream = NULL;
	}
	// 2. 建立新流并逐文件加载
	pStream = new VideoStream(AppRender::GetDevice(), AppRender::GetDeviceContext());
	dwFiles = data->organizer->GetFilenumByName(index);
	printf("[VIDEOCUT] 打开视频组: %s (%u 个文件)\n",
		data->organizer->GetName(index), (unsigned)dwFiles);
	for (DWORD i = 0; i < dwFiles; i++)
	{
		LPCSTR lpFile = data->organizer->GetFilename(index, i);
		printf("[VIDEOCUT]   OPEN: %s\n", (lpFile != NULL) ? lpFile : "(空)");
		if (pStream->Load(lpFile) == FALSE)
		{
			printf("[VIDEOCUT]   加载失败\n");
			delete pStream;
			goto END;
		}
	}
	// 3. 接管与初值
	data->stream = pStream;
	data->dwGroup = index;
	data->durationMs = pStream->GetMilliseconds();
	data->beginMs = 0;
	data->endMs = data->durationMs;
	data->curTimeMs = 0;
	data->curTimeUs = 0;
	data->dwThumb = 0;					// 活动端复位为起点(与源项目 RefreshUI 的 slider.oper = 0 一致)
	data->speed = 1.0;
	data->IsPlaying = FALSE;
	{
		CtrlInfo* pListCtrl = FindCtrl(LAYOUT_EDITOR, "##CLIPLIST");
		if (pListCtrl != NULL && pListCtrl->bufferInfo.buffer != NULL &&
			pListCtrl->bufferInfo.size >= sizeof(ListBuffer))
		{
			ListBuffer* pList = (ListBuffer*)pListCtrl->bufferInfo.buffer;
			pList->dwCount = 0;
			pList->dwSelected = LIST_SEL_NONE;
			pList->dwTop = 0;
		}
	}
	data->qwLastTick = GetTickCount64();
	data->qwNextTick = data->qwLastTick;
	data->qwPlayFix = 0;
	data->dwPlayCount = 0;
	RefreshUi();
	printf("[VIDEOCUT] 时长: %lld ms, 分辨率: %ldx%ld, 帧率: %u\n",
		(long long)data->durationMs,
		(long)data->stream->GetResolution().cx, (long)data->stream->GetResolution().cy,
		(unsigned)data->stream->GetFPS());
	bRet = TRUE;
END:
	return bRet;
}

// 函数: OpenFolder
// 描述: 扫描目录并按首个分组打开视频(见 .h)
BOOL AppState::OpenFolder(LPCSTR lpFolder)
{
	BOOL bRet = FALSE;
	MediaOrganizer* pOrg = NULL;
	if (data == NULL || lpFolder == NULL || lpFolder[0] == '\0')
	{
		return bRet;
	}
	if (data->IsRemixing != FALSE)
	{
		printf("[VIDEOCUT] 正在重混, 拒绝切换目录\n");
		return bRet;
	}
	if (MediaFileUtils::IsDirectoryExists(lpFolder) == FALSE)
	{
		printf("[VIDEOCUT] 目录不存在: %s\n", lpFolder);
		return bRet;
	}
	pOrg = MediaFileUtils::SearchVideos(lpFolder);
	if (pOrg == NULL || pOrg->GetNamenum() == 0)
	{
		if (pOrg != NULL)
		{
			delete pOrg;
		}
		printf("[VIDEOCUT] 目录内未找到视频: %s\n", lpFolder);
		return bRet;
	}
	// 1. 接管新组织器
	ReleaseMedia();
	data->organizer = pOrg;
	strcpy_s(data->szFolder, sizeof(data->szFolder), lpFolder);
	// 2. 重建组合框选项
	{
		CtrlInfo* pCombo = FindCtrl(LAYOUT_EDITOR, "##COMBO_VIDEO");
		if (pCombo != NULL && pCombo->bufferInfo.buffer != NULL &&
			pCombo->bufferInfo.size >= sizeof(CtrlComboData))
		{
			CtrlComboData* pData = (CtrlComboData*)pCombo->bufferInfo.buffer;
			if (pData->items != NULL)
			{
				AllocatorHub::FreeSmall(BufferInfo{ (VOID*)pData->items,
					sizeof(LPCSTR) * (size_t)((pData->count > 0) ? pData->count : 1) });
			}
			pData->count = (INT)pOrg->GetNamenum();
			pData->index = 0;
			BufferInfo arr = AllocatorHub::AllocSmall(sizeof(LPCSTR) * (size_t)pData->count);
			pData->items = (LPCSTR*)arr.buffer;
			if (pData->items != NULL)
			{
				for (INT i = 0; i < pData->count; i++)
				{
					pData->items[i] = pOrg->GetName((DWORD)i);
				}
			}
			else
			{
				pData->count = 0;
			}
		}
	}
	// 3. 切到编辑布局并打开首个分组
	SwitchLayout(LAYOUT_EDITOR);
	if (!OpenVideo(0))
	{
		printf("[VIDEOCUT] 首个分组加载失败, 停留在编辑布局的空状态\n");
	}
	bRet = TRUE;
	return bRet;
}

// 函数: Init
// 描述: 初始化业务状态(见 .h)
BOOL AppState::Init()
{
	BOOL bRet = FALSE;
	data = new AppData{};
	if (data == NULL)
	{
		return bRet;
	}
	data->speed = 1.0;
	data->dwLayout = LAYOUT_EDITOR;
	data->qwLastTick = GetTickCount64();
	data->qwNextTick = data->qwLastTick;
	if (!BindBuffers())
	{
		printf("[VIDEOCUT] 控件缓冲绑定未完全成功(部分控件将显示为空)\n");
	}
	// 准备界面是编辑界面的内嵌(flex)成员: 启动时登记嵌套, 默认不打开
	// (编辑界面是基座, 承载窗口段; 准备界面是叠在其上的浮层, 由「重设目录」的 OnReset 显示)
	if (Dispatch::GetLayoutCount() > LAYOUT_PREPARE)
	{
		Dispatch::SetLayoutNested(LAYOUT_PREPARE, TRUE);
		Dispatch::SetLayoutVisible(LAYOUT_PREPARE, FALSE);
	}
	Dispatch::SetLayoutVisible(LAYOUT_EDITOR, TRUE);
	SyncTimeControls();
	RefreshUi();
	{
		// 无人值守验证: 可选直接打开目录, 或直接显示准备浮层(无目录时)
		CHAR szDir[MAX_PATH * 3]{};
		DWORD dwLen = GetEnvironmentVariableA(IDENTITY_ENV_OPEN_DIR, szDir, (DWORD)sizeof(szDir));
		if (dwLen > 0 && dwLen < (DWORD)sizeof(szDir))
		{
			CtrlInfo* pInput = FindCtrl(LAYOUT_PREPARE, "##INPUT_SRC");
			if (pInput != NULL && pInput->bufferInfo.buffer != NULL && pInput->bufferInfo.size > 0)
			{
				strcpy_s((LPSTR)pInput->bufferInfo.buffer, pInput->bufferInfo.size, szDir);
			}
			OpenFolder(szDir);
		}
		else
		{
			// 默认停留编辑界面; 指定 prepare 时显示准备浮层(无人值守验证用)
			CHAR szLayout[32]{};
			if (GetEnvironmentVariableA(IDENTITY_ENV_START_LAYOUT, szLayout, (DWORD)sizeof(szLayout)) > 0 &&
				_stricmp(szLayout, "prepare") == 0)
			{
				SwitchLayout(LAYOUT_PREPARE);
			}
		}
	}
	{
		// 打印最终布局状态(启动分支可能已改变准备界面的可见性)
		BOOL bVisible = FALSE;
		BOOL bNested = FALSE;
		if (Dispatch::GetLayoutState(LAYOUT_PREPARE, &bVisible, &bNested) != FALSE)
		{
			printf("[VIDEOCUT] 布局: 编辑=槽 %u(基座) 准备=槽 %u(可见=%u 内嵌=%u)\n",
				(unsigned)LAYOUT_EDITOR, (unsigned)LAYOUT_PREPARE, (unsigned)bVisible, (unsigned)bNested);
		}
	}
	bRet = TRUE;
	return bRet;
}

// 函数: Release
// 描述: 释放业务状态(见 .h)
VOID AppState::Release()
{
	if (data == NULL)
	{
		return;
	}
	if (data->worker.joinable())
	{
		data->worker.join();
	}
	if (data->pRemixClips != NULL)
	{
		delete[] (BYTE*)data->pRemixClips;
		data->pRemixClips = NULL;
	}
	ReleaseMedia();
	delete data;
	data = NULL;
}

// 函数: DecodeTick
// 描述: 每帧推进预览: 先判活动端时码是否被拖动并定位, 再按帧率节流解码, 并把当前时码写回活动端(见 .h)
VOID AppState::DecodeTick()
{
	if (data == NULL || data->stream == NULL)
	{
		return;
	}
	DWORD dwFPS = data->stream->GetFPS();
	if (dwFPS == 0)
	{
		dwFPS = 1;
	}
	BOOL IsDecoder = FALSE;
	CONST ULONGLONG qwCur = GetTickCount64();
	// 活动端: dwThumb 为 0 取起点, 为 1 取终点(与源项目 slider.oper 同义)
	int64_t& sliderMs = (data->dwThumb == 0) ? data->beginMs : data->endMs;
	// 1. 活动端时码与当前预览时间不一致: 用户刚调整了滑块或时间条, 直接定位到该端
	if (sliderMs != data->curTimeMs)
	{
		data->curTimeUs = sliderMs * 1000;
		data->stream->SetCurrentTick(data->curTimeUs);
		data->curTimeMs = data->stream->GetCurrentMilliseconds();
		sliderMs = data->curTimeMs;
		data->qwLastTick = qwCur;
		data->qwNextTick = qwCur + 1000 / dwFPS;
		data->qwPlayFix = 0;
		data->dwPlayCount = 0;
		IsDecoder = TRUE;
		SyncTimeControls();
	}
	else if (data->IsPlaying == FALSE)
	{
		// 暂停: 只重置节拍, 防止再次播放时时间跳动过大
		data->qwLastTick = data->qwNextTick = qwCur;
		data->qwPlayFix = 0;
		data->dwPlayCount = 0;
	}
	else if (data->curTimeUs < data->stream->GetDuration())
	{
		if (qwCur > data->qwNextTick)
		{
			DWORD dwMS = 1000 / dwFPS;
			if (data->dwPlayCount % dwFPS > 1000 % dwFPS)
			{
				dwMS++;
			}
			data->qwPlayFix = qwCur - data->qwNextTick;
			data->qwNextTick = qwCur + dwMS - data->qwPlayFix;
			data->qwLastTick = qwCur;
			data->dwPlayCount++;
			data->curTimeUs = data->stream->GetCurrentTick();
			data->curTimeMs = data->stream->GetCurrentMilliseconds();
			// 活动端跟随播放头(与源项目 DecoderNext 播放分支同序)
			sliderMs = data->curTimeMs;
			IsDecoder = TRUE;
			SyncTimeControls();
		}
	}
	else
	{
		// 已到结尾: 结束播放(既有收尾行为)
		data->IsPlaying = FALSE;
		SetPlayText(FALSE);
	}
	// 2. 需要时解码一帧
	if (IsDecoder != FALSE)
	{
		data->stream->DecodeOper();
	}
}

// 函数: JoinRemix
// 描述: 回收已完成的重混线程与片段缓冲(见 .h)
VOID AppState::JoinRemix()
{
	if (data == NULL)
	{
		return;
	}
	if (data->worker.joinable() && data->IsRemixing == FALSE)
	{
		data->worker.join();
	}
	if (data->worker.joinable())
	{
		return;
	}
	if (data->pRemixClips != NULL)
	{
		delete[] (BYTE*)data->pRemixClips;
		data->pRemixClips = NULL;
	}
}

// 函数: Frame
// 描述: 每帧入口(见 .h)
VOID AppState::Frame(VOID* pUser)
{
	if (data != NULL)
	{
		DecodeTick();
		JoinRemix();
		SyncProgress();
		SyncSeek();
	}
	Dispatch::Frame(pUser);
}

// 函数: OnTitleBar
// 描述: 标题栏事件(见 .h)
VOID AppState::OnTitleBar(VOID* obj, uint64_t oper, uint64_t param)
{
	if (data == NULL)
	{
		return;
	}
	// 1. 关闭钮: 内嵌(flex)浮层只收起自己, 基座窗口才退出应用
	if (OperCode::GetSegment(oper, OperCode::OPER_SEG_OWN) == OPER_STRUCT_TITLE_CLOSE)
	{
		INT nLayout = LayoutIndexOfTitle(obj);
		BOOL bNested = FALSE;
		if (nLayout >= 0)
		{
			Dispatch::GetLayoutState((DWORD)nLayout, NULL, &bNested);
		}
		if (bNested != FALSE)
		{
			SwitchLayout(LAYOUT_EDITOR);
			return;
		}
		HWND hWnd = ImGuiHost::GetWindow(0);
		if (hWnd != NULL)
		{
			PostMessageW(hWnd, WM_CLOSE, 0, 0);
			PostQuitMessage(0);
		}
		return;
	}
	// 2. 拖动窗口: 按触发标题栏所属布局定位(准备界面与编辑界面同窗, 不能只看当前业务界面)
	{
		CONST BOOL bHold = OperCode::HasCommon(oper, OPER_COMMON_HOLD);
		CONST BOOL bActive = OperCode::HasCommon(oper, OPER_COMMON_ACTIVE);
		CONST BOOL bRelease = OperCode::HasCommon(oper, OPER_COMMON_DEACTIVATED);
		if (bHold != FALSE || bActive != FALSE || bRelease != FALSE)
		{
			INT nLayout = LayoutIndexOfTitle(obj);
			if (nLayout < 0)
			{
				nLayout = (INT)data->dwLayout;
			}
			CtrlInfo* pWnd = Dispatch::GetLayoutAt((DWORD)nLayout).GetLayoutBase(CTRL_TYPE_WND);
			if (pWnd != NULL)
			{
				BOOL bNested = FALSE;
				Dispatch::GetLayoutState((DWORD)nLayout, NULL, &bNested);
				if (bNested != FALSE)
				{
					// 内嵌(flex)布局: 位置记录相对宿主客户区, 而标题栏回传的 param 以宿主窗口位置为基准
					// (与面板坐标原点不一致, 直接采用会让面板在按下首帧跳到错误位置)
					// ⇒ 不采用 param, 改由鼠标客户区坐标与抓取偏移结算; 抓取偏移在按下帧记录
					HWND hWnd = ImGuiHost::GetCurrentWnd();
					POINT ptOrigin{};
					POINT ptCursor{};
					if (hWnd != NULL && ClientToScreen(hWnd, &ptOrigin) != FALSE &&
						GetCursorPos(&ptCursor) != FALSE)
					{
						CONST vec2 posCursor((FLOAT)(ptCursor.x - ptOrigin.x), (FLOAT)(ptCursor.y - ptOrigin.y));
						if (bActive != FALSE)
						{
							data->flexGrab = vec2(posCursor.x - pWnd->position.x, posCursor.y - pWnd->position.y);
							data->dwFlexDragLayout = (DWORD)nLayout;
							data->IsFlexDrag = TRUE;
						}
						if (data->IsFlexDrag != FALSE && data->dwFlexDragLayout == (DWORD)nLayout)
						{
							pWnd->position = vec2(posCursor.x - data->flexGrab.x, posCursor.y - data->flexGrab.y);
						}
					}
				}
				else if (bHold != FALSE && param != 0)
				{
					pWnd->position = UnpackWndPos(param);
				}
			}
			if (bRelease != FALSE)
			{
				data->IsFlexDrag = FALSE;
			}
			return;
		}
	}
	// 3. 双击最大化: 需要框架提供运行期参考尺寸重算入口, 当前阻塞
	if (OperCode::HasCommon(oper, OPER_COMMON_DOUBLECLICK) != FALSE)
	{
		printf("[VIDEOCUT] 双击最大化: 待框架提供运行期布局重算入口\n");
	}
}

// 函数: OnBrowse
// 描述: 浏览文件夹按钮(见 .h)
VOID AppState::OnBrowse()
{
	if (data == NULL)
	{
		return;
	}
	CtrlInfo* pInput = FindCtrl(LAYOUT_PREPARE, "##INPUT_SRC");
	if (pInput == NULL || pInput->bufferInfo.buffer == NULL)
	{
		return;
	}
	if (MediaFileUtils::FolderBrowse((LPSTR)pInput->bufferInfo.buffer, pInput->bufferInfo.size) == FALSE)
	{
		printf("[VIDEOCUT] 文件夹选择失败\n");
	}
}

// 函数: OnOpenFolder
// 描述: 打开文件夹按钮(见 .h)
VOID AppState::OnOpenFolder()
{
	if (data == NULL)
	{
		return;
	}
	CtrlInfo* pInput = FindCtrl(LAYOUT_PREPARE, "##INPUT_SRC");
	if (pInput == NULL || pInput->bufferInfo.buffer == NULL)
	{
		return;
	}
	OpenFolder((LPCSTR)pInput->bufferInfo.buffer);
}

// 函数: OnSelectVideo
// 描述: 视频分组下拉框变更(见 .h)
VOID AppState::OnSelectVideo()
{
	if (IsEditorActive() == FALSE)
	{
		return;
	}
	CtrlInfo* pCombo = FindCtrl(LAYOUT_EDITOR, "##COMBO_VIDEO");
	if (pCombo == NULL || pCombo->bufferInfo.buffer == NULL ||
		pCombo->bufferInfo.size < sizeof(CtrlComboData))
	{
		return;
	}
	CtrlComboData* pData = (CtrlComboData*)pCombo->bufferInfo.buffer;
	if (pData->index >= 0 && pData->index < pData->count)
	{
		OpenVideo((DWORD)pData->index);
	}
}

// 函数: OnAddSpan
// 描述: 添加剪辑片段到时间片列表(见 .h)
VOID AppState::OnAddSpan()
{
	if (IsEditorActive() == FALSE)
	{
		return;
	}
	if (data->beginMs < 0 || data->endMs <= data->beginMs)
	{
		printf("[VIDEOCUT] 无效片段: %lld ~ %lld\n",
			(long long)data->beginMs, (long long)data->endMs);
		return;
	}
	CtrlInfo* pCtrl = FindCtrl(LAYOUT_EDITOR, "##CLIPLIST");
	if (pCtrl == NULL || pCtrl->bufferInfo.buffer == NULL ||
		pCtrl->bufferInfo.size < sizeof(ListBuffer))
	{
		return;
	}
	ListBuffer* pList = (ListBuffer*)pCtrl->bufferInfo.buffer;
	if (pList->dwCount >= pList->dwCapacity)
	{
		printf("[VIDEOCUT] 片段表已满(%u)\n", (unsigned)pList->dwCapacity);
		return;
	}
	size_t cbItem = (pList->cbItem >= sizeof(TimeSpanItem)) ? (size_t)pList->cbItem : sizeof(TimeSpanItem);
	TimeSpanItem* pItem = (TimeSpanItem*)((BYTE*)(pList + 1) + cbItem * (size_t)pList->dwCount);
	pItem->startMs = data->beginMs;
	pItem->endMs = data->endMs;
	pList->dwCount++;
	printf("[VIDEOCUT] 添加片段: %lld ~ %lld (共 %u 段)\n",
		(long long)data->beginMs, (long long)data->endMs, (unsigned)pList->dwCount);
}

// 函数: OnSpanSlider
// 描述: 双手柄范围滑块变更(见 .h)
VOID AppState::OnSpanSlider(VOID* obj, uint64_t oper)
{
	if (IsEditorActive() == FALSE || obj == NULL)
	{
		return;
	}
	CtrlInfo* pCtrl = (CtrlInfo*)obj;
	if (pCtrl->bufferInfo.buffer == NULL || pCtrl->bufferInfo.size < sizeof(CtrlSliderRangeData))
	{
		return;
	}
	CtrlSliderRangeData* pData = (CtrlSliderRangeData*)pCtrl->bufferInfo.buffer;
	int64_t vMin = (pData->vMin > 0) ? pData->vMin : 0;
	int64_t vMax = (pData->vMax > vMin) ? pData->vMax : vMin;
	if (vMin > data->durationMs) { vMin = data->durationMs; }
	if (vMax > data->durationMs) { vMax = data->durationMs; }
	if (vMax < vMin) { vMax = vMin; }
	data->dwThumb = (OperCode::GetSegment(oper, OperCode::OPER_SEG_OWN) == OPER_SLIDER_THUMB_RIGHT) ? 1 : 0;
	data->beginMs = vMin;
	data->endMs = vMax;
	// 本处只写数值, 不直接定位解码: 下一帧 DecodeTick 发现活动端与预览时间不一致时统一定位
	// (与源项目 AdjustSpanBySlider 只改数值、由 DecoderNext 检测差异的写法一致)
	SyncTimeControls();
}

// 函数: OnEditBegin
// 描述: 起点时码提交(见 .h)
VOID AppState::OnEditBegin()
{
	if (IsEditorActive() == FALSE)
	{
		return;
	}
	CtrlInfo* pInput = FindCtrl(LAYOUT_EDITOR, "##TIMEBAR_BEGIN");
	if (pInput == NULL || pInput->bufferInfo.buffer == NULL ||
		pInput->bufferInfo.size < sizeof(CtrlTimeBarData))
	{
		return;
	}
	CtrlTimeBarData* pData = (CtrlTimeBarData*)pInput->bufferInfo.buffer;
	int64_t value = (pData->valueMs > 0) ? pData->valueMs : 0;
	if (value > data->durationMs) { value = data->durationMs; }
	data->dwThumb = 0;					// 编辑起点即把活动端切到起点(与源项目 slider.oper 一致)
	data->beginMs = value;
	if (data->endMs < data->beginMs) { data->endMs = data->beginMs; }
	SyncTimeControls();
	printf("[VIDEOCUT] 起点: %lld ms\n", (long long)data->beginMs);
}

// 函数: OnEditEnd
// 描述: 终点时码提交(见 .h)
VOID AppState::OnEditEnd()
{
	if (IsEditorActive() == FALSE)
	{
		return;
	}
	CtrlInfo* pInput = FindCtrl(LAYOUT_EDITOR, "##TIMEBAR_END");
	if (pInput == NULL || pInput->bufferInfo.buffer == NULL ||
		pInput->bufferInfo.size < sizeof(CtrlTimeBarData))
	{
		return;
	}
	CtrlTimeBarData* pData = (CtrlTimeBarData*)pInput->bufferInfo.buffer;
	int64_t value = (pData->valueMs > 0) ? pData->valueMs : 0;
	if (value > data->durationMs) { value = data->durationMs; }
	data->dwThumb = 1;					// 编辑终点即把活动端切到终点(与源项目 slider.oper 一致)
	data->endMs = value;
	if (data->beginMs > data->endMs) { data->beginMs = data->endMs; }
	SyncTimeControls();
	printf("[VIDEOCUT] 终点: %lld ms\n", (long long)data->endMs);
}

// 函数: OnListOperation
// 描述: 时间片列表行操作(见 .h)
VOID AppState::OnListOperation(VOID* obj, uint64_t oper, uint64_t param)
{
	UNREFERENCED_PARAMETER(obj);
	if (IsEditorActive() == FALSE)
	{
		return;
	}
	CtrlInfo* pCtrl = FindCtrl(LAYOUT_EDITOR, "##CLIPLIST");
	if (pCtrl == NULL || pCtrl->bufferInfo.buffer == NULL ||
		pCtrl->bufferInfo.size < sizeof(ListBuffer))
	{
		return;
	}
	ListBuffer* pList = (ListBuffer*)pCtrl->bufferInfo.buffer;
	CONST uint32_t dwRowOper = (uint32_t)(OperCode::GetSegment(oper, OperCode::OPER_SEG_PAYLOAD) & 0xFFFF);
	if (dwRowOper != OPER_LIST_ROW_BTN_CLICKED)
	{
		return;
	}
	DWORD dwRow = (DWORD)param;
	if (dwRow >= pList->dwCount)
	{
		return;
	}
	size_t cbItem = (pList->cbItem >= sizeof(TimeSpanItem)) ? (size_t)pList->cbItem : sizeof(TimeSpanItem);
	BYTE* pBase = (BYTE*)(pList + 1);
	for (DWORD i = dwRow; i + 1 < pList->dwCount; i++)
	{
		memcpy(pBase + cbItem * (size_t)i, pBase + cbItem * (size_t)(i + 1), cbItem);
	}
	pList->dwCount--;
	if (pList->dwSelected == dwRow)
	{
		pList->dwSelected = LIST_SEL_NONE;
	}
	else if (pList->dwSelected != LIST_SEL_NONE && pList->dwSelected > dwRow)
	{
		pList->dwSelected--;
	}
	printf("[VIDEOCUT] 删除片段: 行 %u (余 %u 段)\n", (unsigned)dwRow, (unsigned)pList->dwCount);
}

// 函数: OnPlay
// 描述: 播放 / 暂停(见 .h)
VOID AppState::OnPlay()
{
	if (IsEditorActive() == FALSE || data->stream == NULL)
	{
		return;
	}
	data->IsPlaying = (data->IsPlaying != FALSE) ? FALSE : TRUE;
	data->qwLastTick = data->qwNextTick = GetTickCount64();
	data->qwPlayFix = data->dwPlayCount = 0;
	SetPlayText(data->IsPlaying);
}

// 函数: OnSpeed
// 描述: 倍速输入提交(见 .h)
VOID AppState::OnSpeed()
{
	if (IsEditorActive() == FALSE)
	{
		return;
	}
	CtrlInfo* pSpeed = FindCtrl(LAYOUT_EDITOR, "##INPUT_SPEED");
	if (pSpeed == NULL || pSpeed->bufferInfo.buffer == NULL ||
		pSpeed->bufferInfo.size < sizeof(DOUBLE))
	{
		return;
	}
	DOUBLE* pValue = (DOUBLE*)pSpeed->bufferInfo.buffer;
	if (*pValue < SPEED_MIN)
	{
		*pValue = SPEED_MIN;
	}
	else if (*pValue > SPEED_MAX)
	{
		*pValue = SPEED_MAX;
	}
	data->speed = *pValue;
	if (data->stream != NULL)
	{
		data->stream->SetFactor(data->speed);
	}
	printf("[VIDEOCUT] 倍速: %.2f\n", data->speed);
}

// 函数: OnRemix
// 描述: 执行重混导出(见 .h)
VOID AppState::OnRemix()
{
	if (IsEditorActive() == FALSE || data->stream == NULL || data->organizer == NULL)
	{
		return;
	}
	if (data->IsRemixing != FALSE)
	{
		printf("[VIDEOCUT] 正在重混, 请稍后\n");
		return;
	}
	JoinRemix();
	CtrlInfo* pCtrl = FindCtrl(LAYOUT_EDITOR, "##CLIPLIST");
	if (pCtrl == NULL || pCtrl->bufferInfo.buffer == NULL ||
		pCtrl->bufferInfo.size < sizeof(ListBuffer))
	{
		return;
	}
	ListBuffer* pList = (ListBuffer*)pCtrl->bufferInfo.buffer;
	DWORD dwUsed = pList->dwCount;
	if (dwUsed == 0)
	{
		printf("[VIDEOCUT] 没有可导出的片段\n");
		return;
	}
	size_t cbItem = (pList->cbItem >= sizeof(TimeSpanItem)) ? (size_t)pList->cbItem : sizeof(TimeSpanItem);
	// 1. 复制并按起点插入排序
	{
		AppSpan* Sorted = new AppSpan[dwUsed];
		DWORD dwCount = 0;
		for (DWORD i = 0; i < dwUsed; i++)
		{
			TimeSpanItem* pItem = (TimeSpanItem*)((BYTE*)(pList + 1) + cbItem * (size_t)i);
			int64_t left = pItem->startMs;
			int64_t right = pItem->endMs;
			int j = (int)dwCount - 1;
			while (j >= 0 && Sorted[j].left > left)
			{
				Sorted[j + 1] = Sorted[j];
				j--;
			}
			Sorted[j + 1].left = left;
			Sorted[j + 1].right = right;
			dwCount++;
		}
		// 2. 合并重叠或相邻片段
		DWORD dwMerged = 0;
		for (DWORD i = 0; i < dwCount; i++)
		{
			if (dwMerged == 0)
			{
				Sorted[dwMerged] = Sorted[i];
				dwMerged++;
			}
			else
			{
				AppSpan& prev = Sorted[dwMerged - 1];
				AppSpan& curr = Sorted[i];
				if (prev.right >= curr.left)
				{
					if (curr.right > prev.right)
					{
						prev.right = curr.right;
					}
				}
				else
				{
					Sorted[dwMerged] = curr;
					dwMerged++;
				}
			}
		}
		// 3. 构建工作线程需要的片段块(微秒)
		{
			size_t cbClips = sizeof(int64_t) + (size_t)dwMerged * sizeof(int64_t) * 2;
			BYTE* pClips = new BYTE[cbClips];
			int64_t* pHead = (int64_t*)pClips;
			int64_t* pSpans = (int64_t*)(pClips + sizeof(int64_t));
			int64_t expect = 0;
			*pHead = (int64_t)dwMerged;
			for (DWORD i = 0; i < dwMerged; i++)
			{
				pSpans[i * 2] = Sorted[i].left * 1000;
				pSpans[i * 2 + 1] = Sorted[i].right * 1000;
				int64_t range = Sorted[i].right - Sorted[i].left;
				expect += (range > 0) ? range * 1000 : 0;
			}
			// 4. 输出路径
			strcpy_s(data->szOutput, sizeof(data->szOutput), data->szFolder);
			strcat_s(data->szOutput, sizeof(data->szOutput), "\\");
			strcat_s(data->szOutput, sizeof(data->szOutput), data->organizer->GetName(data->dwGroup));
			strcat_s(data->szOutput, sizeof(data->szOutput), "[remix].mp4");
			// 5. 启动工作线程
			data->pRemixClips = pClips;
			data->remixCur = 0;
			data->remixExpect = expect;
			data->IsRemixing = TRUE;
			data->worker = std::thread(&VideoStream::RemixClips, data->stream,
				data->szOutput, data->speed, std::ref(data->remixCur),
				std::ref(data->IsRemixing), data->pRemixClips);
			printf("[VIDEOCUT] 输出: %s (片段 %u, 预期 %lld us)\n",
				data->szOutput, (unsigned)dwMerged, (long long)expect);
		}
		delete[] Sorted;
	}
}

// 函数: OnReset
// 描述: 重设目录并回到准备界面(见 .h)
VOID AppState::OnReset()
{
	if (data == NULL)
	{
		return;
	}
	if (data->IsRemixing != FALSE)
	{
		printf("[VIDEOCUT] 正在重混, 拒绝重设\n");
		return;
	}
	JoinRemix();
	ReleaseMedia();
	{
		CtrlInfo* pListCtrl = FindCtrl(LAYOUT_EDITOR, "##CLIPLIST");
		if (pListCtrl != NULL && pListCtrl->bufferInfo.buffer != NULL &&
			pListCtrl->bufferInfo.size >= sizeof(ListBuffer))
		{
			ListBuffer* pList = (ListBuffer*)pListCtrl->bufferInfo.buffer;
			pList->dwCount = 0;
			pList->dwSelected = LIST_SEL_NONE;
			pList->dwTop = 0;
		}
	}
	data->szFolder[0] = '\0';
	data->szOutput[0] = '\0';
	{
		CtrlInfo* pInput = FindCtrl(LAYOUT_PREPARE, "##INPUT_SRC");
		if (pInput != NULL && pInput->bufferInfo.buffer != NULL)
		{
			RtlZeroMemory(pInput->bufferInfo.buffer, pInput->bufferInfo.size);
		}
	}
	SwitchLayout(LAYOUT_PREPARE);
	RefreshUi();
	printf("[VIDEOCUT] 已重设目录\n");
}
