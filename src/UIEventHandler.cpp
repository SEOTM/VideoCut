#include <windows.h>
#include <stdio.h>
#include <thread>
#include "UIEventHandler.h"
#include "Application.h"
#include "MediaFileUtils.h"
#include "D3D11Manager.h"
#include "VideoStream.h"
// ================================
// 操作定义
// ================================
#define TIMESPAN_OPER(oper) ((oper) & 1 ^ 1)

// ================================
// 区域: 处理函数前向声明区
// ================================

// 函数: OnTitleBarEvent
// 描述: 标题栏事件处理函数
VOID OnTitleBarEvent();
// 函数: FileBrowse
// 功能: 调用系统文件浏览器
VOID FileBrowse();
// 函数: OpenFolder
// 功能: 打开文件夹
VOID OpenFolder();
// 函数: DecoderNext
// 功能: 解码下一帧
VOID DecoderNext();
// 函数: AppendSpan
// 功能: 增加时间段
VOID AppendSpan();
// 函数: AdjustSpanByTimeBar
// 功能: 时段调整
VOID AdjustSpanByTimeBar(int oper);
VOID AdjustSpanByTimeBarL();
VOID AdjustSpanByTimeBarR();
// 函数: AdjustSpanBySlider
// 功能: 时段调整
VOID AdjustSpanBySlider();
// 函数: SetPlayState
// 功能: 设置播放状态
VOID SetPlayState();
// 函数: SelectVideo
// 功能: 选择视频
VOID SelectVideo();
// 函数: RemixClips
// 功能: 根据时间片段重新裁剪和重组视频
VOID RemixClips();
// 函数: RefreshUI
// 功能: 刷新界面 (用于切换视频需要重新清空时段列表、加载新的视频信息等情况)
VOID RefreshUI(VideoStream* video);
// 函数: OpenVideoByIndex
// 功能: 通过索引打开视频组中的视频
VideoStream* OpenVideoByIndex(MediaOrganizer* org, DWORD index);
// 函数: ListOperation
// 功能: 列表操作选项
VOID ListOperation();
// 函数: CheckStatic
// 功能: 检查状态
VOID CheckStatic();
// 函数: CheckSpeed
// 功能: 检查输入速度是否在处理区间
VOID CheckSpeed();
// 函数: ResetFolder
// 功能: 切换回默认界面，重设目录
VOID ResetFolder();

// ================================
// 辅助结构体 (有限作用域)
// ================================

// 结构: Range
// 描述: 一个范围左值和右值
template <typename numerical>
struct RANGE
{
	numerical left;
	numerical right;
};

// 结构: VideoInfo
// 描述: 视频信息结构体
struct VideoInfo
{
	SIZE size;
	ID3D11ShaderResourceView* srv;
	VideoStream* vs;
	MediaOrganizer* organizer;
	int64_t curTimeUS;
	int64_t curTimeMS;
	int64_t IsPlaying;
};
// 结构: SliderInfo
// 描述: 滑块信息结构体
struct SliderInfo
{
	RANGE<int64_t> val;
	RANGE<int64_t> limit;
	int64_t oper;
};
// 结构: ListSpanInfo
// 描述: 时段列表结构体
struct ListSpanInfo
{
	DWORD used;
	DWORD top;
	DWORD caps;
	DWORD oper;
	RANGE<int64_t> spans[];
};
// 结构: TimeBarInfo
// 描述: 时段列表结构体
struct TimeBarInfo
{
	LPCSTR hint;
	RANGE<int64_t> span;
};
// 结构: ComboInfo
// 描述: 组合框信息结构体
struct ComboInfo
{
	DWORD count;
	DWORD index;
	LPCSTR* items;
};
// 结构: ProgInfo
// 描述: 进度信息结构体
struct ProgInfo
{
	int64_t current = 1;			// 当前处理
	int64_t expect = 1;			// 预期处理
	BOOL IsVisible = FALSE;		// 可视标记
};

