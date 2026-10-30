#pragma once
#include <minwindef.h>
#include <d3d11.h>
#include <stdint.h>
// 前向声明FFmpeg相关结构
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct AVPacket;

// ================================
// 类名: VideoStream
// 说明: 视频流
// ================================
class VideoStream
{
private:
	// 内联函数
	union str
	{
		LPCSTR cstr;
		LPCWSTR cwstr;
	};
	// 结构
	struct VideoNode
	{
		VideoNode* next;	// 后继节点
		LPCSTR file;		// 视频文件
		int64_t base;		// 文件 BASE PTS 偏移
		int64_t offset;		// 全局偏移
		int64_t duration;	// 视频时长
		struct
		{
			INT video;		// 视频流索引
			INT voice;		// 音频流索引
		} index;
	};
	struct VideoInfo
	{
		struct
		{
			SIZE size;		// 视频分辨率
			struct {
				INT num;
				INT den;
			} framerate;		// 视频帧率
		} Fixed;				// 固定信息
		int64_t duration;	// 视频总时长
		int64_t offset;		// 时间偏移
		struct
		{
			INT video;		// 视频流索引
			INT voice;		// 音频流索引
		} index;
	};
	// 成员方法

	// 函数: ReadMetadata
	// 功能: 读取视频元数据
	BOOL ReadMetadata(VideoInfo& info);

	// DX11 对象
	ID3D11Device* pDevice = NULL;				// D3D11 设备
	ID3D11DeviceContext* pDeviceContext = NULL;	// D3D11 设备上下文
	ID3D11ShaderResourceView* pSRV = NULL;		// 着色器资源视图
	ID3D11Texture2D* pTexture = NULL;			// 纹理对象

	// FFmpeg对象 (不可复用部分)
	//			  (仅活动对象加载文件，在不解码时，默认最后一个加载的文件为活动对象)
	AVFormatContext* pFormatContext = NULL;		// 格式上下文
	AVCodecContext* pCodecContext = NULL;		// 编码上下文
	// FFmpeg对象 (可复用部分)
	SwsContext* pSwsContext = NULL;				// 
	AVFrame* pFrame = NULL;						// 帧
	AVFrame* pFrameRGB = NULL;
	AVPacket* pPacket = NULL;
	// 其他成员
	VideoNode* head = NULL;						// 视频链表头
	VideoNode* tail = NULL;						// 视频链表尾
	VideoNode* cur = NULL;						// 当前视频节点
	LPBYTE pBuffer = NULL;						// 视频数据缓冲
	int64_t qwTimeCur = 0;						// 当前视频播放时间 (基于时间基数)
	DWORD dwBytes = 0;							// 视频数据缓冲大小
	VideoInfo GlobalInfo;						// 视频全局信息
	DOUBLE SpeedFactor;							// 视频播放倍速
	int64_t qwFrameNext;						// 下帧播放时间
	int64_t IncFrame;							// 帧间递增数值
	int64_t IncPlay;							// 播放递增数值
public:
	// 成员方法

	// 函数: VideoStream
	// 功能: 构造函数
	VideoStream(ID3D11Device* device, ID3D11DeviceContext* context);
	// 函数: ~VideoStream
	// 功能: 析构函数
	~VideoStream();
	// 函数: InitializeReusableObjects
	// 功能: 初始化可复用 FFmpeg 对象以及 DX11 纹理对象
	BOOL InitializeReusableObjects(VideoInfo info);
	// 函数: InitializeDecoderContext
	// 功能: 根据视频节点创建解码器上下文，并为视频帧分配缓冲区
	BOOL InitializeDecoderContext(VideoNode* node);

	// 函数: Load
	// 功能: 加载视频文件(后续添加文件时追加)
	BOOL Load(LPCSTR file);
	// 函数: Close
	// 功能: 关闭视频文件
	VOID Close();
	// 函数: SerCurrentTick
	// 功能: 设置当前处理时间 (基于时间基数)
	VOID SetCurrentTick(int64_t time);
	// 函数: GetCurrentTick
	// 功能: 获取当前处理时间 (基于时间基数)
	int64_t GetCurrentTick();
	// 函数: SetCurrentMilliseconds
	// 功能: 设置当前处理时间 (基于毫秒)
	VOID SetCurrentMilliseconds(int64_t ms);
	// 函数: GetCurrentMilliseconds
	// 功能: 获取当前处理时间（基于毫秒）
	int64_t GetCurrentMilliseconds();
	// 函数: GetDuration
	// 功能: 获取视频总时长（基于时间基数）
	int64_t GetDuration();
	// 函数: GetMilliseconds
	// 功能: 获取视频总时长（基于毫秒）
	int64_t GetMilliseconds();
	// 函数: GetFPS
	// 功能: 获取视频每秒帧率
	DWORD GetFPS();
	// 函数: GetResolution
	// 功能: 获取视频分辨率
	SIZE GetResolution();
	// 函数: GetSRV
	// 功能: 着色器资源视图
	ID3D11ShaderResourceView* GetSRV();
	// 函数: DecodeOper
	// 功能: 逐帧解码操作
	BOOL DecodeOper();
	// 函数: RemixClips
	// 功能: 根据时间片段重新裁剪和重组视频
	BOOL RemixClips(LPSTR lpFilename,DOUBLE speed, int64_t& current,BOOL& IsRemixing, VOID* data);
	// 函数: SetFactor
	// 功能: 设置播放倍数
	BOOL SetFactor(DOUBLE fatcor);
};