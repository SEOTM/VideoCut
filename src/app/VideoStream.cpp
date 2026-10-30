#include "VideoStream.h"
#include <assert.h>
#include <chrono>
#include "VideoEncoder.h"
// FFmpeg 头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avutil.lib")

// ================================
// 辅助函数
// ================================

// 函数: StreamWrite
// 功能: 流复制写入指定范围的数据
int64_t StreamWrite(
	AVFormatContext* fmtDest, int audio_dest, int video_dest,
	AVFormatContext* fmtOper, int audio_oper, int video_oper,
	AVPacket* pkt, int64_t qwOffset, int64_t qwLeft, int64_t qwRight
);

// 函数: ReadMetadata
// 功能: 读取视频元数据
BOOL VideoStream::ReadMetadata(VideoStream::VideoInfo& info)
{
	BOOL ret = FALSE;
	// 检查格式上下文是否存在
	if (pFormatContext) {
		// 读取视频元数据


		// 查找视频流和音频流索引
		int videoStreamIndex = -1;
		int audioStreamIndex = -1;
		for (unsigned int i = 0; i < pFormatContext->nb_streams; i++) {
			if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				if (videoStreamIndex == -1) // 只记录第一个视频流索引
				{
					videoStreamIndex = i;
				}
			}
			else if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				audioStreamIndex = i;
			}
			// 虽然只记录一个音视频流
			// 但是输出其他流的信息以便调试
			// 1. 输出流类型编号
			AVMediaType mediaType = pFormatContext->streams[i]->codecpar->codec_type;
			// 2. 输出流索引
			int streamIndex = i;
			// 3. 输出流编码格式
			AVCodecID codecID = pFormatContext->streams[i]->codecpar->codec_id;
			// DEBUG 输出流信息
			printf("    Stream %d: Type=%d, CodecID=%d\n", streamIndex, mediaType, codecID);
			// 对于视频流，输出分辨率和帧率
			if (mediaType == AVMEDIA_TYPE_VIDEO)
			{
				int width = pFormatContext->streams[i]->codecpar->width;
				int height = pFormatContext->streams[i]->codecpar->height;
				AVRational frameRate = av_guess_frame_rate(pFormatContext, pFormatContext->streams[i], NULL);
				printf("      Video Stream: Resolution=%dx%d, FrameRate=%d/%d fps\n",
					width, height, frameRate.num, frameRate.den);
			}
			// 对于音频流，输出采样率
			else if (mediaType == AVMEDIA_TYPE_AUDIO)
			{
				int sampleRate = pFormatContext->streams[i]->codecpar->sample_rate;
				printf("      Audio Stream: SampleRate=%d Hz\n", sampleRate);
			}
		}
		// 开始读取元数据
		AVStream& videoStream = *pFormatContext->streams[videoStreamIndex];
		AVCodecParameters* codecParameters = pFormatContext->streams[videoStreamIndex]->codecpar;
		// 获取分辨率
		SIZE readsize;
		readsize.cx = codecParameters->width;
		readsize.cy = codecParameters->height;
		// 获取持续时间
		int64_t dwDuration = pFormatContext->duration;
		int64_t qwOffset = 0;
		{
			// 保存当前位置（实际上不需要，因为之后要 seek 到开头）
			// 先 seek 到文件开头，确保读到第一个包
			AVPacket* pkt_ptr = av_packet_alloc();
			AVPacket& pkt = *pkt_ptr;
			pkt.data = NULL;
			pkt.size = 0;
			AVRational timebase = pFormatContext->streams[videoStreamIndex]->time_base;
			int64_t base_pts = AV_NOPTS_VALUE;
			int64_t base_dts = AV_NOPTS_VALUE;
			while (av_read_frame(pFormatContext, &pkt) >= 0)
			{
				if (pkt.stream_index == videoStreamIndex)
				{
					if (pkt.pts != AV_NOPTS_VALUE)
					{
						base_pts = (pkt.pts * AV_TIME_BASE * timebase.num) / timebase.den;
					}
					if (pkt.dts != AV_NOPTS_VALUE)
					{
						base_dts = (pkt.dts * AV_TIME_BASE * timebase.num) / timebase.den;
					}
					av_packet_unref(&pkt);
					break;
				}
				av_packet_unref(&pkt);
			}
			// 再次 seek 回开头，以便后续解码从头开始
			av_seek_frame(pFormatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
			// 释放临时 pkt
			av_packet_free(&pkt_ptr);

			qwOffset = base_dts;  // 即使为 AV_NOPTS_VALUE 也没关系
			// printf("base_dts: %lld\n", node->base);
		}
		// 获取帧率
		AVRational dwFrameRate = av_guess_frame_rate(pFormatContext, &videoStream, NULL);
		// 填充字段
		info.Fixed.size = readsize;
		info.Fixed.framerate = { dwFrameRate.num, dwFrameRate.den };
		info.duration = dwDuration;
		info.offset = qwOffset;
		info.index = { videoStreamIndex, audioStreamIndex };

		// DEBUG 输出元数据
		printf(
			"    Metadata:\n"
			"      Resolution : %dx%d\n"
			"      Duration   : %lld ms\n"
			"      FrameRate  : %d/%d fps\n"
			"      TimeOffset : %lld ms\n",
			info.Fixed.size.cx, info.Fixed.size.cy,
			info.duration / (AV_TIME_BASE / 1000), // 转换为毫秒
			info.Fixed.framerate.num, info.Fixed.framerate.den,
			info.offset / 1000
		);

		// 设置返回值
		ret = TRUE; // 成功读取元数据
	}
	// 返回结果
	return ret;
}
VideoStream::VideoStream(ID3D11Device* device, ID3D11DeviceContext* context)
	: pDevice(device), pDeviceContext(context)
{
	// 增加引用计数
	if (pDevice) pDevice->AddRef();
	if (pDeviceContext) pDeviceContext->AddRef();
	// 初始化其他成员变量
	GlobalInfo = {};
	SpeedFactor = 1.0;
}