// ================================
// 其他常量
// ================================
constexpr int FOLDER_BUFFER = MAX_PATH * 3;
constexpr int TIMEBAR_BUFFER = sizeof(TimeBarInfo);
constexpr int SLIDER_BUFFER = sizeof(SliderInfo);
constexpr int LIST_ITEMS = 126;
constexpr int LIST_BUFFER = sizeof(RANGE<int64_t>) * LIST_ITEMS + sizeof(ListSpanInfo);
constexpr int RENDER_BUFFER = sizeof(VideoInfo);
constexpr int PSTRING_BUFFER = sizeof(LPCSTR);
constexpr int COMBO_BUFFER = sizeof(ComboInfo);
constexpr int PROG_BUFFER = sizeof(ProgInfo);
constexpr int DOUBLE_BUFFER = sizeof(DOUBLE);

// ================================
// 特殊变量
// ================================
struct // 重混状态
{
	int64_t current = 0;			// 当前处理
	int64_t expect = 0;			// 预期处理
	BOOL IsRemixing = FALSE;		// 重混标记
}RemixStatic;
static std::thread WorkThread;	// 线程对象

// ================================
// 类名: UIEventHandler
// 描述: UI事件处理类
// ================================

// 函数: BindEventHandler
// 描述: 绑定事件处理函数
// 输入:
//    LPCSTR eventname - 事件名称
// 返回值:
//    pfnCtrlEvent - 返回事件处理函数指针
EventInfo UIEventHandler::BindEventHandler(LPCSTR eventname)
{
	EventInfo ret{};
	if (strcmp(eventname, "TITLEBAR_EVENT") == 0)
	{
		ret.pfnEvent = OnTitleBarEvent;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "ONLY_BUFFER_PATH") == 0)
	{
		// ONLY_BUFFER 前缀指无指定事件，但是无法通过类型直接指定缓冲区，所以通过绑定事件时来获取一个大小
		ret.bufferSize = FOLDER_BUFFER;
	}
	else if (strcmp(eventname, "FILEBROWSE") == 0)
	{
		ret.pfnEvent = FileBrowse;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "OPENFOLDER") == 0)
	{
		ret.pfnEvent = OpenFolder;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "APPENDSPAN") == 0)
	{
		ret.pfnEvent = AppendSpan;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "REMIXCLIPS") == 0)
	{
		ret.pfnEvent = RemixClips;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "ADJUSTSPANBYSLIDER") == 0)
	{
		ret.pfnEvent = AdjustSpanBySlider;
		ret.bufferSize = SLIDER_BUFFER;
	}
	else if (strcmp(eventname, "ADJUSTSPANBYTIMEBARL") == 0)
	{
		ret.pfnEvent = AdjustSpanByTimeBarL;
		ret.bufferSize = TIMEBAR_BUFFER;
	}
	else if (strcmp(eventname, "ADJUSTSPANBYTIMEBARR") == 0)
	{
		ret.pfnEvent = AdjustSpanByTimeBarR;
		ret.bufferSize = TIMEBAR_BUFFER;
	}
	else if (strcmp(eventname, "LISTOPERATION") == 0)
	{
		ret.pfnEvent = ListOperation;
		ret.bufferSize = LIST_BUFFER;
	}
	else if (strcmp(eventname, "DECODERNEXT") == 0)
	{
		ret.pfnEvent = DecoderNext;
		ret.bufferSize = RENDER_BUFFER;;
	}
	else if (strcmp(eventname, "SETPLAYSTATE") == 0)
	{
		ret.pfnEvent = SetPlayState;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "SELECTVIDEO") == 0)
	{
		ret.pfnEvent = SelectVideo;
		ret.bufferSize = COMBO_BUFFER;
	}
	else if (strcmp(eventname, "CHECKSTATE") == 0)
	{
		ret.pfnEvent = CheckStatic;
		ret.bufferSize = PROG_BUFFER;
	}
	else if (strcmp(eventname, "CHECKSPEED") == 0)
	{
		ret.pfnEvent = CheckSpeed;
		ret.bufferSize = 0;
	}
	else if (strcmp(eventname, "RESETFOLDER") == 0)
	{
		ret.pfnEvent = ResetFolder;
		ret.bufferSize = 0;
	}
	return ret;
}

