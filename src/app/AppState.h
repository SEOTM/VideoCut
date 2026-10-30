#pragma once
#include <windows.h>
#include <stdint.h>
#include "framework/base/ControlSpec.h"

// ================================
// 类名: AppState
// 描述: VideoCut 业务状态与界面联动（静态类）
// 备注: 本类只依赖 Kanat 的公开接口（Dispatch / Layout / CtrlBuffer / AllocatorHub），
//       不修改框架层；视频预览所需的渲染设备入口当前阻塞，见 docs/MIGRATION.md
// ================================
class AppState
{
private:
	// 结构: AppData
	// 描述: 业务状态集中块（生命周期与进程一致）
	struct AppData;
	static AppData* data;

	AppState() = delete;
	~AppState() = delete;
	AppState(const AppState&) = delete;
	AppState& operator=(const AppState&) = delete;

	// 函数: BindBuffers
	// 描述: 为框架未分配缓冲的控件补缓冲（路径输入 / 时间条），并初始化业务初值
	static BOOL BindBuffers();
	// 函数: FindCtrl
	// 描述: 按布局槽序号与控件标签查找控件
	static CtrlInfo* FindCtrl(DWORD index, LPCSTR label);
	// 函数: SwitchLayout
	// 描述: 切换当前业务界面（编辑界面恒为基座；准备界面按内嵌(flex)成员的可见性显隐）
	static BOOL SwitchLayout(DWORD index);
	// 函数: IsEditorActive
	// 描述: 编辑界面当前是否可操作（准备界面以 flex 浮层显示时，编辑界面控件不接受业务操作）
	static BOOL IsEditorActive();
	// 函数: OpenFolder
	// 描述: 扫描目录并按首个分组打开视频
	static BOOL OpenFolder(LPCSTR lpFolder);
	// 函数: OpenVideo
	// 描述: 按分组索引加载视频流并刷新界面缓冲
	static BOOL OpenVideo(DWORD index);
	// 函数: RefreshUi
	// 描述: 把视频时长与初值写回控件缓冲
	static VOID RefreshUi();
	// 函数: SetPlayText
	// 描述: 设置播放按钮文本
	static VOID SetPlayText(BOOL IsPlaying);
	// 函数: SyncTimeControls
	// 描述: 把起止时间写回双手柄滑块与两个可编辑时间条
	static VOID SyncTimeControls();
	// 函数: SyncSeek
	// 描述: 把起止时间写回双手柄滑块（不覆盖正在编辑的时间条数值）
	static VOID SyncSeek();
	// 函数: SyncProgress
	// 描述: 把重混进度写回进度控件缓冲
	static VOID SyncProgress();
	// 函数: DecodeTick
	// 描述: 每帧推进预览：先判活动端时码是否被拖动并定位，再按帧率节流解码，并把当前时码写回活动端
	static VOID DecodeTick();
	// 函数: JoinRemix
	// 描述: 回收已完成的重混线程与片段缓冲
	static VOID JoinRemix();
	// 函数: ReleaseMedia
	// 描述: 释放视频流、文件组织器与组合框选项
	static VOID ReleaseMedia();

public:
	// 函数: Init
	// 描述: 初始化业务状态（须在 Dispatch::Init 之后、宿主启动之前调用）
	static BOOL Init();
	// 函数: Release
	// 描述: 释放业务状态（幂等）
	static VOID Release();
	// 函数: Frame
	// 描述: 每帧入口（先推进业务，再转交 Dispatch::Frame）
	static VOID Frame(VOID* pUser);

	// 函数: OnTitleBar
	// 描述: 标题栏事件（拖动 / 关闭；按触发控件所属布局定位，双击最大化待框架支持运行期重算）
	static VOID OnTitleBar(VOID* obj, uint64_t oper, uint64_t param);
	// 函数: OnBrowse
	// 描述: 浏览文件夹按钮
	static VOID OnBrowse();
	// 函数: OnOpenFolder
	// 描述: 打开文件夹按钮
	static VOID OnOpenFolder();
	// 函数: OnSelectVideo
	// 描述: 视频分组下拉框变更
	static VOID OnSelectVideo();
	// 函数: OnAddSpan
	// 描述: 添加剪辑片段
	static VOID OnAddSpan();
	// 函数: OnSpanSlider
	// 描述: 双手柄范围滑块变更（起点 / 终点）
	static VOID OnSpanSlider(VOID* obj, uint64_t oper);
	// 函数: OnEditBegin
	// 描述: 起点时码提交
	static VOID OnEditBegin();
	// 函数: OnEditEnd
	// 描述: 终点时码提交
	static VOID OnEditEnd();
	// 函数: OnListOperation
	// 描述: 时间片列表行操作（删除 / 选中）
	static VOID OnListOperation(VOID* obj, uint64_t oper, uint64_t param);
	// 函数: OnPlay
	// 描述: 播放 / 暂停
	static VOID OnPlay();
	// 函数: OnSpeed
	// 描述: 倍速输入提交
	static VOID OnSpeed();
	// 函数: OnRemix
	// 描述: 执行重混导出
	static VOID OnRemix();
	// 函数: OnReset
	// 描述: 重设目录并回到准备界面
	static VOID OnReset();
};