VideoStream::~VideoStream()
{
	Close();

	// 释放D3D11对象
	if (pSRV) pSRV->Release();
	if (pTexture) pTexture->Release();

	if (pDeviceContext) pDeviceContext->Release();
	if (pDevice) pDevice->Release();

}
// 函数: InitializeReusableObjects
// 功能: 初始化可复用 FFmpeg 对象以及 DX11 纹理对象
BOOL VideoStream::InitializeReusableObjects(VideoInfo info)
{
	// 设置返回变量
	BOOL ret = FALSE;
	// 变量声明区，避免goto ERR 时跳过变量声明
	int align = 32; // 32字节对齐，满足AVFrameRGB的要求
	// 1. 初始化 DX11 纹理对象
	// 备注: 设备为空(宿主未登记或渲染设施未就绪)时跳过纹理创建,
	//       视频解码 / 时长 / 重混仍可用, 仅预览画面不可用
	if (pDevice != NULL && pDeviceContext != NULL)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		desc.Width = info.Fixed.size.cx;
		desc.Height = info.Fixed.size.cy;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;       // RGBA 格式
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DYNAMIC;               // 动态使用，允许CPU写入
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;    // 绑定为着色器资源
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;   // 允许CPU写入
		HRESULT hr = pDevice->CreateTexture2D(&desc, NULL, &pTexture);
		if (FAILED(hr))
		{
			goto ERR;
		}
		// 创建着色器资源视图
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		hr = pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV);
		if (FAILED(hr))
		{
			goto ERR;
		}
	}
	// 2. 初始化可复用 FFmpeg 对象
	// 2.1 初始化 SwsContext
	pSwsContext = sws_getContext(
		info.Fixed.size.cx, info.Fixed.size.cy, AV_PIX_FMT_YUV420P, // 输入格式
		info.Fixed.size.cx, info.Fixed.size.cy, AV_PIX_FMT_RGBA,   // 输出格式
		SWS_BILINEAR, NULL, NULL, NULL);
	if (!pSwsContext)
	{
		goto ERR;
	}
	// 2.2 初始化 AVFrame 和 AVPacket
	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();
	pPacket = av_packet_alloc();
	if (!pFrame || !pFrameRGB || !pPacket)
	{
		goto ERR;
	}
	// 2.3 分配 RGB 帧缓冲
	pFrameRGB->format = AV_PIX_FMT_RGBA;
	pFrameRGB->width = info.Fixed.size.cx;
	pFrameRGB->height = info.Fixed.size.cy;
	av_frame_get_buffer(pFrameRGB, align);
	// 设置返回值
	ret = TRUE; // 成功初始化可复用对象
	goto END;	// 跳过错误时的资源释放
ERR:
	// 错误标签，依次释放已分配的资源
	if (pSwsContext) sws_freeContext(pSwsContext);
	if (pFrame) av_frame_free(&pFrame);
	if (pFrameRGB) av_frame_free(&pFrameRGB);
	if (pPacket) av_packet_free(&pPacket);
	if (pSRV) { pSRV->Release(); pSRV = NULL; }
	if (pTexture) { pTexture->Release(); pTexture = NULL; }
END:
	return ret;
}
// 函数: InitializeDecoderContext
// 功能: 根据视频节点创建解码器上下文，并为视频帧分配缓冲区
BOOL VideoStream::InitializeDecoderContext(VideoNode* node)
{
	// 设置返回变量
	BOOL ret = FALSE;
	// 1. 创建解码器上下文
	AVCodecParameters* codecParameters = pFormatContext->streams[node->index.video]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);

	if (!codec)
	{
		goto ERR;
	}
	pCodecContext = avcodec_alloc_context3(codec);
	if (!pCodecContext)
	{
		goto ERR;
	}
	if (avcodec_parameters_to_context(pCodecContext, codecParameters) < 0)
	{
		goto ERR;
	}
	if (avcodec_open2(pCodecContext, codec, NULL) < 0)
	{
		goto ERR;
	}

	// 2. 帧缓冲部分复用，无需重新分配

	// 设置返回值
	ret = TRUE; // 成功初始化解码器上下文
	goto END;	// 跳过错误时的资源释放
