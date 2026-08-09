#pragma once
#include <minwindef.h>
#include <stdint.h>
// ================================
// 辅助类型定义
// ================================

// 类型: PROCSAMPLE
// 描述: 处理样本函数指针，将一个 type 数据经过 cls 的处理后得到一个新的 type 数据
template <typename cls, typename type>
using PROCSAMPLE = type(cls::*)(type);
// 类型: SENDSAMPLE
// 描述: 送入样本函数指针，将一个 type 数据送入 cls 的处理器，返回处理结果
template <typename cls, typename type>
using SENDSAMPLE = BOOL(cls::*)(type);
// 类型: RELEASESAMPLE
// 描述: 释放样本函数指针，通过 cls 释放通过 cls 处理后创建的得到数据（值拷贝的数据不需要释放，主要释放的是指针数据和引用数据）
template <typename cls, typename type>
using RELEASESAMPLE = BOOL(cls::*)(type);

// ================================
// 类型: StreamPipe
// 描述: 流式管道模板类，根据输入/输出增量（采样周期）对离散样本进行时域缩放。采用最近邻法则，每个输入样本负责输出其时间区间内的所有输出样本。
// ================================
template <typename cls, typename type>
class StreamPipe
{
private:
	struct
	{
		int64_t num = 0;	// 流时域值
		int64_t inc = 0;	// 样本递增时值
	} input, output;
	cls* obj = NULL;
	PROCSAMPLE<cls, type> proc = NULL;
	SENDSAMPLE<cls, type> send = NULL;
	RELEASESAMPLE<cls, type> release = NULL;
	type SampleLast = {};
public:
	// 函数: StreamPipe
	// 功能: 构造一个流式管道
	StreamPipe(int64_t qwInputInc, int64_t qwOutputInc, cls* object, PROCSAMPLE<cls, type> pProcess, SENDSAMPLE<cls, type> pSend, RELEASESAMPLE<cls, type> pRelease)
	{
		input = { 0,qwInputInc };
		output = { 0,qwOutputInc };
		obj = object;
		proc = pProcess;
		send = pSend;
		release = pRelease;
		SampleLast = {};
	}
	// 函数: PushSample
	// 功能: 向管道推入一个离散样本
	VOID PushSample(type sample)
	{
		// 计算区间边界
		int64_t right = input.num * 2 + input.inc;
		// 区间判断
		if (output.num * 2 < right)
		{
			// 释放存在旧有样本
			if (SampleLast && release)
			{
				(obj->*release)(SampleLast);
			}

			// 调用处理函数生成输出样本
			SampleLast = (obj->*proc)(sample);

			// 输出所有落在该区间内的输出样本
			while (output.num * 2 < right)
			{
				// 发送输出样本，忽略返回值（可根据需要处理）
				(obj->*send)(SampleLast);

				// 推进输出时间点
				output.num += output.inc;
			}
		}
		// 推进输入时间点
		input.num += input.inc;
	}
	// 函数: Flush
	// 功能: 用最后一个样本刷新至结束时间
	void Flush(int64_t qwEndNum) {
		if (SampleLast)
		{
			while (output.num * 2 < qwEndNum * 2)
			{
				(obj->*send)(SampleLast);
				output.num += output.inc;
			}
			if (release)
			{
				(obj->*release)(SampleLast);
			}
		}
	}
};