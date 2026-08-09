// ================================================================
// 文件: VideoEncoder.cpp
// 说明: 实现 VideoEncoder 类，用于视频重编码和复用
//       使用 StreamPipe 流式管道处理视频倍速缩放
// ================================================================

#include "VideoEncoder.h"
#include "StreamPipe.hpp"
#include <cstdio>
#include <Windows.h>
#include <mfapi.h>
#include <chrono>
#include <queue>
#include <vector>
#include <cstring>
#include <assert.h>
#include <algorithm>   // for std::lower_bound
#include <memory>       // for std::unique_ptr

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/codec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
}

// ================================
// 编译指示区域
// ================================
constexpr int QSV_UPLOAD_MODE = 1;

// ================================
// 辅助函数声明区（仅保留静态函数）
// ================================

VOID SetErrorLog(int code, LPCSTR fmt, ...);
static int CalcVEncLv(SIZE resolution, AVRational framerate);
static void ShowPixFmts(const AVCodec* codec, AVCodecContext* codec_ctx = nullptr);
static void ShowEncoder(AVCodecID id = {});
static AVCodecContext* OpenAudioEncoder(AVFormatContext* ofmt_ctx);
static AVCodecContext* OpenVideoEncoderLibx264(AVFormatContext* ofmt_ctx, SIZE resolution, AVRational framerate);
static AVCodecContext* OpenVideoEncoderQSV(AVFormatContext* ofmt_ctx, SIZE resolution, AVRational framerate);

// ================================
// 名称: VideoEncoder 
// 功能: 视频编码器类实现
// ================================

// 函数: VideoEncoder
// 功能: 私有构造函数，仅初始化成员为默认值
VideoEncoder::VideoEncoder()
    : sws_ctx(nullptr), swr_ctx(nullptr), m_audio_fifo(nullptr), m_speed(1.0)
{
    enc = {};
    dec = {};
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        SetErrorLog(0, "MFStartup failed");
    }
}

// 函数: Create
// 功能: 静态工厂函数，使用返回码代替异常
VideoEncoder* VideoEncoder::Create(LPCSTR lpFilename, SIZE resolution, AVRational video_frame_rate, double speed, int& out_error_code)
{
    // 倍速范围校验
    const double MIN_SPEED = 1.0 / 32.0;
    const double MAX_SPEED = 32.0;
    if (speed < MIN_SPEED || speed > MAX_SPEED)
    {
        out_error_code = AVERROR(EINVAL);
        SetErrorLog(out_error_code, "Speed parameter out of range [1/32, 32]: %f", speed);
        return nullptr;
    }

    VideoEncoder* self = new VideoEncoder();
    if (!self) {
        out_error_code = AVERROR(ENOMEM);
        SetErrorLog(out_error_code, "new VideoEncoder failed");
        return nullptr;
    }

    self->m_speed = speed;   // 保存倍速

    AVFormatContext* ofmt_ctx = nullptr;
    AVStream* vstream = nullptr;
    AVStream* astream = nullptr;
    AVCodecContext* venc_ctx = nullptr;
    AVCodecContext* aenc_ctx = nullptr;

    int ret = 0;

    // 1. 分配输出上下文
    ret = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, lpFilename);
    if (ret < 0)
    {
        out_error_code = ret;
        SetErrorLog(ret, "avformat_alloc_output_context2 failed");
        delete self;
        return nullptr;
    }

    // 2. 创建视频流
    vstream = avformat_new_stream(ofmt_ctx, nullptr);
    if (!vstream)
    {
        out_error_code = AVERROR(ENOMEM);
        SetErrorLog(out_error_code, "avformat_new_stream failed for video");
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }
    vstream->id = ofmt_ctx->nb_streams - 1;

    // 2.2 打开视频编码器（优先尝试QSV，失败则回退到libx264）
    venc_ctx = OpenVideoEncoderQSV(ofmt_ctx, resolution, video_frame_rate);
    if (!venc_ctx) 
    {
        venc_ctx = OpenVideoEncoderLibx264(ofmt_ctx, resolution, video_frame_rate);
        if (!venc_ctx) 
        {
            out_error_code = AVERROR_ENCODER_NOT_FOUND;
            SetErrorLog(out_error_code, "No video encoder available (tried h264_qsv and libx264)");
            avformat_free_context(ofmt_ctx);
            delete self;
            return NULL;
        }
    }

    ShowPixFmts(venc_ctx->codec, venc_ctx);

    // 2.4 将编码器参数复制到流
    ret = avcodec_parameters_from_context(vstream->codecpar, venc_ctx);
    if (ret < 0)
    {
        out_error_code = ret;
        SetErrorLog(ret, "avcodec_parameters_from_context failed for video");
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }
    vstream->time_base = venc_ctx->time_base;

    // 3. 创建音频流
    astream = avformat_new_stream(ofmt_ctx, nullptr);
    if (!astream)
    {
        out_error_code = AVERROR(ENOMEM);
        SetErrorLog(out_error_code, "avformat_new_stream failed for audio");
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }
    astream->id = ofmt_ctx->nb_streams - 1;

    // 3.2 打开音频编码器
    aenc_ctx = OpenAudioEncoder(ofmt_ctx);
    if (!aenc_ctx)
    {
        out_error_code = AVERROR_ENCODER_NOT_FOUND;
        SetErrorLog(out_error_code, "Failed to open audio encoder");
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }

    // 3.4 参数复制到音频流
    ret = avcodec_parameters_from_context(astream->codecpar, aenc_ctx);
    if (ret < 0)
    {
        out_error_code = ret;
        SetErrorLog(ret, "avcodec_parameters_from_context failed for audio");
        avcodec_free_context(&aenc_ctx);
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }
    astream->time_base = aenc_ctx->time_base;

    // 4. 打开输出文件并写入头
    ret = avio_open(&ofmt_ctx->pb, lpFilename, AVIO_FLAG_WRITE);
    if (ret < 0)
    {
        out_error_code = ret;
        SetErrorLog(ret, "avio_open failed");
        avcodec_free_context(&aenc_ctx);
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }

    ret = avformat_write_header(ofmt_ctx, nullptr);
    if (ret < 0)
    {
        out_error_code = ret;
        SetErrorLog(ret, "avformat_write_header failed");
        avio_closep(&ofmt_ctx->pb);
        avcodec_free_context(&aenc_ctx);
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }

    // 5. 分配 packet 和 frame（确保成员始终有效）
    self->pPacket = av_packet_alloc();
    if (!self->pPacket) {
        out_error_code = AVERROR(ENOMEM);
        SetErrorLog(out_error_code, "av_packet_alloc failed");
        avcodec_free_context(&aenc_ctx);
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }
    self->pFrame = av_frame_alloc();
    if (!self->pFrame) {
        out_error_code = AVERROR(ENOMEM);
        SetErrorLog(out_error_code, "av_frame_alloc failed");
        av_packet_free(&self->pPacket);
        avcodec_free_context(&aenc_ctx);
        avcodec_free_context(&venc_ctx);
        avformat_free_context(ofmt_ctx);
        delete self;
        return nullptr;
    }

    // 6. 保存成员变量到 enc
    self->enc.fmt_ctx = ofmt_ctx;
    self->enc.video.stream = vstream;
    self->enc.audio.stream = astream;
    self->enc.video.codec_ctx = venc_ctx;
    self->enc.audio.codec_ctx = aenc_ctx;
    self->enc.video.stream_idx = vstream->index;
    self->enc.audio.stream_idx = astream->index;
    self->enc.video.codec = venc_ctx->codec;
    self->enc.audio.codec = aenc_ctx->codec;

    // 初始化流 PTS 维护字段
    self->enc.video.duration = 0;
    self->enc.audio.duration = 0;
    self->enc.video.inc = av_rescale_q(1, av_inv_q(self->enc.video.codec_ctx->framerate), self->enc.video.stream->time_base);
    self->enc.audio.inc = self->enc.audio.codec_ctx->frame_size <= 0 ? 1024 : self->enc.audio.codec_ctx->frame_size;

    self->resolution = resolution;
    self->duration = 0;
    self->last_dts_written = AV_NOPTS_VALUE;

    out_error_code = 0;
    return self;
}

