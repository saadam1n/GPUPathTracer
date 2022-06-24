#pragma once

#include <stdint.h>
#include <chrono>

uint64_t GetCurrentTimeNano64(void);

struct Timer {
	Timer(void);

	void Begin();
	void End();

	void DebugTime(void);

	uint64_t StartTime, EndTime;

	double Delta;
};