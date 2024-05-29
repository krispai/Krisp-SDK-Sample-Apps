#ifndef KRISP_METRICS_HPP
#define KRISP_METRICS_HPP

// Headers from third party library
#include <iostream>
#include <ctime>
#include <chrono>

namespace KRISP {
namespace TEST_UTILS {

typedef std::chrono::high_resolution_clock::time_point PRF_TIME;

typedef struct timeInfo_t
{
	uint64_t lastSystemTime;
	uint64_t lastProcessTime;
	uint64_t lastRunTick;
} TimeInfo;

class Metrics
{
private:
	// Members for Time measurement
	PRF_TIME startTime_;
	PRF_TIME endTime_;

	// Members for CPU-usage
	TimeInfo timeInfo_;

private:
	// Functions for CPU-usage
	double   calculateCurrentProcessUsage();
	static uint64_t GetProcessTime();
	bool     isEnoughTimePassed(double elapsedTimeMs);
	inline bool isFirstRun() { return (timeInfo_.lastRunTick == 0); }

public:
	Metrics() : timeInfo_{}
	{}

	// Functions for Time measurement
	void startTimePoint()
	{
		startTime_ = std::chrono::high_resolution_clock::now();
	}

	void endTimePoint()
	{
		endTime_ = std::chrono::high_resolution_clock::now();
	}

	double getTimeInUsec()
	{
		std::chrono::duration<double, std::milli> ms_double = endTime_ - startTime_;
		return ms_double.count() * 1000;
	}

	double getTimeDurationMilli() const {
		std::chrono::duration<double, std::milli> ms_double = endTime_ - startTime_;
		return ms_double.count();
	}

	double getTimeDurationMicro() const {
		std::chrono::duration<double, std::micro> ms_double = endTime_ - startTime_;
		return ms_double.count();
	}


    static uint64_t getCPUTimes();


	// Functions for CPU-usage measurement
	double  getCPUUsageOnCurrentProcess();
	double  getCPUUsageOnCurrentProcess(double elapsedTimeMs);

	// Functions for MEMORY usage measurement
	double getCurrentProcessMemory();

	// Sleep: Time period in milliseconds for which execution of the program is suspended
	void sleepMs(uint32_t milliseconds);

};


} // end of namespace TEST_UTILS
} // end of namespace KRISP

#endif // KRISP_METRICS_HPP