// 函数: ~VideoEncoder
// 功能: 析构函数，调用Finish()完成编码并释放资源
VideoEncoder::~VideoEncoder()
{
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }
    if (m_audio_fifo) {
        av_audio_fifo_free(m_audio_fifo);
        m_audio_fifo = nullptr;
    }
    CleanupDec();
    Finish();
    MFShutdown();
}

// 函数: CleanupDec
// 功能: 释放 dec 中分配的资源，并将指针置空
void VideoEncoder::CleanupDec()
{
    if (dec.video.codec_ctx) {
        avcodec_free_context(&dec.video.codec_ctx);
    }
    if (dec.audio.codec_ctx) {
        avcodec_free_context(&dec.audio.codec_ctx);
    }
    if (dec.fmt_ctx) {
        avformat_close_input(&dec.fmt_ctx);
    }
    dec = {};
}

// 函数: EncodingWrite
// 功能: 将输入文件中的指定时间片段重编码并复用写入输出文件
BOOL VideoEncoder::EncodingWrite(LPCSTR lpOperfile, int64_t left, int64_t right, int64_t& duration_real)
{
    // 前置空指针检查，确保编码器已正确初始化
    if (!enc.fmt_ctx || !enc.video.codec_ctx || !enc.audio.codec_ctx || !pPacket || !pFrame)
    {
        SetErrorLog(0, "Encoder not properly initialized");
        return FALSE;
    }

    int ret = 0;
    BOOL result = FALSE;

    // 诊断计数
    int64_t sel_video_frames = 0, sel_audio_frames = 0;
    int64_t skip_video_frames = 0, skip_audio_frames = 0;
    int64_t sel_video_min_pts = AV_NOPTS_VALUE, sel_video_max_pts = AV_NOPTS_VALUE;
    int64_t sel_audio_min_us = AV_NOPTS_VALUE, sel_audio_max_us = AV_NOPTS_VALUE;
    int64_t video_left_ts = AV_NOPTS_VALUE, video_right_ts = AV_NOPTS_VALUE;
    StreamPipe<VideoEncoder, AVFrame*>* pFramePipe = NULL;

    int64_t duration_start = duration_real;
    CleanupDec();

    // 释放之前可能残留的转换上下文
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }
    if (m_audio_fifo) {
        av_audio_fifo_free(m_audio_fifo);
        m_audio_fifo = nullptr;
    }

    // 1. 打开输入文件
    ret = avformat_open_input(&dec.fmt_ctx, lpOperfile, nullptr, nullptr);
    if (ret < 0)
    {
        SetErrorLog(ret, "Failed to open input file");
        goto cleanup;
    }
    ret = avformat_find_stream_info(dec.fmt_ctx, nullptr);
    if (ret < 0)
    {
        SetErrorLog(ret, "Failed to find stream info");
        goto cleanup;
    }

    // 2. 查找视频流和音频流
    dec.video.stream_idx = av_find_best_stream(dec.fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec.video.codec, 0);
    dec.audio.stream_idx = av_find_best_stream(dec.fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec.audio.codec, 0);
    if (dec.video.stream_idx >= 0)
    {
        dec.video.stream = dec.fmt_ctx->streams[dec.video.stream_idx];
        dec.video.timeoffset = dec.video.stream->start_time;
    }
    if (dec.audio.stream_idx >= 0)
    {
        dec.audio.stream = dec.fmt_ctx->streams[dec.audio.stream_idx];
        dec.audio.timeoffset = dec.audio.stream->start_time;
    }
    if (!(dec.video.stream && dec.audio.stream && dec.video.codec && dec.audio.codec))
    {
        goto cleanup;
    }

    // 获取视频和音频的起始时间戳（用于 timeoffset 修正）
    {
        int64_t video_min_ts = AV_NOPTS_VALUE;
        int64_t audio_pts = AV_NOPTS_VALUE;
        AVPacket* pkt_ptr = av_packet_alloc();
        AVPacket& pkt = *pkt_ptr;

        int video_idx = dec.video.stream_idx;
        int audio_idx = dec.audio.stream_idx;
        AVRational tbv = dec.video.stream->time_base;
        AVRational tba = dec.audio.stream->time_base;

        while (av_read_frame(dec.fmt_ctx, &pkt) >= 0)
        {
            if (pkt.stream_index == video_idx && video_min_ts == AV_NOPTS_VALUE) {
                int64_t pts = pkt.pts != AV_NOPTS_VALUE ? pkt.pts : AV_NOPTS_VALUE;
                int64_t dts = pkt.dts != AV_NOPTS_VALUE ? pkt.dts : AV_NOPTS_VALUE;
                if (pts != AV_NOPTS_VALUE && dts != AV_NOPTS_VALUE)
                    video_min_ts = FFMIN(pts, dts);
                else if (pts != AV_NOPTS_VALUE)
                    video_min_ts = pts;
                else if (dts != AV_NOPTS_VALUE)
                    video_min_ts = dts;
            }
            if (pkt.stream_index == audio_idx && audio_pts == AV_NOPTS_VALUE)
            {
                audio_pts = pkt.pts != AV_NOPTS_VALUE ? pkt.pts : AV_NOPTS_VALUE;
            }
            av_packet_unref(&pkt);
            if ((video_idx < 0 || video_min_ts != AV_NOPTS_VALUE) && (audio_idx < 0 || audio_pts != AV_NOPTS_VALUE))
                break;
        }
        av_seek_frame(dec.fmt_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
        av_packet_free(&pkt_ptr);

        dec.video.timeoffset = video_min_ts;
        dec.audio.timeoffset = audio_pts;
    }

    // 转换 left/right 到视频流时间基
    {
        video_left_ts = av_rescale_q(left, AV_TIME_BASE_Q, dec.video.stream->time_base);
        video_right_ts = av_rescale_q(right, AV_TIME_BASE_Q, dec.video.stream->time_base);
        printf("RESCALED CUT : [%lld, %lld]\n", (long long)video_left_ts, (long long)video_right_ts);
    }

    // 3. 初始化解码器（视频）
    {
        dec.video.codec_ctx = avcodec_alloc_context3(dec.video.codec);
        if (!dec.video.codec_ctx) {
            SetErrorLog(0, "Failed to allocate video decoder context");
            goto cleanup;
        }
        ret = avcodec_parameters_to_context(dec.video.codec_ctx, dec.video.stream->codecpar);
        if (ret < 0) {
            SetErrorLog(ret, "Failed to copy video codec parameters");
            goto cleanup;
        }
        dec.video.codec_ctx->time_base = dec.video.stream->time_base;
        ret = avcodec_open2(dec.video.codec_ctx, dec.video.codec, nullptr);
        if (ret < 0) {
            SetErrorLog(ret, "Failed to open video decoder");
            goto cleanup;
        }
    }

    // 4. 初始化解码器（音频）及重采样相关
    {
        dec.audio.codec_ctx = avcodec_alloc_context3(dec.audio.codec);
        if (!dec.audio.codec_ctx) {
            SetErrorLog(0, "Failed to allocate audio decoder context");
            goto cleanup;
        }
        ret = avcodec_parameters_to_context(dec.audio.codec_ctx, dec.audio.stream->codecpar);
        if (ret < 0) {
            SetErrorLog(ret, "Failed to copy audio codec parameters");
            goto cleanup;
        }
        dec.audio.codec_ctx->time_base = dec.audio.stream->time_base;
        ret = avcodec_open2(dec.audio.codec_ctx, dec.audio.codec, nullptr);
        if (ret < 0) {
            SetErrorLog(ret, "Failed to open audio decoder");
            goto cleanup;
        }

        // 创建 SwrContext
        swr_ctx = swr_alloc();
        if (!swr_ctx) {
            SetErrorLog(0, "Failed to allocate SwrContext");
            goto cleanup;
        }
        av_opt_set_chlayout(swr_ctx, "in_chlayout", &dec.audio.codec_ctx->ch_layout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate", (dec.audio.codec_ctx->sample_rate * m_speed), 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", dec.audio.codec_ctx->sample_fmt, 0);
        av_opt_set_chlayout(swr_ctx, "out_chlayout", &enc.audio.codec_ctx->ch_layout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate", enc.audio.codec_ctx->sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", enc.audio.codec_ctx->sample_fmt, 0);
        ret = swr_init(swr_ctx);
        if (ret < 0) {
            SetErrorLog(ret, "Failed to initialize SwrContext");
            swr_free(&swr_ctx);
            goto cleanup;
        }

        // 创建音频 FIFO，存储原始音频样本（解码后格式）
        if (!m_audio_fifo) {
            // 使用解码器的参数分配 FIFO
            int fifo_size = dec.audio.codec_ctx->frame_size > 0 ? dec.audio.codec_ctx->frame_size * 8 : 4096;
            m_audio_fifo = av_audio_fifo_alloc(dec.audio.codec_ctx->sample_fmt,
                dec.audio.codec_ctx->ch_layout.nb_channels,
                fifo_size);
            if (!m_audio_fifo) {
                SetErrorLog(0, "Failed to allocate audio FIFO");
                goto cleanup;
            }
        }
        else {
            av_audio_fifo_reset(m_audio_fifo);
        }
    }

    // 5. 创建 SwsContext（视频像素转换）
    {
        enum AVPixelFormat src_pix_fmt = dec.video.codec_ctx->pix_fmt;
        enum AVPixelFormat dst_sw_fmt;

        if (enc.video.codec_ctx->hw_frames_ctx) {
            AVHWFramesContext* hw_frames_ctx = (AVHWFramesContext*)enc.video.codec_ctx->hw_frames_ctx->data;
            dst_sw_fmt = hw_frames_ctx->sw_format;
        }
        else {
            dst_sw_fmt = enc.video.codec_ctx->pix_fmt;
        }

        sws_ctx = sws_getContext(
            dec.video.codec_ctx->width, dec.video.codec_ctx->height, src_pix_fmt,
            dec.video.codec_ctx->width, dec.video.codec_ctx->height, dst_sw_fmt,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_ctx) {
            SetErrorLog(0, "Failed to create SwsContext");
            goto cleanup;
        }
    }

    // 统一使用管道处理视频帧，无论 speed 是否为 1
    {
        // 计算输入帧间隔（微秒）
        AVRational input_framerate = dec.video.stream->avg_frame_rate;
        if (input_framerate.num == 0 || input_framerate.den == 0)
            input_framerate = dec.video.stream->r_frame_rate;
        if (input_framerate.num == 0 || input_framerate.den == 0)
            input_framerate = dec.video.codec_ctx->framerate;
        int64_t input_inc_us = av_rescale_q(1, av_inv_q(input_framerate), AV_TIME_BASE_Q);
        // 输出帧间隔（微秒）
        int64_t output_inc_us = av_rescale_q(1, av_inv_q(enc.video.codec_ctx->framerate), AV_TIME_BASE_Q);
        // 创建管道
        pFramePipe = new StreamPipe<VideoEncoder, AVFrame*>
            (
                static_cast<int64_t>(input_inc_us / m_speed), output_inc_us, this,
                &VideoEncoder::ProcessFrame,
                &VideoEncoder::DirectSendVideoFrame,
                &VideoEncoder::ReleaseVideoFrame
            );
    }

    // 6. 读取包循环
    while (av_read_frame(dec.fmt_ctx, pPacket) >= 0)
    {
        int stream_idx = pPacket->stream_index;
        int out_stream_idx = -1;
        AVStream* in_stream = dec.fmt_ctx->streams[stream_idx];
        AVCodecContext* dec_ctx = nullptr;
        AVCodecContext* enc_ctx = nullptr;
        AVStream* out_stream = nullptr;

        if (stream_idx == dec.video.stream_idx)
        {
            dec_ctx = dec.video.codec_ctx;
            out_stream_idx = enc.video.stream_idx;
            enc_ctx = enc.video.codec_ctx;
            out_stream = enc.video.stream;
        }
        else if (stream_idx == dec.audio.stream_idx)
        {
            dec_ctx = dec.audio.codec_ctx;
            out_stream_idx = enc.audio.stream_idx;
            enc_ctx = enc.audio.codec_ctx;
            out_stream = enc.audio.stream;
        }
        else
        {
            av_packet_unref(pPacket);
            continue;
        }

        // 视频流：在流时间基上比较
        if (stream_idx == dec.video.stream_idx)
        {
            int64_t pkt_ts = pPacket->pts != AV_NOPTS_VALUE ? pPacket->pts : pPacket->dts;
            if (pkt_ts == AV_NOPTS_VALUE) {
                av_packet_unref(pPacket);
                continue;
            }
            int64_t content_ts = pkt_ts - dec.video.timeoffset;
            if (video_left_ts != AV_NOPTS_VALUE && video_right_ts != AV_NOPTS_VALUE) {
                if (content_ts < video_left_ts || content_ts >= video_right_ts)
                {
                    skip_video_frames++;
                    av_packet_unref(pPacket);
                    continue;
                }
            }
        }
        else // 音频流：微秒比较
        {
            int64_t pkt_ts = (pPacket->pts != AV_NOPTS_VALUE) ? pPacket->pts : pPacket->dts;
            if (pkt_ts == AV_NOPTS_VALUE) {
                av_packet_unref(pPacket);
                continue;
            }
            int64_t content_us = av_rescale_q(pkt_ts - dec.audio.timeoffset,
                in_stream->time_base,
                AV_TIME_BASE_Q);
            if (content_us < left || content_us >= right) {
                skip_audio_frames++;
                av_packet_unref(pPacket);
                continue;
            }
        }

        if (pPacket->pts == AV_NOPTS_VALUE)
            pPacket->pts = pPacket->dts;

        ret = avcodec_send_packet(dec_ctx, pPacket);
        if (ret < 0) {
            SetErrorLog(ret, "Error sending packet to decoder");
            avcodec_flush_buffers(dec_ctx);
            av_packet_unref(pPacket);
            continue;
        }

        while (ret >= 0)
        {
            ret = avcodec_receive_frame(dec_ctx, pFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                SetErrorLog(ret, "Error receiving frame from decoder");
                goto cleanup;
            }

            int64_t frame_pts_ts = AV_NOPTS_VALUE;
            if (pFrame->pts != AV_NOPTS_VALUE)
                frame_pts_ts = pFrame->pts;
            else if (pFrame->pkt_dts != AV_NOPTS_VALUE)
                frame_pts_ts = pFrame->pkt_dts;
            else {
                av_frame_unref(pFrame);
                continue;
            }

            if (stream_idx == dec.video.stream_idx)
            {
                int64_t frame_content_ts = frame_pts_ts - dec.video.timeoffset;
                if (video_left_ts != AV_NOPTS_VALUE && video_right_ts != AV_NOPTS_VALUE) {
                    if (frame_content_ts == AV_NOPTS_VALUE ||
                        frame_content_ts < video_left_ts ||
                        frame_content_ts >= video_right_ts) {
                        skip_video_frames++;
                        av_frame_unref(pFrame);
                        continue;
                    }
                }
                sel_video_frames++;
                if (sel_video_min_pts == AV_NOPTS_VALUE || frame_content_ts < sel_video_min_pts)
                    sel_video_min_pts = frame_content_ts;
                if (sel_video_max_pts == AV_NOPTS_VALUE || frame_content_ts > sel_video_max_pts)
                    sel_video_max_pts = frame_content_ts;

                // 统一通过管道处理视频帧
                if (pFramePipe)
                {
                    pFramePipe->PushSample(pFrame);
                }
                else
                {
                    SetErrorLog(0, "Video pipeline not created");
                    goto cleanup;
                }
            }
            else // audio
            {
                int64_t frame_content_us = av_rescale_q(frame_pts_ts - dec.audio.timeoffset,
                    dec_ctx->time_base,
                    AV_TIME_BASE_Q);
                if (frame_content_us < left || frame_content_us >= right) {
                    skip_audio_frames++;
                    av_frame_unref(pFrame);
                    continue;
                }
                sel_audio_frames++;
                if (sel_audio_min_us == AV_NOPTS_VALUE || frame_content_us < sel_audio_min_us)
                    sel_audio_min_us = frame_content_us;
                if (sel_audio_max_us == AV_NOPTS_VALUE || frame_content_us > sel_audio_max_us)
                    sel_audio_max_us = frame_content_us;

                // 将原始音频帧推入 FIFO
                ret = PushRawAudioToFIFO(pFrame);
                if (ret < 0)
                    goto cleanup;

                // 尝试从 FIFO 中取出原始数据并编码
                ret = ProcAudioFIFO();
                if (ret < 0)
                    goto cleanup;
            }

            if (pFrame->pts != AV_NOPTS_VALUE)
            {
                int64_t offset = (stream_idx == dec.video.stream_idx) ? dec.video.timeoffset : dec.audio.timeoffset;
                int64_t us = av_rescale_q(pFrame->pts - offset, in_stream->time_base, AV_TIME_BASE_Q);
                us = us - left + duration;
                duration_real = (us > duration_real) ? us : duration_real;
            }
            av_frame_unref(pFrame);
        }
        av_packet_unref(pPacket);
    }

    // ==================== 7. 刷新剩余数据 ====================
    // 7.1 刷新视频解码器
    if (dec.video.codec_ctx) {
        avcodec_send_packet(dec.video.codec_ctx, nullptr);
        while (1) {
            ret = avcodec_receive_frame(dec.video.codec_ctx, pFrame);
            if (ret == AVERROR_EOF) break;
            if (ret < 0) break;

            int64_t frame_pts_ts = pFrame->pts != AV_NOPTS_VALUE ? pFrame->pts : pFrame->pkt_dts;
            if (frame_pts_ts == AV_NOPTS_VALUE) {
                av_frame_unref(pFrame);
                continue;
            }
            int64_t frame_content_ts = frame_pts_ts - dec.video.timeoffset;
            if (frame_content_ts >= video_left_ts && frame_content_ts < video_right_ts) {
                if (pFramePipe)
                {
                    pFramePipe->PushSample(pFrame);
                }
                else
                {
                    SetErrorLog(0, "Video pipeline not created");
                    goto cleanup;
                }
            }
            av_frame_unref(pFrame);
        }
    }

    // 7.2 刷新视频管道（如果使用）
    if (pFramePipe)
    {
        int64_t output_end_us = (int64_t)llround((double)(right - left) / m_speed);
        pFramePipe->Flush(output_end_us);
    }

    // 7.3 刷新音频解码器
    if (dec.audio.codec_ctx) {
        avcodec_send_packet(dec.audio.codec_ctx, nullptr);
        while (1) {
            ret = avcodec_receive_frame(dec.audio.codec_ctx, pFrame);
            if (ret == AVERROR_EOF) break;
            if (ret < 0) break;

            int64_t frame_content_us = av_rescale_q(pFrame->pts - dec.audio.timeoffset,
                dec.audio.codec_ctx->time_base,
                AV_TIME_BASE_Q);
            if (frame_content_us >= left && frame_content_us < right) {
                ret = PushRawAudioToFIFO(pFrame);
                if (ret < 0)
                    goto cleanup;
            }
            av_frame_unref(pFrame);
        }
    }

    // 7.4 刷新音频 FIFO 及重采样器延迟
    if (m_audio_fifo) {
        ret = FlushAudioFIFO();
        if (ret < 0)
            goto cleanup;
    }

    // 8. 更新总时长
    duration_real = duration_start + (right - left);
    // 设置返回值
    result = TRUE;
cleanup:
    if (pFramePipe)
    {
        delete pFramePipe;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }

    CleanupDec();
    if (pPacket) av_packet_unref(pPacket);
    if (pFrame) av_frame_unref(pFrame);

    printf(
        "FRAME_SEL :\n"
        "  VIDEO: %lld [%lld, %lld]\n"
        "  AUDIO: %lld [%lld, %lld]\n",
        sel_video_frames, sel_video_min_pts, sel_video_max_pts,
        sel_audio_frames, sel_audio_min_us, sel_audio_max_us
    );
    printf(
        "FRAME_SKIP:\n"
        "  VIDEO: %lld\n"
        "  AUDIO: %lld\n",
        skip_video_frames, skip_audio_frames
    );

    return result;
}

