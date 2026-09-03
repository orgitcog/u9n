// Copyright Epic Games, Inc. All Rights Reserved.

#include "libGPUCounters.h"
#include "hwcpipe/counter_database.hpp"
#include "hwcpipe/gpu.hpp"
#include "hwcpipe/sampler.hpp"

static libGPUCountersLogCallback GHWCPipe_LogCallback = nullptr;
static hwcpipe::counter_database* GHWCPipe_CounterDatabase = nullptr;
static hwcpipe::gpu* GHWCPipe_Gpu = nullptr;
static hwcpipe::sampler<>* GHWCPipe_Sampler = nullptr;
static int64_t GHWCPipe_LastFramePrimitiveCounts[3] = { -1, -1, -1 };	// size 3 assumed in code
static double GHWCPipe_LastSampleDurationMicroSeconds = 0.0;

static void __attribute__((format(printf, 2, 3))) LogMessage(libGPUCountersLogLevel Level, const char* Format, ...)
{
	if (!GHWCPipe_LogCallback)
	{
		return;
	}

	char Buffer[1024];

	va_list Args;
	va_start(Args, Format);
	const int RequiredSize = vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	if (RequiredSize < sizeof(Buffer))
	{
		GHWCPipe_LogCallback(static_cast<uint8_t>(Level), Buffer);
	}
	else
	{
		char* AllocBuffer = (char*)malloc(RequiredSize + 1);
		vsnprintf(AllocBuffer, RequiredSize, Format, Args);
		GHWCPipe_LogCallback(static_cast<uint8_t>(Level), AllocBuffer);
		free(AllocBuffer);
	}
	va_end(Args);
}

#define UE_LOG(...) LogMessage(libGPUCountersLogLevel::Log, __VA_ARGS__)
#define UE_ERROR(...) LogMessage(libGPUCountersLogLevel::Error, __VA_ARGS__)

template<typename T>
static T GetSampleValue(const hwcpipe::counter_sample& Sample)
{
	switch (Sample.type) {
	case hwcpipe::counter_sample::type::uint64:
		return static_cast<T>(Sample.value.uint64);
	case hwcpipe::counter_sample::type::float64:
		return static_cast<T>(Sample.value.float64);
	}
	return static_cast<T>(-1);
}

static int32_t FormatSampleValue(char* Buffer, size_t BufferLen, const hwcpipe::counter_sample& Sample, hwcpipe_counter Counter)
{
	if (!GHWCPipe_CounterDatabase)
	{
		return snprintf(Buffer, BufferLen, "");
	}

	hwcpipe::counter_metadata Meta = {};
	if (GHWCPipe_CounterDatabase->describe_counter(Counter, Meta))
	{
		return snprintf(Buffer, BufferLen, "");
	}

	// these are formatted in million
	if (!strcasecmp(Meta.units, "cycles") ||
		!strcasecmp(Meta.units, "pixels") ||
		!strcasecmp(Meta.units, "quads") ||
		!strcasecmp(Meta.units, "primitives"))
	{
		return snprintf(Buffer, BufferLen, "%s: %.2fMln", Meta.name, static_cast<float>(GetSampleValue<uint64_t>(Sample)) / (1000.f * 1000.f));
	}

	// these are formatted in megabytes
	if (!strcasecmp(Meta.units, "bytes"))
	{
		return snprintf(Buffer, BufferLen, "%s: %.1fMB", Meta.name, static_cast<float>(GetSampleValue<uint64_t>(Sample)) / (1024.f * 1024.f));
	}

	// these are formatted in percentage
	if (!strcasecmp(Meta.units, "percent"))
	{
		return snprintf(Buffer, BufferLen, "%s: %.1f%%", Meta.name, static_cast<float>(GetSampleValue<double>(Sample) * 100.0));
	}

	// format the rest based on the data type
	if (Sample.type == hwcpipe::counter_sample::type::uint64)
	{
		return snprintf(Buffer, BufferLen, "%s: %lu", Meta.name, GetSampleValue<uint64_t>(Sample));
	}
	else
	{
		return snprintf(Buffer, BufferLen, "%s: %.3f", Meta.name, static_cast<float>(GetSampleValue<double>(Sample)));
	}
}

