#include "TimeUtil.h"
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

uint64_t GetCurrentTimeNano64(void) {
	timespec NSec;
	int Res = timespec_get(&NSec, TIME_UTC);
	assert(Res == TIME_UTC);
	uint64_t Current = NSec.tv_sec * 1000000000 + NSec.tv_nsec;
	return Current;
}

Timer::Timer(void) : Delta(0.0f), StartTime(0), EndTime(0) {}

void Timer::Begin(void) {
	StartTime = GetCurrentTimeNano64();
}

void Timer::End(void) {
	uint64_t Current = GetCurrentTimeNano64();

	uint64_t Duration = Current - StartTime;
	Delta = (double)Duration / 1e9;
}

void Timer::DebugTime(void) {
	printf("Time: %f seconds\t\t%f FPS\n", (float)Delta, 1.0f / (float)Delta);
} 