// 函数: Finish
// 功能: 完成编码：写入文件尾，关闭输出流，释放所有编码器上下文和帧/包对象
BOOL VideoEncoder::Finish()
{
    // 刷新视频编码器
    if (enc.video.codec_ctx)
    {
        avcodec_send_frame(enc.video.codec_ctx, nullptr);
        WriteEncPkts(enc.video.codec_ctx, enc.video.stream, enc.video.stream_idx);
    }

    // 刷新音频编码器
    if (enc.audio.codec_ctx)
    {
        avcodec_send_frame(enc.audio.codec_ctx, nullptr);
        WriteEncPkts(enc.audio.codec_ctx, enc.audio.stream, enc.audio.stream_idx);
    }

    if (enc.fmt_ctx)
    {
        av_write_trailer(enc.fmt_ctx);
    }
    if (enc.fmt_ctx && enc.fmt_ctx->pb)
    {
        avio_closep(&enc.fmt_ctx->pb);
    }

    // 释放编码器上下文
    if (enc.video.codec_ctx)
    {
        avcodec_free_context(&enc.video.codec_ctx);
        enc.video.codec_ctx = nullptr;
    }
    if (enc.audio.codec_ctx)
    {
        avcodec_free_context(&enc.audio.codec_ctx);
        enc.audio.codec_ctx = nullptr;
    }

    if (pFrame)
    {
        av_frame_free(&pFrame);
        pFrame = nullptr;
    }
    if (pPacket)
    {
        av_packet_free(&pPacket);
        pPacket = nullptr;
    }
    if (enc.fmt_ctx)
    {
        avformat_free_context(enc.fmt_ctx);
        enc.fmt_ctx = nullptr;
    }
    return TRUE;
}