ERR:
	if (pCodecContext) { avcodec_free_context(&pCodecContext); }
END:
	return ret;
}
// 函数: Load
// 功能: 加载视频文件(后续添加文件时追加)
BOOL VideoStream::Load(LPCSTR file)
{
	// 设置返回变量
	BOOL ret = FALSE;
	VideoNode node{};
	VideoInfo info{};
	VideoNode* oper = cur;
	BOOL IsDoNext = TRUE; // 是否执行追加后续步骤
	// 保存旧 CodecContext 和 FormatContext，以便在追加失败时恢复
	AVCodecContext* oldCodecContext = NULL;
	AVFormatContext* oldFormatContext = NULL;

	// 初始填充 node
	node.file = file;
	node.next = NULL;
	node.base = AV_NOPTS_VALUE;
	// 打开视频文件
	if (cur)
	{
		// 存在活动视频对象，先保存不可复用资源，以便在追加失败时恢复，并将指针置空，以便后续加载新视频文件
		// 不可复用资源包括：pFormatContext、pCodecContext
		oldFormatContext = pFormatContext;
		oldCodecContext = pCodecContext;
		pFormatContext = NULL;
		pCodecContext = NULL;
	}
	// 打开视频文件
	if (avformat_open_input(&pFormatContext, file, NULL, NULL) == 0)
	{
		// 查找流信息
		if (avformat_find_stream_info(pFormatContext, NULL) < 0)
		{
			IsDoNext = FALSE; // 不执行追加后续步骤
		}
		else
		{
			// 检查是否存在视频流
			int videoStreamIndex = -1;
			for (unsigned int i = 0; i < pFormatContext->nb_streams; i++) {
				if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					videoStreamIndex = i;
					break;
				}
			}
			if (videoStreamIndex != -1)
			{
				// 获取视频信息
				IsDoNext = ReadMetadata(info);
			}
			else
			{
				IsDoNext = FALSE; // 不执行追加后续步骤
			}
		}
	}
	if (IsDoNext)
	{
		// 填充节点信息
		node.offset = GlobalInfo.duration; // 全局偏移为当前全局时长
		node.duration = info.duration; // 视频时长
		node.base = info.offset;
		node.index = { info.index.video, info.index.voice }; // 视频流和音频流索引
		// 校验格式信息
		if (cur)
		{
			int cmp = memcmp(&info.Fixed, &GlobalInfo.Fixed, sizeof(info.Fixed));
			if (cmp != 0)
			{
				// 重新加载 cur 节点指向视频文件
				IsDoNext = FALSE; // 不执行追加后续步骤
			}
		}
		else
		{
			// 首次加载视频，初始化复用资源
			IsDoNext = InitializeReusableObjects(info);
			oper = &node; // 将操作节点指向新节点
			// 计算帧间隔 IncFrame
			if (info.Fixed.framerate.num > 0 && info.Fixed.framerate.den > 0)
			{
				IncFrame = av_rescale_q(1, AVRational{ info.Fixed.framerate.den, info.Fixed.framerate.num }, AV_TIME_BASE_Q);
				IncPlay = (int64_t)(IncFrame * SpeedFactor);
			}
		}
	}
	// 初始化编码器
	if (IsDoNext)
	{
		IsDoNext = InitializeDecoderContext(oper);
	}
	if (IsDoNext)
	{
		// 更新指针(同步更新 cur 指针和 tail 指针)
		oper = new VideoNode(node);
		if (cur)
		{
			cur->next = oper;
			cur = oper;
			tail = oper;
		}
		else
		{
			cur = oper;
			tail = oper;
			head = oper;
		}
		// 更新全局信息
		GlobalInfo.Fixed = info.Fixed;
		GlobalInfo.duration += info.duration;
		// 释放旧视频文件资源
		if (oldCodecContext) { avcodec_free_context(&oldCodecContext); }
		if (oldFormatContext) { avformat_close_input(&oldFormatContext); }
		// 更新返回值
		ret = TRUE; // 成功加载视频并追加到链表
	}
	if (!ret)
	{
		// 释放新加载的视频文件资源
		if (pCodecContext) { avcodec_free_context(&pCodecContext); }
		if (pFormatContext) { avformat_close_input(&pFormatContext); }
		// 追加失败，恢复旧的 CodecContext 和 FormatContext
		pFormatContext = oldFormatContext;
		pCodecContext = oldCodecContext;
	}
	return ret;
}

