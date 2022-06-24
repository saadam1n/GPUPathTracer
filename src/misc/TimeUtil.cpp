#include "TimeUtil.h"
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <ratio>

uint64_t GetCurrentTimeNano64(void) {
	return std::chrono::duration_cast<std::chrono::duration<uint64_t, std::nano>>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
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