// ================================
// 成员函数实现（辅助函数）
// ================================

int VideoEncoder::WriteEncPkts(AVCodecContext* enc_ctx, AVStream* stream, int stream_index)
{
    int ret = 0;
    while (1) {
        av_packet_unref(this->pPacket);
        ret = avcodec_receive_packet(enc_ctx, this->pPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        this->pPacket->stream_index = stream_index;
        // 保留原始时间戳用于调试
        if (this->pPacket->duration == 0 && enc_ctx && enc_ctx->framerate.num != 0)
        {
            this->pPacket->duration = av_rescale_q(1, av_inv_q(enc_ctx->framerate), stream->time_base);
        }
        av_packet_rescale_ts(this->pPacket, enc_ctx->time_base, stream->time_base);

        ret = av_interleaved_write_frame(this->enc.fmt_ctx, this->pPacket);
        if (ret < 0)
            return ret;
    }
}

// 从原始 FIFO 读取数据，实时重采样并编码（拉模式核心）
int VideoEncoder::ProcAudioFIFO()
{
    int ret = 0;
    int frame_size = enc.audio.codec_ctx->frame_size;
    if (frame_size == 0) frame_size = 1024;

    while (1) {
        // 估算当前 FIFO 中的原始样本能产生多少输出样本
        int64_t out_samples_est = swr_get_out_samples(swr_ctx, av_audio_fifo_size(m_audio_fifo));
        if (out_samples_est < frame_size)
            break;  // 不足一帧，等待更多数据

        AVFrame* enc_frame = av_frame_alloc();
        if (!enc_frame) return AVERROR(ENOMEM);
        enc_frame->nb_samples = frame_size;
        enc_frame->ch_layout = enc.audio.codec_ctx->ch_layout;
        enc_frame->format = enc.audio.codec_ctx->sample_fmt;
        enc_frame->sample_rate = enc.audio.codec_ctx->sample_rate;
        ret = av_frame_get_buffer(enc_frame, 0);
        if (ret < 0) { av_frame_free(&enc_frame); return ret; }

        int total_converted = 0;
        uint8_t* output_planes[AV_NUM_DATA_POINTERS] = { 0 };
        int bytes_per_sample = av_get_bytes_per_sample(enc.audio.codec_ctx->sample_fmt);
        for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
            output_planes[ch] = enc_frame->extended_data[ch];
        }

        while (total_converted < frame_size) {
            // 从 FIFO 中读取原始样本（一次性最多读取一帧解码大小，避免过大）
            int fifo_size = av_audio_fifo_size(m_audio_fifo);
            int to_read = FFMIN(fifo_size, 4096); // 读取上限，避免单次过大
            if (to_read == 0) break;

            uint8_t** input_samples = nullptr;
            int linesize;
            ret = av_samples_alloc_array_and_samples(&input_samples, &linesize,
                dec.audio.codec_ctx->ch_layout.nb_channels,
                to_read,
                dec.audio.codec_ctx->sample_fmt, 0);
            if (ret < 0) { av_frame_free(&enc_frame); return ret; }

            int read = av_audio_fifo_read(m_audio_fifo, (void**)input_samples, to_read);
            if (read <= 0) {
                av_freep(&input_samples[0]);
                av_freep(&input_samples);
                break;
            }

            int converted = swr_convert(swr_ctx,
                output_planes, frame_size - total_converted,
                (const uint8_t**)input_samples, read);
            av_freep(&input_samples[0]);
            av_freep(&input_samples);

            if (converted < 0) { av_frame_free(&enc_frame); return converted; }

            total_converted += converted;
            // 更新输出平面指针
            for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
                output_planes[ch] += converted * bytes_per_sample;
            }

            if (converted == 0) break; // 无进展，避免死循环
        }

        if (total_converted == frame_size) {
            enc_frame->pts = enc.audio.duration;
            enc.audio.duration += enc.audio.inc;

            ret = avcodec_send_frame(enc.audio.codec_ctx, enc_frame);
            av_frame_free(&enc_frame);
            if (ret < 0) return ret;
            ret = WriteEncPkts(enc.audio.codec_ctx, enc.audio.stream, enc.audio.stream_idx);
            if (ret < 0) return ret;
        }
        else {
            // 如果最终不足一帧，不应该发生，但保险释放
            av_frame_free(&enc_frame);
            break;
        }
    }
    return 0;
}