VOID VideoStream::Close()
{
	// 关闭 活动 视频文件资源
	if (pCodecContext) { avcodec_free_context(&pCodecContext); pCodecContext = NULL; }
	if (pFormatContext) { avformat_close_input(&pFormatContext); pFormatContext = NULL; }
	// 释放视频链表资源
	VideoNode* iter = head;
	while (iter)
	{
		VideoNode* next = iter->next;
		delete iter;
		iter = next;
	}
	head = NULL;
	tail = NULL;
	cur = NULL;
	// 释放 FFmpeg 可复用资源
	if (pSwsContext) { sws_freeContext(pSwsContext); pSwsContext = NULL; }
	if (pFrame) { av_frame_free(&pFrame); pFrame = NULL; }
	if (pFrameRGB) { av_frame_free(&pFrameRGB); pFrameRGB = NULL; }
	if (pPacket) { av_packet_free(&pPacket); pPacket = NULL; }
	if (pBuffer) { delete[] pBuffer; pBuffer = NULL; dwBytes = 0; }
	// 释放 DX11 资源
	if (pSRV) { pSRV->Release(); pSRV = NULL; }
	if (pTexture) { pTexture->Release(); pTexture = NULL; }
	// 重置全局信息
	GlobalInfo = {};
	// 重置其他信息
	SpeedFactor = 1.0;
	IncFrame = 0;
	IncPlay = 0;
	qwFrameNext = 0;
}
// 函数: SetCurrentTick
// 功能: 设置当前处理时间 (基于时间基数)
VOID VideoStream::SetCurrentTick(int64_t time)
{
	// 前向声明变量，避免 goto END 时跳过变量声明
	VideoNode* iter = head;
	VideoNode* oper = NULL;
	int64_t nodeTime = 0;
	// 两个特别说明：
	// 1. 使用 AV_TIME_BASE 作为时间基数
	// 2. 使用 -1 作为视频流，而非指定的视频流索引
	//    这是因为在当前设计中，视频流和音频流的时间基数通常不一致
	//    虽然当前设计中尚未涉及音频解码，但是为了后续可能的音频解码功能扩展
	//	  使用 AV_TIME_BASE 作为统一的时间基数，可以避免后续在处理音频时间复杂的转换问题
	//    而要使用 AV_TIME_BASE 作为时间基数，则需要在使用 av_seek_frame 或 avformat_seek_file 等函数进行时间定位时
	//    指定 -1 作为流索引，以便 FFmpeg 使用全局时间基数进行定位

	// 根据 time 寻找对应的节点
	while (iter)
	{
		if (time < iter->offset + iter->duration)
		{
			oper = iter;
			break;
		}
		iter = iter->next;
	}
	if (!oper)
	{
		goto END;
	}
	// 如果找到的节点与当前节点不同，切换到新节点
	if (oper != cur)
	{
		// 保存旧有节点资源，以便在切换失败时恢复
		AVFormatContext* oldFormatContext = pFormatContext;
		AVCodecContext* oldCodecContext = pCodecContext;
		pFormatContext = NULL;
		pCodecContext = NULL;
		// 执行标志
		BOOL IsDoNext = TRUE;
		// 加载新节点视频文件
		if (avformat_open_input(&pFormatContext, oper->file, NULL, NULL) == 0)
		{
			// 查找流信息
			if (avformat_find_stream_info(pFormatContext, NULL) < 0)
			{
				IsDoNext = FALSE; // 不执行追加后续步骤
			}
			else
			{
				// 初始化解码器上下文
				IsDoNext = InitializeDecoderContext(oper);
			}
		}
		else
		{
			IsDoNext = FALSE;
		}
		if (IsDoNext)
		{
			cur = oper; // 切换到新节点
			// 释放旧节点资源
			if (oldCodecContext) { avcodec_free_context(&oldCodecContext); }
			if (oldFormatContext) { avformat_close_input(&oldFormatContext); }
		}
		else
		{
			// 切换失败，恢复旧节点资源
			pFormatContext = oldFormatContext;
			pCodecContext = oldCodecContext;
		}
	}
	// 设置当前时间
	qwTimeCur = time;
	qwFrameNext = time;
	// 计算 Node 内部时间
	nodeTime = time - cur->offset + cur->base;
	// 使用 av_seek_frame 定位到 nodeTime
	if (av_seek_frame(pFormatContext, -1, nodeTime, AVSEEK_FLAG_BACKWARD) < 0)
	{
		goto END;
	}
	// 清空解码器缓冲区
	avcodec_flush_buffers(pCodecContext);
END:
	return;
}
// 函数: GetCurrentTick
// 功能: 获取当前处理时间 (基于时间基数)
int64_t VideoStream::GetCurrentTick()
{
	return qwTimeCur;
}
// 函数: SetCurrentMilliseconds
// 功能: 设置当前处理时间 (基于毫秒)
VOID VideoStream::SetCurrentMilliseconds(int64_t ms)
{
	SetCurrentTick(ms * (AV_TIME_BASE / 1000));
}
// 函数: GetCurrentMilliseconds
// 功能: 获取当前处理时间（基于毫秒）
int64_t VideoStream::GetCurrentMilliseconds()
{
	return (qwTimeCur * 1000) / AV_TIME_BASE;
}
// 函数: GetDuration
// 功能: 获取视频总时长（基于时间基数）
int64_t VideoStream::GetDuration()
{
	return GlobalInfo.duration;
}
// 函数: GetMilliseconds
// 功能: 获取视频总时长（基于毫秒）
int64_t VideoStream::GetMilliseconds()
{
	return GlobalInfo.duration * 1000 / AV_TIME_BASE;
}