extern "C" void LIBGPUCOUNTERS_API libGPUCountersInit(libGPUCountersLogCallback Callback)
{
	GHWCPipe_LogCallback = Callback;

	static hwcpipe::gpu HWCPipe_Gpu(0);	// probe device 0
	if (HWCPipe_Gpu.valid())
	{
		static hwcpipe::counter_database CounterDatabase;

		// list all available counters
		hwcpipe::counter_metadata Meta = {};
		UE_LOG("HWCPipe: ProfileGPU 0 Supported counters:");
		for (hwcpipe_counter counter : CounterDatabase.counters_for_gpu(HWCPipe_Gpu)) {
			if (!CounterDatabase.describe_counter(counter, Meta))
			{
				UE_LOG("   [%d] %s (unit: '%s')", (int)counter, Meta.name, Meta.units);
			}
		}

		// setup a config to only add the counter we're interested in
		static hwcpipe::sampler_config SamplerConfig(HWCPipe_Gpu);
		if (SamplerConfig.add_counter(MaliGeomTotalPrim))
		{
			UE_ERROR("HWCPipe: Failed to add counter [%d] that supposedly supported", (int)MaliGeomTotalPrim);
		}
		else
		{
			static hwcpipe::sampler<> Sampler(SamplerConfig);
			if (!Sampler.start_sampling())
			{
				// all set! enable the system by setting the global pointers pointing to the local statics
				GHWCPipe_Sampler = &Sampler;
				GHWCPipe_Gpu = &HWCPipe_Gpu;
				GHWCPipe_CounterDatabase = &CounterDatabase;
			}
			else
			{
				UE_ERROR("HWCPipe: Failed to start sampler");
			}
		}
	}
	else
	{
		UE_LOG("HWCPipe: GPU 0 not valid");
	}
}

extern "C" void LIBGPUCOUNTERS_API libGPUCountersUpdate()
{
	if (!GHWCPipe_Sampler)
	{
		return;
	}

	const std::chrono::time_point<std::chrono::steady_clock> StartSampleTime = std::chrono::steady_clock::now();

	int64_t TotalPrimitives = -1;	// -1 means sampling failed
	if (!GHWCPipe_Sampler->sample_now())
	{
		hwcpipe::counter_sample Sample;
		if (!GHWCPipe_Sampler->get_counter_value(MaliGeomTotalPrim, Sample))
		{
			TotalPrimitives = GetSampleValue<int64_t>(Sample);
		}
	}

	GHWCPipe_LastFramePrimitiveCounts[2] = GHWCPipe_LastFramePrimitiveCounts[1];
	GHWCPipe_LastFramePrimitiveCounts[1] = GHWCPipe_LastFramePrimitiveCounts[0];
	GHWCPipe_LastFramePrimitiveCounts[0] = TotalPrimitives;

	const std::chrono::time_point<std::chrono::steady_clock> FinishedSampleTime = std::chrono::steady_clock::now();

	GHWCPipe_LastSampleDurationMicroSeconds = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(FinishedSampleTime - StartSampleTime).count();
}

extern "C" void LIBGPUCOUNTERS_API libGPUCountersLog()
{
	if (!GHWCPipe_Sampler)
	{
		UE_LOG("HWCPipe: not enabled, returning");
		return;
	}

	if (GHWCPipe_Sampler->sample_now())
	{
		UE_LOG("HWCPipe: sampling failed");
		return;
	}

	UE_LOG("HWCPipe: listing all supported counters");
	hwcpipe::counter_sample Sample;
	for (hwcpipe_counter Counter : GHWCPipe_CounterDatabase->counters_for_gpu(*GHWCPipe_Gpu))
	{
		if (!GHWCPipe_Sampler->get_counter_value(Counter, Sample))
		{
			char SampleText[256];
			FormatSampleValue(SampleText, sizeof(SampleText), Sample, Counter);
			UE_LOG("HWCPipe: [%d]: %s", Counter, SampleText);
		}
	}
	UE_LOG("HWCPipe: list complete");

	int64_t TotalPrimitives = -1;
	if (!GHWCPipe_Sampler->get_counter_value(MaliGeomTotalPrim, Sample))
	{
		TotalPrimitives = GetSampleValue<int64_t>(Sample);
	}
	UE_LOG("HWCPipe: primitive counts [%ld] %lld %lld %lld",
		TotalPrimitives,
		(long long)GHWCPipe_LastFramePrimitiveCounts[0],
		(long long)GHWCPipe_LastFramePrimitiveCounts[1],
		(long long)GHWCPipe_LastFramePrimitiveCounts[2]);
	UE_LOG("HWCPipe: primitive sample duration: %.1f micro seconds", (float)GHWCPipe_LastSampleDurationMicroSeconds);
}