// 刷新音频 FIFO 及重采样器延迟
int VideoEncoder::FlushAudioFIFO()
{
    int ret = 0;
    int frame_size = enc.audio.codec_ctx->frame_size;
    if (frame_size == 0) frame_size = 1024;

    // 1. 处理 FIFO 中剩余的原始样本
    while (av_audio_fifo_size(m_audio_fifo) > 0) {
        // 尝试生成一个编码帧（可能不足，需要用静音填充）
        AVFrame* enc_frame = av_frame_alloc();
        if (!enc_frame) return AVERROR(ENOMEM);
        enc_frame->nb_samples = frame_size;
        enc_frame->ch_layout = enc.audio.codec_ctx->ch_layout;
        enc_frame->format = enc.audio.codec_ctx->sample_fmt;
        enc_frame->sample_rate = enc.audio.codec_ctx->sample_rate;
        ret = av_frame_get_buffer(enc_frame, 0);
        if (ret < 0) { av_frame_free(&enc_frame); return ret; }

        // 先将整个输出帧填零（静音）
        int bytes_per_sample = av_get_bytes_per_sample(enc.audio.codec_ctx->sample_fmt);
        for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
            memset(enc_frame->extended_data[ch], 0, enc_frame->nb_samples * bytes_per_sample);
        }

        int total_converted = 0;
        uint8_t* output_planes[AV_NUM_DATA_POINTERS] = { 0 };
        for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
            output_planes[ch] = enc_frame->extended_data[ch];
        }

        while (total_converted < frame_size && av_audio_fifo_size(m_audio_fifo) > 0) {
            int fifo_size = av_audio_fifo_size(m_audio_fifo);
            int to_read = FFMIN(fifo_size, 4096);
            uint8_t** input_samples = nullptr;
            int linesize;
            ret = av_samples_alloc_array_and_samples(&input_samples, &linesize,
                dec.audio.codec_ctx->ch_layout.nb_channels,
                to_read,
                dec.audio.codec_ctx->sample_fmt, 0);
            if (ret < 0) { av_frame_free(&enc_frame); return ret; }

            int read = av_audio_fifo_read(m_audio_fifo, (void**)input_samples, to_read);
            if (read <= 0) {
                av_freep(&input_samples[0]);
                av_freep(&input_samples);
                break;
            }

            int converted = swr_convert(swr_ctx,
                output_planes, frame_size - total_converted,
                (const uint8_t**)input_samples, read);
            av_freep(&input_samples[0]);
            av_freep(&input_samples);

            if (converted < 0) { av_frame_free(&enc_frame); return converted; }

            total_converted += converted;
            for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
                output_planes[ch] += converted * bytes_per_sample;
            }
        }

        // 即使 total_converted < frame_size，也发送此帧（剩余部分为静音）
        enc_frame->pts = enc.audio.duration;
        enc.audio.duration += enc.audio.inc;  // 注意：即使实际转换不足，PTS 也要按整帧前进

        ret = avcodec_send_frame(enc.audio.codec_ctx, enc_frame);
        av_frame_free(&enc_frame);
        if (ret < 0) return ret;
        ret = WriteEncPkts(enc.audio.codec_ctx, enc.audio.stream, enc.audio.stream_idx);
        if (ret < 0) return ret;
    }

    // 2. 处理重采样器内部延迟
    while (1) {
        int delay_samples = swr_get_delay(swr_ctx, enc.audio.codec_ctx->sample_rate);
        if (delay_samples <= 0) break;

        int out_samples = swr_get_out_samples(swr_ctx, 0);
        if (out_samples <= 0) break;

        // 分配输出帧（大小取整帧，但可能不足，用静音填充）
        AVFrame* enc_frame = av_frame_alloc();
        if (!enc_frame) return AVERROR(ENOMEM);
        enc_frame->nb_samples = frame_size;
        enc_frame->ch_layout = enc.audio.codec_ctx->ch_layout;
        enc_frame->format = enc.audio.codec_ctx->sample_fmt;
        enc_frame->sample_rate = enc.audio.codec_ctx->sample_rate;
        ret = av_frame_get_buffer(enc_frame, 0);
        if (ret < 0) { av_frame_free(&enc_frame); return ret; }

        // 静音填充
        int bytes_per_sample = av_get_bytes_per_sample(enc.audio.codec_ctx->sample_fmt);
        for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
            memset(enc_frame->extended_data[ch], 0, enc_frame->nb_samples * bytes_per_sample);
        }

        uint8_t* output_planes[AV_NUM_DATA_POINTERS] = { 0 };
        for (int ch = 0; ch < enc.audio.codec_ctx->ch_layout.nb_channels; ch++) {
            output_planes[ch] = enc_frame->extended_data[ch];
        }

        int converted = swr_convert(swr_ctx, output_planes, frame_size, nullptr, 0);
        if (converted < 0) { av_frame_free(&enc_frame); return converted; }

        enc_frame->pts = enc.audio.duration;
        enc.audio.duration += enc.audio.inc;  // PTS 按整帧步进

        ret = avcodec_send_frame(enc.audio.codec_ctx, enc_frame);
        av_frame_free(&enc_frame);
        if (ret < 0) return ret;
        ret = WriteEncPkts(enc.audio.codec_ctx, enc.audio.stream, enc.audio.stream_idx);
        if (ret < 0) return ret;

        if (converted < frame_size) break; // 最后不足一帧时，已用静音补齐，停止
    }

    return 0;
}