// ================================
// 区域: 处理函数前向定义区
// ================================

// 函数: OnTitleBarEvent
// 描述: 标题栏事件处理函数
VOID OnTitleBarEvent()
{
	HWND hWnd = Application::GetWindowHandle();
	CtrlInfo* title = Application::GetLayout().GetCtrlInfo(1);
	TitleBuffer& info = *(TitleBuffer*)title->bufferInfo.buffer;
	// 处理关闭
	if (info.flag & TITLEBAR_FLAG_EXITCLICKED)
	{
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		goto EVENT_END;
	}
	// 双击切换最大化
	if ((info.flag & TITLEBAR_FLAG_DOUBLECLICK) && (info.flag & TITLEBAR_FLAG_HOVERED))
	{
		Application::SwitchMaximized();
		goto EVENT_END;
	}
	// 拖拽处理
	static POINT dragOffset = { 0, 0 };
	static BOOL isDragging = FALSE;

	if (info.flag & TITLEBAR_FLAG_ACTIVE)
	{
		// 鼠标正在标题栏上按下
		if (!isDragging)
		{
			// 第一次捕获：计算偏移
			POINT cursor;
			RECT rect;
			GetCursorPos(&cursor);
			GetWindowRect(hWnd, &rect);
			dragOffset.x = cursor.x - rect.left;
			dragOffset.y = cursor.y - rect.top;
			isDragging = TRUE;
		}
		else
		{
			// 拖拽移动中
			POINT cursor;
			GetCursorPos(&cursor);
			int newX = cursor.x - dragOffset.x;
			int newY = cursor.y - dragOffset.y;
			SetWindowPos(hWnd, NULL, newX, newY, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
		}
	}
	else
	{
		// 鼠标已释放（重置拖拽状态）
		isDragging = FALSE;
	}
EVENT_END:
	// 消费指令后重置指令
	info.flag = TITLEBAR_FLAG_NONE;
}

// 函数: FileBrowse
// 功能: 调用系统文件浏览器
VOID FileBrowse()
{
	const CtrlInfo* info = Application::FindControl(0, "##INPUT_SRC");
	BufferInfo buf = info->bufferInfo;
	MediaFileUtils::FolderBrowse((LPSTR)buf.buffer, buf.size);
}

// 函数: OpenFolder
// 功能: 打开文件夹
VOID OpenFolder()
{
	LPCSTR path = (LPCSTR)Application::FindControl(0, "##INPUT_SRC")->bufferInfo.buffer;
	BOOL ret = MediaFileUtils::IsDirectoryExists(path);
	printf(u8"目录 %s ", path);
	if (ret)
	{
		puts(u8"存在\n");
		// 获取布局对象引用
		VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
		ComboInfo& combo = *(ComboInfo*)Application::FindControl(1, "##COMBO_VIDEO")->bufferInfo.buffer;
		// 获取窗体样式
		LONG_PTR style = GetWindowLongPtr(Application::GetWindowHandle(), GWL_STYLE);
		// 搜索视频
		MediaOrganizer* organizer = MediaFileUtils::SearchVideos(path);
		if (organizer == nullptr || organizer->GetNamenum() == 0)
		{
			if (organizer)
			{
				delete organizer;
			}
			goto END;
		}
		// 设置视频信息
		video.organizer = organizer;
		// 设置组合框信息
		combo.count = organizer->GetNamenum();
		combo.index = 0;
		combo.items = new LPCSTR[combo.count];
		for (DWORD i = 0; i < combo.count; i++)
		{
			combo.items[i] = organizer->GetName(i);
		}
		// 更换布局
		Application::SwitchLayout("Editor");
		// 默认选择第一个视频组
		SelectVideo();
		// 保存操作目录
		Application::SetCurrentFolder(path);
		// 重设窗体样式
		SetWindowLongPtr(Application::GetWindowHandle(), GWL_STYLE, style | WS_THICKFRAME);
		SetWindowPos(Application::GetWindowHandle(), NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		// 初始化成功，设置返回值为 TRUE
		ret = TRUE;
	}
	else
	{
		puts(u8"不存在\n");
	}
END:
	return;
}
// 函数: DecoderNext
// 功能: 解码下一帧
VOID DecoderNext()
{
	static int64_t qwLast = GetTickCount64();
	static int64_t qwNext = GetTickCount64();
	static int64_t qwFix = 0;
	static int64_t qwPlay = 0;
	// 设置渲染器控件
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	SliderInfo& slider = *(SliderInfo*)Application::FindControl(1, "##TIMESPANSLIDER")->bufferInfo.buffer;
	TimeBarInfo& begin = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_BEGIN")->bufferInfo.buffer;
	TimeBarInfo& end = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_END")->bufferInfo.buffer;
	DOUBLE& speed = *(DOUBLE*)Application::FindControl(1, "##INPUT_SPEED")->bufferInfo.buffer;
	// 准备判断
	BOOL IsDecoder = FALSE;
	int64_t qwCur = GetTickCount64();
	DWORD dwFPS = video.vs->GetFPS();
	BOOL oper = slider.oper & 1 ^ 1;
	int64_t& time = oper ? begin.span.left : end.span.left;
	int64_t& sliderus = oper ? slider.val.left : slider.val.right;
	if (video.curTimeUS != sliderus)
	{
		// 如果当前时间与滑块时间不一致，说明用户调整了滑块，此时需要直接跳转到滑块位置并解码
		video.curTimeUS = sliderus;
		video.vs->SetCurrentTick(video.curTimeUS);
		video.curTimeMS = video.vs->GetCurrentMilliseconds();
		time = video.curTimeMS;

		qwPlay = 0;
		qwNext = qwCur + 1000 / dwFPS;
		qwLast = qwCur;
		qwFix = 0;
		IsDecoder = TRUE;
	}
	else
	{
		if (!video.IsPlaying)
		{
			// 更新下一帧时间，防止用户暂停后再次点击播放时，时间跳动过大
			qwLast = qwNext = qwCur;
			qwFix = qwPlay = 0;
		}
		else if (video.curTimeUS < video.vs->GetDuration())
		{
			if (qwCur > qwNext)
			{
				DWORD dwMS = 1000 / dwFPS;
				if (qwPlay % dwFPS > 1000 % dwFPS)
				{
					dwMS++;
				}
				qwFix = qwCur - qwNext;
				qwNext = qwCur + dwMS - qwFix;
				qwLast = qwCur;
				qwPlay++;

				video.curTimeUS = video.vs->GetCurrentTick();
				sliderus = video.curTimeUS;
				video.curTimeMS = video.vs->GetCurrentMilliseconds();
				time = video.curTimeMS;

				IsDecoder = TRUE;
			}

		}
	}
	if (IsDecoder)
	{
		video.vs->DecodeOper();
	}
}
// 函数: AppendSpan
// 功能: 增加时间段
VOID AppendSpan()
{
	// 设置渲染器控件
	ListSpanInfo& list = *(ListSpanInfo*)Application::FindControl(1, "##CLIPLIST")->bufferInfo.buffer;
	TimeBarInfo& begin = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_BEGIN")->bufferInfo.buffer;
	TimeBarInfo& end = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_END")->bufferInfo.buffer;
	if (list.used < list.caps)
	{
		list.spans[list.used] = { begin.span.left,end.span.left };
		list.used += 1;
	}
}
// 函数: AdjustSpanByTimeBar
// 功能: 时段调整
VOID AdjustSpanByTimeBar(int oper)
{
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	SliderInfo& slider = *(SliderInfo*)Application::FindControl(1, "##TIMESPANSLIDER")->bufferInfo.buffer;
	TimeBarInfo& begin = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_BEGIN")->bufferInfo.buffer;
	TimeBarInfo& end = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_END")->bufferInfo.buffer;
	DWORD dwFPS = video.vs->GetFPS();
	// 判断左右端点
	int64_t& sliderus = TIMESPAN_OPER(oper) ? slider.val.left : slider.val.right;
	int64_t& time = TIMESPAN_OPER(oper) ? begin.span.left : end.span.left;

	sliderus = time * 1000;
	slider.oper = oper;
}
// 函数: AdjustSpanByTimeBar
// 功能: 时段调整（左）
VOID AdjustSpanByTimeBarL()
{
	AdjustSpanByTimeBar(0);
}
// 函数: AdjustSpanByTimeBar
// 功能: 时段调整（右）
VOID AdjustSpanByTimeBarR()
{
	AdjustSpanByTimeBar(1);
}
// 函数: AdjustSpanBySlider
// 功能: 时段调整
VOID AdjustSpanBySlider()
{
	// 设置渲染器控件
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	SliderInfo& slider = *(SliderInfo*)Application::FindControl(1, "##TIMESPANSLIDER")->bufferInfo.buffer;
	TimeBarInfo& begin = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_BEGIN")->bufferInfo.buffer;
	TimeBarInfo& end = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_END")->bufferInfo.buffer;

	DWORD dwFPS = video.vs->GetFPS();
	// 判断左右端点
	int64_t& sliderus = TIMESPAN_OPER(slider.oper) ? slider.val.left : slider.val.right;
	int64_t& time = TIMESPAN_OPER(slider.oper) ? begin.span.left : end.span.left;
	time = sliderus / 1000;
}
// 函数: SetPlayState
// 功能: 设置播放状态
VOID SetPlayState()
{
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	// 获取播放按钮控件
	UINT index = Application::FindControlIndex(1, "##PLAY");
	CtrlInfo& playBtn = Application::GetLayout().GetCtrlInfoRef(index);
	// 切换播放状态
	video.IsPlaying = ~video.IsPlaying;
	// 更新播放按钮文本
	if (video.IsPlaying)
	{
		playBtn.bufferInfo.buffer = (void*)u8"暂停预览"; // 暂停预览
	}
	else
	{
		playBtn.bufferInfo.buffer = (void*)u8"播放预览"; // 播放预览
	}
}
// 函数: SelectVideo
// 功能: 选择视频
VOID SelectVideo()
{
	// 获取控件
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	ComboInfo& combo = *(ComboInfo*)Application::FindControl(1, "##COMBO_VIDEO")->bufferInfo.buffer;
	// 获取选择的索引
	DWORD index = combo.index;

	// 通过索引打开视频
	VideoStream* newStream = OpenVideoByIndex(video.organizer, index);
	if (newStream)
	{
		// 刷新界面
		RefreshUI(newStream);
	}
}
// 函数: RemixClips
// 功能: 根据时间片段重新裁剪和重组视频
VOID RemixClips()
{
	// 读取剪切列表
	ListSpanInfo& list = *(ListSpanInfo*)Application::FindControl(1, "##CLIPLIST")->bufferInfo.buffer;
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	ComboInfo& combo = *(ComboInfo*)Application::FindControl(1, "##COMBO_VIDEO")->bufferInfo.buffer;
	ProgInfo& prog = *(ProgInfo*)Application::FindControl(1, "##PROG_REMIX")->bufferInfo.buffer;
	DOUBLE& speed = *(DOUBLE*)Application::FindControl(1, "##INPUT_SPEED")->bufferInfo.buffer;
	DWORD index = combo.index;
	if (RemixStatic.IsRemixing)
	{
		printf(u8"FAIL: 正在执行任务中，请稍后尝试\n");
		return;
	}
	struct Clips
	{
		int64_t count;
		RANGE<int64_t> spans[LIST_ITEMS];
	};
	Clips* clips = new Clips{};
	// 1. 原有时间进行重排序
	for (DWORD i = 0; i < list.used; i++)
	{
		auto [left, right] = list.spans[i];
		left *= 1000;
		right *= 1000;
		// 插入排序：在已排序的 clips->spans 中找到插入位置
		int j = clips->count - 1;
		while (j >= 0 && clips->spans[j].left > left)
		{
			clips->spans[j + 1] = clips->spans[j]; // 后移元素
			j--;
		}
		// 插入新元素
		clips->spans[j + 1] = { left, right };
		clips->count++;
	}
	// DEBUG: 按序输出
	printf(u8"剪切列表:\n");
	for (int i = 0; i < clips->count; i++)
	{
		printf("  %lld ~ %lld\n", clips->spans[i].left, clips->spans[i].right);
	}
	// 2. 合并重合字段 
	//    已知 [l1,r1], [l2,r2] 且 r1 >= l2
	//    则 > [l1,r2] 
	if (clips->count > 0)
	{
		int new_count = 0;
		for (int i = 0; i < clips->count; i++)
		{
			if (new_count == 0)
			{
				clips->spans[new_count] = clips->spans[i];
				new_count++;
			}
			else
			{
				RANGE<int64_t>& prev = clips->spans[new_count - 1];
				RANGE<int64_t>& curr = clips->spans[i];
				if (prev.right >= curr.left)
				{
					// 重叠或相邻
					if (curr.right > prev.right)
					{
						prev.right = curr.right; // 扩展右端点
					}
				}
				else
				{
					clips->spans[new_count] = curr;
					new_count++;
				}
			}
		}
		clips->count = new_count;
	}
	putchar(10);
	// DEBUG: 输出合并后结果
	printf(u8"合并剪切:\n");
	int64_t expect = 0;
	for (int i = 0; i < clips->count; i++)
	{
		printf("  %lld ~ %lld\n", clips->spans[i].left, clips->spans[i].right);
		int64_t range = clips->spans[i].right - clips->spans[i].left;
		expect += range > 0 ? range : 0;
	}
	putchar(10);
	if (clips->count == 0)
	{
		printf(u8"FAIL: 无效剪切范围\n");
		delete clips;
	}
	else
	{
		// 构建输出目录和输出文件名：
		char outpath[MAX_PATH * 3]{};
		strcpy_s(outpath, MAX_PATH * 3, video.organizer->GetName(index));
		strcat_s(outpath, MAX_PATH, "[remix].mp4");
		LPCSTR lpFolder = Application::GetCurrentFolder();
		printf(u8"输出目录: %s\n", lpFolder);
		printf(u8"输出文件: %s\n\n", outpath);
		CHAR* lpFilename = new CHAR[MAX_PATH * 3]{};
		strcpy_s(lpFilename, MAX_PATH * 3, lpFolder);
		strcat_s(lpFilename, MAX_PATH * 3, "\\");
		strcat_s(lpFilename, MAX_PATH * 3, outpath);
		// 设置重混状态
		RemixStatic.current = 0;
		RemixStatic.expect = expect;
		RemixStatic.IsRemixing = TRUE;

		// 执行裁剪
		WorkThread = std::thread(&VideoStream::RemixClips, video.vs, lpFilename, speed, std::ref(RemixStatic.current), std::ref(RemixStatic.IsRemixing), clips);
		//video.vs->RemixClips(lpFolder, outpath, prog.current, RemixStatic.IsRemixing, clips);
		prog.IsVisible = TRUE;
	}
}
// 函数: RefreshUI
// 功能: 刷新界面 (用于切换视频需要重新清空时段列表、加载新的视频信息等情况)
VOID RefreshUI(VideoStream* stream)
{
	// 1. 获取相关控件的引用
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	SliderInfo& slider = *(SliderInfo*)Application::FindControl(1, "##TIMESPANSLIDER")->bufferInfo.buffer;
	TimeBarInfo& begin = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_BEGIN")->bufferInfo.buffer;
	TimeBarInfo& end = *(TimeBarInfo*)Application::FindControl(1, "##TIMEBAR_END")->bufferInfo.buffer;
	ListSpanInfo& list = *(ListSpanInfo*)Application::FindControl(1, "##CLIPLIST")->bufferInfo.buffer;
	ComboInfo& combo = *(ComboInfo*)Application::FindControl(1, "##COMBO_VIDEO")->bufferInfo.buffer;
	ProgInfo& prog = *(ProgInfo*)Application::FindControl(1, "##PROG_REMIX")->bufferInfo.buffer;
	CONST BufferInfo& listbuf = Application::FindControl(1, "##CLIPLIST")->bufferInfo;
	DOUBLE& speed = *(DOUBLE*)Application::FindControl(1, "##INPUT_SPEED")->bufferInfo.buffer;
	// 3. 更新时间控制组件范围
	begin.span.left = slider.val.left = slider.limit.left = 0;
	begin.span.right = end.span.left = end.span.right = stream->GetMilliseconds();
	slider.val.right = slider.limit.right = stream->GetDuration();
	slider.oper = 0;
	// 4. 清空时段列表
	RtlZeroMemory(listbuf.buffer, listbuf.size);
	list.used = 0;
	list.top = 0;
	list.oper = 0;
	list.caps = LIST_ITEMS;
	// 5. 设置播放按钮状态
	UINT index = Application::FindControlIndex(1, "##PLAY");
	CtrlInfo& play = Application::GetLayout().GetCtrlInfoRef(index);
	play.bufferInfo.buffer = (void*)u8"播放预览"; // 播放预览
	// 6. 释放旧的视频资源（如果存在）
	if (video.vs)
	{
		delete video.vs;
		video.vs = NULL;
	}
	// 7. 设置 VideoInfo 结构体中的视频资源
	video.vs = stream;
	video.size = video.vs->GetResolution();
	video.srv = video.vs->GetSRV();
	video.curTimeUS = 0;
	video.curTimeMS = 0;
	video.IsPlaying = FALSE;
	// 设置重混进度
	prog = {};
	// 设置变速因子为 1.0
	speed = 1.0;
}
// 函数: OpenVideoByIndex
// 功能: 通过索引打开视频组中的视频
VideoStream* OpenVideoByIndex(MediaOrganizer* org, DWORD index)
{
	// 设置返回变量
	VideoStream* ret = NULL;
	VideoStream* vs = NULL;
	DWORD dwNum = 0;
	// 检查索引有效性
	if (index >= org->GetNamenum())
	{
		goto END;
	}
	// 检查视频资源是否存在
	if (org->GetFilenumByName(index) == 0)
	{
		goto END;
	}
	// 加载新视频
	vs = new VideoStream(D3D11Manager::GetInstance().GetDevice(), D3D11Manager::GetInstance().GetDeviceContext());
	dwNum = org->GetFilenumByName(index);
	printf(u8"%s [%d]\n", org->GetName(index), dwNum);
	// 按序加载 org 中索引为 index 的所有视频
	for (int i = 0; i < dwNum; i++)
	{
		// 输出加载提示
		printf(u8"  OPEN: %s\n", org->GetFilename(index, i));
		BOOL loadret = vs->Load(org->GetFilename(index, i));
		if (loadret == FALSE)
		{
			printf("    STATE: FAIL\n\n");
			// 加载失败，释放已加载的视频资源
			delete vs;
			goto END;
		}
		else
		{
			printf("    STATE: SUCC\n\n");
		}
	}
	// 加载成功，设置返回值
	ret = vs;
END:
	return ret;
}
// 函数: ListOperation
// 功能: 列表操作选项
VOID ListOperation()
{
	// 获取列表控件信息
	ListSpanInfo* listInfo = (ListSpanInfo*)Application::FindControl(1, "##CLIPLIST")->bufferInfo.buffer;

	DWORD oper = listInfo->oper;
	if (oper & 1) // 删除操作
	{
		DWORD index = oper >> 1;
		if (index < listInfo->used)
		{
			// 将后面的元素前移
			for (DWORD i = index; i < listInfo->used - 1; i++)
			{
				listInfo->spans[i] = listInfo->spans[i + 1];
			}
			listInfo->used--;
			listInfo->oper = 0;      // 清除操作标志
			listInfo->top = 0;        // 可选：重置滚动起始位置
		}
	}
}
// 函数: CheckStatic
// 功能: 检查状态
VOID CheckStatic()
{
	ProgInfo& prog = *(ProgInfo*)Application::FindControl(1, "##PROG_REMIX")->bufferInfo.buffer;
	prog.current = RemixStatic.current;
	prog.expect = RemixStatic.expect;
	if (!RemixStatic.IsRemixing)
	{
		if (WorkThread.joinable())
		{
			WorkThread.join();
			prog.IsVisible = FALSE;
		}
	}
}
// 函数: CheckSpeed
// 功能: 检查输入速度是否在处理区间
VOID CheckSpeed()
{
	constexpr DOUBLE minval = 1.0 / 4.0;
	constexpr DOUBLE maxval = 32.0 / 1.0;
	DOUBLE& speed = *(DOUBLE*)Application::FindControl(1, "##INPUT_SPEED")->bufferInfo.buffer;
	if (speed < minval)
	{
		speed = minval;
	}
	else if (speed > maxval)
	{
		speed = maxval;
	}
	VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
	video.vs->SetFactor(speed);
}
// 函数: ResetFolder
// 功能: 切换回默认界面，重设目录
VOID ResetFolder()
{
	// 仅当没有进行重混任务时能够进行该任务
	if (!RemixStatic.IsRemixing)
	{
		// 获取 VideoInfo 并释放视频流和媒体组织器
		VideoInfo& video = *(VideoInfo*)Application::FindControl(1, "##VIDEORENDER")->bufferInfo.buffer;
		if (video.vs)
		{
			delete video.vs;
			video.vs = nullptr;
		}
		if (video.organizer)
		{
			delete video.organizer;
			video.organizer = nullptr;
		}

		// 获取组合框信息并释放动态分配的 items 数组
		ComboInfo& combo = *(ComboInfo*)Application::FindControl(1, "##COMBO_VIDEO")->bufferInfo.buffer;
		if (combo.items)
		{
			delete[] combo.items;
			combo.items = nullptr;
		}
		combo.count = 0;
		combo.index = 0;

		// 清空剪辑片段列表的缓冲区
		CONST CtrlInfo* listCtrl = Application::FindControl(1, "##CLIPLIST");
		ZeroMemory(listCtrl->bufferInfo.buffer, listCtrl->bufferInfo.size);

		// 清空文件夹路径输入框
		CONST CtrlInfo* inputSrc = Application::FindControl(0, "##INPUT_SRC");
		ZeroMemory(inputSrc->bufferInfo.buffer, inputSrc->bufferInfo.size);

		// 清除保存的文件夹路径指针（避免悬空）
		Application::SetCurrentFolder(NULL);
		
		// 切换布局
		Application::SwitchLayout("Prepare");

		// 重置静态变量
		Application::ResetStatic();
	}
}