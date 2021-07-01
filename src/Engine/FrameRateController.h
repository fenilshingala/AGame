#pragma once

class FrameRateController
{
public:
	FrameRateController(uint32_t maxFrameRate);
	~FrameRateController();

	void FrameStart();
	void FrameEnd();
	uint32_t GetFrameTicks();
	float GetFrameTime();

private:
	uint32_t mTickStart;
	uint32_t mTickEnd;
	uint32_t mFrameTicks;
	uint32_t mNeededTicksPerFrame;
};