// 将原始音频帧写入 FIFO（不进行重采样）
int VideoEncoder::PushRawAudioToFIFO(AVFrame* frame)
{
    if (!frame || !frame->data[0]) return AVERROR(EINVAL);

    // 确保 FIFO 有足够空间
    int space = av_audio_fifo_space(m_audio_fifo);
    if (space < frame->nb_samples) {
        av_audio_fifo_realloc(m_audio_fifo, av_audio_fifo_size(m_audio_fifo) + frame->nb_samples);
    }

    // 直接写入原始样本
    int ret = av_audio_fifo_write(m_audio_fifo, (void**)frame->extended_data, frame->nb_samples);
    return ret;
}

AVFrame* VideoEncoder::ProcessFrame(AVFrame* in_frame)
{
    bool need_conv = false;
    if constexpr (QSV_UPLOAD_MODE == 1) {
        need_conv = (enc.video.codec_ctx->hw_device_ctx != nullptr);
    }
    else {
        need_conv = (enc.video.codec_ctx->pix_fmt == AV_PIX_FMT_NV12 && in_frame->format != AV_PIX_FMT_NV12);
    }

    if (!need_conv) {
        return av_frame_clone(in_frame);
    }

    if (!sws_ctx) {
        SetErrorLog(0, "sws_ctx not created");
        return nullptr;
    }

    enum AVPixelFormat sw_format;
    if constexpr (QSV_UPLOAD_MODE == 1) {
        if (!enc.video.codec_ctx->hw_frames_ctx) {
            SetErrorLog(0, "hw_frames_ctx not set");
            return nullptr;
        }
        AVHWFramesContext* hw_frames_ctx = (AVHWFramesContext*)enc.video.codec_ctx->hw_frames_ctx->data;
        sw_format = hw_frames_ctx->sw_format;
    }
    else {
        sw_format = AV_PIX_FMT_NV12;
    }

    AVFrame* sw_frame = av_frame_alloc();
    if (!sw_frame) return nullptr;
    sw_frame->width = in_frame->width;
    sw_frame->height = in_frame->height;
    sw_frame->format = sw_format;
    if (av_frame_get_buffer(sw_frame, 0) < 0) {
        av_frame_free(&sw_frame);
        return nullptr;
    }

    int ret = sws_scale(sws_ctx,
        in_frame->data, in_frame->linesize,
        0, in_frame->height,
        sw_frame->data, sw_frame->linesize);
    if (ret < 0) {
        av_frame_free(&sw_frame);
        return nullptr;
    }
    av_frame_copy_props(sw_frame, in_frame);

    if constexpr (QSV_UPLOAD_MODE == 1) {
        AVFrame* hw_frame = av_frame_alloc();
        if (!hw_frame) {
            av_frame_free(&sw_frame);
            return nullptr;
        }
        ret = av_hwframe_get_buffer(enc.video.codec_ctx->hw_frames_ctx, hw_frame, 0);
        if (ret < 0) {
            av_frame_free(&sw_frame);
            av_frame_free(&hw_frame);
            return nullptr;
        }
        ret = av_hwframe_transfer_data(hw_frame, sw_frame, 0);
        if (ret < 0) {
            av_frame_free(&sw_frame);
            av_frame_free(&hw_frame);
            return nullptr;
        }
        av_frame_copy_props(hw_frame, sw_frame);
        av_frame_free(&sw_frame);
        return hw_frame;
    }
    else {
        return sw_frame;
    }
}

