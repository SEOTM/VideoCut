//=============================================================================
//  VideoEncoder.h
//  说明：管理输出文件的编码与复用，支持逐段添加输入片段（视频+音频）
//=============================================================================
#pragma once

#include <minwindef.h>
#include <d3d11.h>
#include <stdint.h>
#include "StreamPipe.hpp"
// FFmpeg 前向声明
struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVPacket;
struct AVFrame;
struct AVRational;
struct AVAudioFifo;
enum AVSampleFormat;
struct AVCodec;
struct SwsContext;
struct SwrContext;

// ================================
// 类名: VideoEncoder
// 说明: 视频重编码器，用于将指定视频重编码为指定格式后保存
// ================================
class VideoEncoder
{
private:
    struct
    {
        AVFormatContext* fmt_ctx = NULL;
        struct
        {
            AVStream* stream = NULL;            // 流
            AVCodecContext* codec_ctx = NULL;   // 编码器上下文
            CONST AVCodec* codec = NULL;        // 编码器
            int64_t duration = 0;               // 输出流时长（以流时间基为单位）
            int inc;                            // 输出流时长逐帧递增值
            int stream_idx = -1;                // 流索引
        } video, audio;
    } enc;
    struct
    {
        AVFormatContext* fmt_ctx = NULL;
        struct
        {
            AVStream* stream = NULL;
            AVCodecContext* codec_ctx = NULL;
            const AVCodec* codec = NULL;
            int64_t timeoffset = 0;
            int stream_idx = -1;
        } video, audio;
    } dec;

    // 其他成员变量
    AVFrame* pFrame = NULL;            // 帧
    AVPacket* pPacket = NULL;          // 包
    SIZE resolution{};                     // 视频分辨率
    int64_t duration = 0;                  // 整体持续时长（以 AV_TIME_BASE 为单位）
    int64_t last_dts_written = 0;          // 上次写入的 DTS（保证单调递增）

    // 可复用的转换上下文（每个输入文件独立创建，在 EncodingWrite 中管理）
    struct SwsContext* sws_ctx = NULL;
    struct SwrContext* swr_ctx = NULL;

    // 硬件上传复用帧
    AVFrame* m_reuse_frame = NULL;

    // 音频 FIFO 成员变量，用于累积所有片段的音频样本
    AVAudioFifo* m_audio_fifo = NULL;

    // 倍速因子（1/32 ~ 32）
    double m_speed = 1.0;

    // 辅助函数（成员函数）
    int WriteEncPkts(AVCodecContext* enc_ctx, AVStream* stream, int stream_index);
    int ProcAudioFIFO();
    int FlushAudioFIFO();
    int PushRawAudioToFIFO(AVFrame* frame);  // 使用成员 swr_ctx 和 m_audio_fifo
    AVFrame* ProcessFrame(AVFrame* in_frame);
    BOOL DirectSendVideoFrame(AVFrame* frame);
    BOOL ReleaseVideoFrame(AVFrame* frame);

    // 清理 dec 资源（EncodingWrite 开始时调用）
    void CleanupDec();

    // 私有构造函数，只能通过 Create 创建
    VideoEncoder();

public:
    // 函数: Create
    // 功能: 静态工厂方法，创建 VideoEncoder 对象，返回错误码（0 表示成功）
    // 参数 speed: 输出视频速率相对于输入文件的速率，范围 [1/32, 32]
    static VideoEncoder* Create(LPCSTR lpFilename, SIZE resolution, AVRational video_frame_rate, double speed, int& out_error_code);

    // 函数: ~VideoEncoder
    // 功能: 析构函数，自动清理未释放资源
    ~VideoEncoder();

    // 禁止拷贝
    VideoEncoder(const VideoEncoder&) = delete;
    VideoEncoder& operator=(const VideoEncoder&) = delete;

    // 函数: EncodingWrite
    // 功能: 对源进行编码写入到输出文件
    // 参数 lpOperfile : 输入文件路径
    //      left, right: 截取时间范围（相对于文件内时间轴，单位 AV_TIME_BASE）
    //      duration_real: 返回当前写入后的总时长
    BOOL EncodingWrite(LPCSTR lpOperfile, int64_t left, int64_t right, int64_t& duration_real);

    // 函数: Finish
    // 功能: 结束追加，并输出文件
    BOOL Finish();
};