DWORD VideoStream::GetFPS()
{
	// 前向声明变量，避免 goto END 时跳过变量声明
	DWORD fps = 0;
	// 检查帧率分母是否为0，避免除以0错误
	if (GlobalInfo.Fixed.framerate.den == 0)
	{
		goto END;
	}
	// 计算帧率
	// 公式：帧率 = 帧率分子 / 帧率分母
	fps = (DWORD)((double)GlobalInfo.Fixed.framerate.num / GlobalInfo.Fixed.framerate.den);
END:
	return fps;
}
// 函数: GetResolution
// 功能: 获取视频分辨率
SIZE VideoStream::GetResolution()
{
	return GlobalInfo.Fixed.size;
}
// 函数: GetSRV
// 功能: 着色器资源视图
ID3D11ShaderResourceView* VideoStream::GetSRV()
{
	return pSRV;
}
BOOL VideoStream::DecodeOper()
{
	BOOL ret = FALSE;

	if (!pFormatContext || !pCodecContext || !cur)
		goto END;

	// 1. 推进播放时间
	qwTimeCur += IncPlay;
	qwTimeCur = qwTimeCur < GlobalInfo.duration ? qwTimeCur : GlobalInfo.duration;
	// 判断播放时间是否位于当前帧
	if (qwTimeCur >= cur->offset + cur->duration)
	{
		SetCurrentTick(qwTimeCur);
	}
	// 2. 检查是否需要解码新帧
	if (qwTimeCur >= qwFrameNext)
	{
		// 循环读取直到解码出一帧视频
		while (av_read_frame(pFormatContext, pPacket) >= 0)
		{
			if (pPacket->stream_index == cur->index.video)
			{
				if (avcodec_send_packet(pCodecContext, pPacket) == 0)
				{
					int retCode = avcodec_receive_frame(pCodecContext, pFrame);
					// 帧更新时间
					qwFrameNext += IncFrame;
					if (qwFrameNext < qwTimeCur)
					{
						// 当前帧不是需要读取的帧
						// 跳过
						av_packet_unref(pPacket);
						continue;
					}
					if (retCode == 0)
					{
						// 颜色空间转换
						sws_scale(pSwsContext,
							pFrame->data, pFrame->linesize,
							0, GlobalInfo.Fixed.size.cy,
							pFrameRGB->data, pFrameRGB->linesize);

						// 更新 D3D11 纹理
						// 备注: 无设备时跳过上传, 解码与时间推进保持不变
						D3D11_MAPPED_SUBRESOURCE mappedResource;
						if (pDeviceContext != NULL && pTexture != NULL &&
							SUCCEEDED(pDeviceContext->Map(pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
						{
							BYTE* dest = (BYTE*)mappedResource.pData;
							BYTE* src = pFrameRGB->data[0];
							for (int y = 0; y < GlobalInfo.Fixed.size.cy; y++)
							{
								memcpy(dest, src, GlobalInfo.Fixed.size.cx * 4);
								dest += mappedResource.RowPitch;
								src += pFrameRGB->linesize[0];
							}
							pDeviceContext->Unmap(pTexture, 0);
						}

						ret = TRUE;  // 成功解码一帧
						av_packet_unref(pPacket);
						goto END;    // 直接返回，保留当前帧
					}
				}
			}
			av_packet_unref(pPacket);
		}
		// 没有更多帧可读（文件结束）
		ret = FALSE;
	}
	else
	{
		// 未达到下一帧时间，无需解码
		ret = FALSE;
	}

END:
	// 确保 packet 被释放（安全处理）
	if (pPacket->buf)
	{
		av_packet_unref(pPacket);
	}
	return ret;
}
// 函数: RemixClips
// 功能: 根据时间片段重新裁剪和重组视频
BOOL VideoStream::RemixClips(LPSTR lpFilename, DOUBLE speed, int64_t& current, BOOL& IsRemixing, VOID* data)
{
	// 解析传入的片段信息
	struct ClipInfo
	{
		int64_t count;
		struct
		{
			int64_t left;
			int64_t right;
		} spans[];
	}*clips = (ClipInfo*)data;
	IsRemixing = TRUE;
	// 局部变量集中声明（便于goto清理）
	BOOL ret = FALSE;
	AVFormatContext* ofmt_ctx = NULL;
	AVFormatContext* fmt_oper = NULL;
	AVPacket* pkt = NULL;
	const AVOutputFormat* ofmt = NULL;
	AVStream* in_video_stream = NULL;
	AVStream* in_audio_stream = NULL;
	AVStream* out_video_stream = NULL;
	AVStream* out_audio_stream = NULL;
	int video_dest = -1, audio_dest = -1;
	int err = 0;
	char errors[1024];

	int64_t clip_idx = 0;
	int64_t current_left = clips->spans[0].left;
	int64_t current_right = clips->spans[0].right;
	int64_t output_offset = 0;          // 输出时间线上的累积偏移

	VideoNode* node = head;

	constexpr size_t bufSize = MAX_PATH * 3;
	// 记录解码-编码过程开始时间
	auto __venc_start_time = std::chrono::high_resolution_clock::now();
	VideoNode* first_node = head;
	// 设置日志级别
	av_log_set_level(AV_LOG_QUIET);
	// av_log_set_level(AV_LOG_TRACE);

	// 创建编码器
	// VideoEncoder encoder(lpFilename, GlobalInfo.Fixed.size, { GlobalInfo.Fixed.framerate.num,GlobalInfo.Fixed.framerate.den });
	VideoEncoder* encoder = VideoEncoder::Create(
		lpFilename,
		GlobalInfo.Fixed.size,
		{ GlobalInfo.Fixed.framerate.num,GlobalInfo.Fixed.framerate.den },
		speed,
		ret
	);
	if (!encoder)
	{
		goto END;
	}

	// return FALSE;
	// 参数校验
	if (!clips || clips->count == 0 || !head)
	{
		av_log(NULL, AV_LOG_ERROR, "Invalid parameters or no video loaded.\n");
		return FALSE;
	}

	// ==================== 创建输出上下文 ====================
	// 使用第一个节点文件获取流信息（所有节点格式一致）


	/*
	if ((err = avformat_open_input(&fmt_oper, first_node->file, NULL, NULL)) < 0)
	{
		av_strerror(err, errors, sizeof(errors));
		av_log(NULL, AV_LOG_ERROR, "Failed to open first node file: %s\n", errors);
		goto cleanup;
	}
	if ((err = avformat_find_stream_info(fmt_oper, NULL)) < 0)
	{
		av_strerror(err, errors, sizeof(errors));
		av_log(NULL, AV_LOG_ERROR, "Failed to find stream info: %s\n", errors);
		goto cleanup;
	}

	// 分配输出上下文
	if ((err = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, szFullname)) < 0)
	{
		av_strerror(err, errors, sizeof(errors));
		av_log(NULL, AV_LOG_ERROR, "Failed to alloc output context: %s\n", errors);
		goto cleanup;
	}
	ofmt = ofmt_ctx->oformat;

	// 获取输入流（使用节点记录的索引）
	in_video_stream = fmt_oper->streams[first_node->index.video];
	if (first_node->index.voice != -1)
		in_audio_stream = fmt_oper->streams[first_node->index.voice];

	// 创建输出视频流
	out_video_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!out_video_stream)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to create video output stream.\n");
		goto cleanup;
	}
	if ((err = avcodec_parameters_copy(out_video_stream->codecpar, in_video_stream->codecpar)) < 0)
	{
		av_strerror(err, errors, sizeof(errors));
		av_log(NULL, AV_LOG_ERROR, "Failed to copy video codec parameters: %s\n", errors);
		goto cleanup;
	}
	out_video_stream->codecpar->codec_tag = 0;
	out_video_stream->time_base = in_video_stream->time_base; // 保持时间基一致
	video_dest = out_video_stream->index;

	// 如果有音频，创建输出音频流
	if (in_audio_stream)
	{
		out_audio_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_audio_stream)
		{
			av_log(NULL, AV_LOG_ERROR, "Failed to create audio output stream.\n");
			goto cleanup;
		}
		if ((err = avcodec_parameters_copy(out_audio_stream->codecpar, in_audio_stream->codecpar)) < 0)
		{
			av_strerror(err, errors, sizeof(errors));
			av_log(NULL, AV_LOG_ERROR, "Failed to copy audio codec parameters: %s\n", errors);
			goto cleanup;
		}
		out_audio_stream->codecpar->codec_tag = 0;
		out_audio_stream->time_base = in_audio_stream->time_base;
		audio_dest = out_audio_stream->index;
	}

	// 关闭临时打开的第一个节点文件
	avformat_close_input(&fmt_oper);
	fmt_oper = NULL;

	// 打开输出文件
	if (!(ofmt->flags & AVFMT_NOFILE))
	{
		if ((err = avio_open(&ofmt_ctx->pb, szFullname, AVIO_FLAG_WRITE)) < 0)
		{
			av_strerror(err, errors, sizeof(errors));
			av_log(NULL, AV_LOG_ERROR, "Failed to open output file: %s\n", errors);
			goto cleanup;
		}
	}

	// 写入文件头
	if ((err = avformat_write_header(ofmt_ctx, NULL)) < 0)
	{
		av_strerror(err, errors, sizeof(errors));
		av_log(NULL, AV_LOG_ERROR, "Failed to write header: %s\n", errors);
		goto cleanup;
	}

	// 分配复用 packet
	pkt = av_packet_alloc();
	if (!pkt)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to alloc packet.\n");
		goto cleanup;
	}
	*/
	// ==================== 遍历节点处理片段 ====================

	while (node && clip_idx < clips->count)
	{
		int64_t node_start = node->offset;
		int64_t node_end = node->offset + node->duration;

		// 如果当前 clip 完全在当前节点之后，跳过该节点
		if (current_left >= node_end)
		{
			node = node->next;
			continue;
		}

		// 当前节点可能与 clip 有重叠，循环处理该节点内所有相关 clip
		while (clip_idx < clips->count && current_left < node_end)
		{
			// 计算在节点内部的时间区间（基于 AV_TIME_BASE）
			int64_t clip_start_in_node = (current_left > node_start) ? (current_left - node_start) : 0;
			int64_t clip_end_in_node = (current_right < node_end) ? (current_right - node_start) : (node_end - node_start);

			if (clip_start_in_node < clip_end_in_node) // 有效区间
			{
				// 调试输出
				printf(u8"[Remix]\n"
					u8"操作文件: %s\n"
					u8"全局偏移: %lld us \n"
					u8"全局时间: [%lld, %lld]\n"
					u8"剪切时间: [%lld, %lld]\n\n",
					node->file, output_offset, current_left, current_right, clip_start_in_node, clip_end_in_node);
				/*
				// 打开当前节点文件
				if ((err = avformat_open_input(&fmt_oper, node->file, NULL, NULL)) < 0)
				{
					av_strerror(err, errors, sizeof(errors));
					av_log(NULL, AV_LOG_ERROR, "Failed to open node file %s: %s\n", node->file, errors);
					goto cleanup;
				}


				// 为确保流索引有效，可调用 avformat_find_stream_info，但节点记录索引应可靠
				if ((err = avformat_find_stream_info(fmt_oper, NULL)) < 0)
				{
					av_strerror(err, errors, sizeof(errors));
					av_log(NULL, AV_LOG_ERROR, "Failed to find stream info for %s: %s\n", node->file, errors);
					avformat_close_input(&fmt_oper);
					goto cleanup;
				}

				*/

				// 调用 StreamWrite 写入区间
				/*
				int64_t written_end = StreamWrite(
					ofmt_ctx,
					audio_dest, video_dest,
					fmt_oper,
					node->index.voice, node->index.video,
					pkt,
					output_offset,
					clip_start_in_node, clip_end_in_node
				);
				*/
				// 调用 encoder
				encoder->EncodingWrite(node->file, clip_start_in_node, clip_end_in_node, current);

				// 更新输出偏移（实际写入时长）
				// output_offset = written_end;

				// 关闭当前节点文件
				avformat_close_input(&fmt_oper);
				fmt_oper = NULL;
			}

			// 根据 clip 是否在节点内结束来更新 clip 状态
			if (current_right <= node_end)
			{
				// 当前 clip 在本节点内结束，移动到下一个 clip
				clip_idx++;
				if (clip_idx < clips->count)
				{
					current_left = clips->spans[clip_idx].left;
					current_right = clips->spans[clip_idx].right;
				}
				else
				{
					break; // 所有 clip 处理完毕
				}
			}
			else
			{
				// 当前 clip 跨节点，更新左边界为节点结束，继续下一个节点
				current_left = node_end;
				// 注意：不增加 clip_idx，继续处理同一个 clip 的剩余部分
				break; // 退出内层循环，让外层进入下一个节点
			}
		}

		node = node->next; // 移动到下一个节点
	}
	/*

	// 写入文件尾
	av_write_trailer(ofmt_ctx);

	ret = TRUE;

cleanup:
	// 资源清理
	if (fmt_oper)
		avformat_close_input(&fmt_oper);
	if (pkt)
		av_packet_free(&pkt);
	if (ofmt_ctx)
	{
		if (!(ofmt->flags & AVFMT_NOFILE))
			avio_closep(&ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
	}

	*/
	printf(lpFilename);
	putchar(10);
	encoder->Finish();
END:
	av_log_set_level(AV_LOG_WARNING);
	auto __venc_end_time = std::chrono::high_resolution_clock::now();
	auto __venc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(__venc_end_time - __venc_start_time).count();
	// 同时输出到控制台以便在无 av_log 可见性的环境中查看
	printf(u8"写入耗时: %lld ms\n", (long long)__venc_ms);
	// 备注: 片段块与输出文件名归调用方所有, 线程只读不释放
	IsRemixing = FALSE;
	return ret;
}
// 函数: SetFactor
// 功能: 设置播放倍数
BOOL VideoStream::SetFactor(DOUBLE factor)
{
	BOOL bRet = FALSE;
	if(pFormatContext)
	{
		SpeedFactor = factor;
		IncPlay = (int64_t)(IncFrame * SpeedFactor);
		bRet = TRUE;
	}
	return bRet;
}
// 函数: StreamWrite
// 功能: 流复制写入指定范围的数据
int64_t StreamWrite(AVFormatContext* fmtDest, int audio_dest, int video_dest, AVFormatContext* fmtOper, int audio_oper, int video_oper, AVPacket* pkt, int64_t qwOffset, int64_t qwLeft, int64_t qwRight)
{
	// 局部变量集中定义区域
	int64_t ret = 0;
	int64_t pts_audio = -1;
	int64_t pts_video = -1;
	int64_t dts_audio = -1;
	int64_t dts_video = -1;
	// 将左右边界转换到各流的时间基，便于简化运算
	int64_t qwLeftVideo = av_rescale_q(qwLeft, AV_TIME_BASE_Q, fmtOper->streams[video_oper]->time_base);
	int64_t qwLeftAudio = av_rescale_q(qwLeft, AV_TIME_BASE_Q, fmtOper->streams[audio_oper]->time_base);
	int64_t qwRightVideo = av_rescale_q(qwRight, AV_TIME_BASE_Q, fmtOper->streams[video_oper]->time_base);
	int64_t qwRightAudio = av_rescale_q(qwRight, AV_TIME_BASE_Q, fmtOper->streams[audio_oper]->time_base);
	// 读取视频之前需要先 seek 关键帧
	if (av_seek_frame(fmtOper, -1, qwLeft, AVSEEK_FLAG_BACKWARD) < 0)
	{
		goto END;
	};
	// 读取视频数据并写入输出文件中
	while (av_read_frame(fmtOper, pkt) >= 0)
	{
		CONST AVStream* StreamOper = NULL;
		CONST AVStream* StreamDest = NULL;
		int64_t pts = 0;
		int64_t dts = 0;
		int64_t pts_us = 0;
		int64_t left = 0;
		int64_t right = 0;
		int out_stream_idx = 0;
		// 获取视频流
		if (pkt->stream_index == video_oper)
		{
			StreamDest = fmtDest->streams[video_dest];
			StreamOper = fmtOper->streams[video_oper];
			out_stream_idx = video_dest;
			left = qwLeftVideo;
			right = qwRightVideo;
		}
		else if (pkt->stream_index == audio_oper)
		{
			StreamDest = fmtDest->streams[audio_dest];
			StreamOper = fmtOper->streams[audio_oper];
			out_stream_idx = audio_dest;
			left = qwLeftAudio;
			right = qwRightAudio;
		}
		else
		{
			av_packet_unref(pkt);
			continue;
		}
		// 如果读取的时间超过了最长时间表示不需要该帧，跳过
		if (pkt->pts > right)// || pkt->pts < left)
		{
			av_packet_unref(pkt);
			continue;
		}
		// 计算 pts 和 dts
		if (pkt->stream_index == video_oper)
		{
			if (dts_video == -1)
			{
				dts_video = pkt->dts - av_rescale_q(qwOffset, AV_TIME_BASE_Q, fmtOper->streams[video_oper]->time_base);
			}
			dts = pkt->dts - dts_video;
		}
		else if (pkt->stream_index == audio_oper)
		{
			if (dts_audio == -1)
			{
				dts_audio = pkt->dts - av_rescale_q(qwOffset, AV_TIME_BASE_Q, fmtOper->streams[audio_oper]->time_base);
			}
			dts = pkt->dts - dts_audio;

		}
		pts = dts + pkt->pts - pkt->dts;
		// 计算当前包的结束时间（微秒），并更新最大值
		int64_t end_time = pts + pkt->duration;  // 在流时间基上的结束时刻
		int64_t end_us = av_rescale_q(end_time, StreamDest->time_base, AV_TIME_BASE_Q);
		if (end_us > ret) {
			ret = end_us;
		}
		// 转换
		pkt->pts = pts;
		pkt->dts = dts;
		pkt->duration = pkt->duration;
		pkt->pos = -1;
		pkt->stream_index = out_stream_idx;
		av_interleaved_write_frame(fmtDest, pkt);
		// av_write_frame(fmtDest,pkt);
		av_packet_unref(pkt);
	}
END:
	return ret;
}