BOOL VideoEncoder::DirectSendVideoFrame(AVFrame* frame)
{
    frame->pts = enc.video.duration;
    frame->pkt_dts = AV_NOPTS_VALUE;

    int ret = avcodec_send_frame(enc.video.codec_ctx, frame);
    if (ret < 0) {
        SetErrorLog(ret, "avcodec_send_frame failed");
        return FALSE;
    }
    ret = WriteEncPkts(enc.video.codec_ctx, enc.video.stream, enc.video.stream_idx);
    if (ret < 0) {
        SetErrorLog(ret, "WriteEncPkts failed");
        return FALSE;
    }
    enc.video.duration += enc.video.inc;
    return TRUE;
}

BOOL VideoEncoder::ReleaseVideoFrame(AVFrame* frame)
{
    av_frame_free(&frame);
    return TRUE;
}

// ================================
// 辅助函数实现区（保持不变）
// ================================

static AVCodecContext* OpenAudioEncoder(AVFormatContext* ofmt_ctx)
{
    AVCodecContext* aenc_ctx = avcodec_alloc_context3(nullptr);
    if (!aenc_ctx) {
        SetErrorLog(AVERROR(ENOMEM), "avcodec_alloc_context3 failed for audio");
        return nullptr;
    }
    aenc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    aenc_ctx->codec_id = AV_CODEC_ID_AAC;
    aenc_ctx->bit_rate = 128000;
    aenc_ctx->sample_rate = 48000;
    aenc_ctx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    aenc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    aenc_ctx->time_base = AVRational{ 1, aenc_ctx->sample_rate };
    aenc_ctx->frame_size = 0;
    aenc_ctx->cutoff = 0;
    aenc_ctx->compression_level = -1;
    aenc_ctx->profile = AV_PROFILE_AAC_LOW;
    if (ofmt_ctx->oformat && (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)) {
        aenc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    const AVCodec* audio_codec = avcodec_find_encoder(aenc_ctx->codec_id);
    if (!audio_codec) {
        SetErrorLog(AVERROR_ENCODER_NOT_FOUND, "avcodec_find_encoder failed for AAC");
        avcodec_free_context(&aenc_ctx);
        return nullptr;
    }
    int ret = avcodec_open2(aenc_ctx, audio_codec, nullptr);
    if (ret < 0) {
        SetErrorLog(ret, "avcodec_open2 failed for audio");
        avcodec_free_context(&aenc_ctx);
        return nullptr;
    }
    return aenc_ctx;
}

static AVCodecContext* OpenVideoEncoderLibx264(AVFormatContext* ofmt_ctx, SIZE resolution, AVRational framerate)
{
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            SetErrorLog(AVERROR_ENCODER_NOT_FOUND, "avcodec_find_encoder failed for libx264/H.264");
            return nullptr;
        }
    }
    AVCodecContext* venc_ctx = avcodec_alloc_context3(codec);
    if (!venc_ctx) {
        SetErrorLog(AVERROR(ENOMEM), "avcodec_alloc_context3 failed for libx264");
        return nullptr;
    }
    venc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    venc_ctx->codec_id = AV_CODEC_ID_H264;
    venc_ctx->width = resolution.cx;
    venc_ctx->height = resolution.cy;
    venc_ctx->time_base = { 1, 90000 };
    venc_ctx->framerate = framerate;
    venc_ctx->max_b_frames = 3;
    venc_ctx->level = CalcVEncLv(resolution, framerate);
    venc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    if (ofmt_ctx->oformat && (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)) {
        venc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if (venc_ctx->priv_data) {
        av_opt_set(venc_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(venc_ctx->priv_data, "tune", "grain", 0);
        av_opt_set(venc_ctx->priv_data, "crf", "20", 0);
        av_opt_set(venc_ctx->priv_data, "profile", "high", 0);
    }
    int ret = avcodec_open2(venc_ctx, codec, nullptr);
    if (ret < 0) {
        SetErrorLog(ret, "avcodec_open2 failed for libx264");
        avcodec_free_context(&venc_ctx);
        return nullptr;
    }
    return venc_ctx;
}

static AVCodecContext* OpenVideoEncoderQSV(AVFormatContext* ofmt_ctx, SIZE resolution, AVRational framerate)
{
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_qsv");
    if (!codec) {
        SetErrorLog(AVERROR_ENCODER_NOT_FOUND, "avcodec_find_encoder_by_name failed for h264_qsv");
        return nullptr;
    }
    AVCodecContext* venc_ctx = avcodec_alloc_context3(codec);
    if (!venc_ctx) {
        SetErrorLog(AVERROR(ENOMEM), "avcodec_alloc_context3 failed for h264_qsv");
        return nullptr;
    }
    venc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    venc_ctx->codec_id = AV_CODEC_ID_H264;
    venc_ctx->width = resolution.cx;
    venc_ctx->height = resolution.cy;
    venc_ctx->time_base = { 1, 90000 };
    venc_ctx->framerate = framerate;
    venc_ctx->max_b_frames = 0;
    venc_ctx->level = CalcVEncLv(resolution, framerate);
    venc_ctx->pix_fmt = AV_PIX_FMT_NV12;
    if (ofmt_ctx->oformat && (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)) {
        venc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if (venc_ctx->priv_data) {
        av_opt_set(venc_ctx->priv_data, "rc", "cqp", 0);
        av_opt_set_int(venc_ctx->priv_data, "global_quality", 20, 0);
        av_opt_set_int(venc_ctx->priv_data, "bf", 0, 0);
    }
    AVBufferRef* hw_device_ctx = nullptr;
    AVDictionary* dict = nullptr;
    if constexpr (QSV_UPLOAD_MODE == 1) {
        av_dict_set(&dict, "child_device_type", "d3d11va", 0);
    }
    int ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_QSV, "auto", dict, 0);
    if constexpr (QSV_UPLOAD_MODE == 1) {
        av_dict_free(&dict);
    }
    if (ret < 0) {
        SetErrorLog(ret, "Create QSV device context failed");
        avcodec_free_context(&venc_ctx);
        return nullptr;
    }
    venc_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    av_buffer_unref(&hw_device_ctx);

    if constexpr (QSV_UPLOAD_MODE == 1) {
        AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(venc_ctx->hw_device_ctx);
        if (!hw_frames_ref) {
            SetErrorLog(0, "Failed to allocate hw frames context");
            avcodec_free_context(&venc_ctx);
            return nullptr;
        }
        AVHWFramesContext* frames_ctx = (AVHWFramesContext*)hw_frames_ref->data;
        frames_ctx->format = AV_PIX_FMT_QSV;
        frames_ctx->sw_format = AV_PIX_FMT_NV12;
        frames_ctx->width = venc_ctx->width;
        frames_ctx->height = venc_ctx->height;
        frames_ctx->initial_pool_size = 20;
        ret = av_hwframe_ctx_init(hw_frames_ref);
        if (ret < 0) {
            SetErrorLog(ret, "Failed to init hw frames context");
            av_buffer_unref(&hw_frames_ref);
            avcodec_free_context(&venc_ctx);
            return nullptr;
        }
        venc_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
        av_buffer_unref(&hw_frames_ref);
        venc_ctx->pix_fmt = AV_PIX_FMT_QSV;
    }

    ret = avcodec_open2(venc_ctx, codec, nullptr);
    if (ret < 0) {
        SetErrorLog(ret, "avcodec_open2 failed for h264_qsv");
        avcodec_free_context(&venc_ctx);
        return nullptr;
    }
    return venc_ctx;
}

VOID SetErrorLog(int code, LPCSTR fmt, ...)
{
    static CHAR buffer[AV_ERROR_MAX_STRING_SIZE]{};
    static CHAR msg[4096]{};
    if (code != 0) {
        av_strerror(code, buffer, AV_ERROR_MAX_STRING_SIZE);
    }
    else {
        buffer[0] = '\0';
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    av_log(nullptr, AV_LOG_ERROR, "%s: %s\n", msg, buffer);
}

static int CalcVEncLv(SIZE resolution, AVRational framerate)
{
    typedef struct LevelEntry {
        int level_id;
        int max_mb_per_sec;
        int max_mb_per_frame;
    } LevelEntry;
    static const LevelEntry level_table[] =
    {
        { 9,   1485,    99 }, {10,   1485,    99 }, {11,   3000,   396 },
        {12,   6000,   396 }, {13,  11880,   396 }, {20,  11880,   396 },
        {21,  19800,   792 }, {22,  20250,  1620 }, {30,  40500,  1620 },
        {31, 108000,  3600 }, {32, 216000,  5120 }, {40, 245760,  8192 },
        {41, 245760,  8192 }, {42, 522240,  8704 }, {50, 589824, 22080 },
        {51, 983040, 36864 }, {52,2073600, 36864 }, {60,4177920,139264 },
        {61,8355840,139264 }, {62,16711680,139264}
    };
    int mb_width = (resolution.cx + 15) / 16;
    int mb_height = (resolution.cy + 15) / 16;
    int64_t mb_per_frame = (int64_t)mb_width * mb_height;
    double fps = av_q2d(framerate);
    double mb_per_sec = mb_per_frame * fps;
    for (size_t i = 0; i < sizeof(level_table) / sizeof(level_table[0]); i++)
    {
        const LevelEntry* level = &level_table[i];
        if (mb_per_frame <= level->max_mb_per_frame && mb_per_sec <= level->max_mb_per_sec)
            return level->level_id;
    }
    return level_table[sizeof(level_table) / sizeof(level_table[0]) - 1].level_id;
}

static void ShowPixFmts(const AVCodec* codec, AVCodecContext* codec_ctx)
{
    printf(u8"%s 支持的像素格式：\n", codec->name);
    const enum AVPixelFormat* pix_fmts = nullptr;
    int num_pix_fmts = 0;
    int ret = avcodec_get_supported_config(
        codec_ctx, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0,
        (const void**)&pix_fmts, &num_pix_fmts);
    if (ret < 0 || !pix_fmts || num_pix_fmts <= 0)
    {
        printf(u8"（无法获取像素格式列表，错误码：%d）\n", ret);
        return;
    }
    for (int i = 0; i < num_pix_fmts; i++)
    {
        const char* name = av_get_pix_fmt_name(pix_fmts[i]);
        printf("  %s\n", name ? name : "Unknown");
    }
}

static void ShowEncoder(AVCodecID id)
{
    const AVCodec* codec = nullptr;
    void* opaque = nullptr;
    int count = 0;
    BOOL IsALL = id == AVCodecID{};
    printf("ENCODERS:\n");
    while ((codec = av_codec_iterate(&opaque)))
    {
        if (av_codec_is_encoder(codec) && (IsALL ? TRUE : codec->id == id))
        {
            printf("  [%3d] %-20s - %s\n", ++count, codec->name, codec->long_name ? codec->long_name : "N/A");
        }
    }
    printf("CNT: %d\n", count);
}