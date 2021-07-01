#include <stdint.h>
#include "FrameRateController.h"

#include <time.h>

FrameRateController::FrameRateController(uint32_t maxFrameRate) {
	mTickStart = mTickEnd = mFrameTicks = 0;

	if (0 != maxFrameRate)
		mNeededTicksPerFrame = CLOCKS_PER_SEC / maxFrameRate;
	else
		mNeededTicksPerFrame = 0;
}

FrameRateController::~FrameRateController() {

}

void FrameRateController::FrameStart() {
	mTickStart = clock();
}

void FrameRateController::FrameEnd() {
	mTickEnd = clock();
	while ((mTickEnd - mTickStart) < mNeededTicksPerFrame) {
		mTickEnd = clock();
	}
	mFrameTicks = mTickEnd - mTickStart;
}

uint32_t FrameRateController::GetFrameTicks() { return mFrameTicks; }
float FrameRateController::GetFrameTime() { return (float)mFrameTicks / (float)CLOCKS_PER_SEC; }