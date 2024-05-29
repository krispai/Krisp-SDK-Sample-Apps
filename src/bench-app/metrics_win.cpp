#if defined _WIN32 || defined __CYGWIN__

#include "Windows.h"
#include "metrics.hpp"
#include "psapi.h"

namespace KRISP {
namespace TEST_UTILS {

static uint64_t addTime(const FILETIME& ftA, const FILETIME& ftB)
{
    LARGE_INTEGER tempFtA, tempFtB;
    tempFtA.LowPart = ftA.dwLowDateTime;
    tempFtA.HighPart = ftA.dwHighDateTime;

    tempFtB.LowPart = ftB.dwLowDateTime;
    tempFtB.HighPart = ftB.dwHighDateTime;

    return tempFtA.QuadPart + tempFtB.QuadPart;
}

// Functions for CPU-usage measurement
double  Metrics::getCPUUsageOnCurrentProcess()
{
	return calculateCurrentProcessUsage();
}

double  Metrics::getCPUUsageOnCurrentProcess(double elapsedTimeMs)
{
	// If this is called too often, the measurement itself will greatly affect the results.
	if (!isEnoughTimePassed(elapsedTimeMs))
	{
		return -1;
	}

	return calculateCurrentProcessUsage();
}

// Functions for MEMORY usage measurement
double Metrics::getCurrentProcessMemory()
{
	PROCESS_MEMORY_COUNTERS_EX pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));

	return static_cast<double>(pmc.WorkingSetSize / 1024); //  Kb
}

// Sleep: Time period in milliseconds for which execution of the program is suspended
void Metrics::sleepMs(uint32_t milliseconds)
{
	Sleep(milliseconds);
}

uint64_t Metrics::getCPUTimes()
{
    uint64_t totalProcessTime = 0;

    FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

    if (!GetProcessTimes(GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
    {
        return -1;
    }

    totalProcessTime = addTime(ftProcKernel, ftProcUser);

    return totalProcessTime / 10;
}

// Functions for CPU-usage
double   Metrics::calculateCurrentProcessUsage()
{
    double cpuUsage = 0;
    FILETIME ftSysIdle, ftSysKernel, ftSysUser;
    uint64_t ftSystemTime = 0, ftProcessTime = 0;

    ftProcessTime = GetProcessTime();
    if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser))
    {
        return -1;
    }

    if (!isFirstRun())
    {
        ftSystemTime = addTime(ftSysKernel, ftSysUser);
        uint64_t ftSystemTimeDelta = ftSystemTime - timeInfo_.lastSystemTime;
        uint64_t ftProcessTimeDelta = ftProcessTime - timeInfo_.lastProcessTime;

        if (ftSystemTimeDelta > 0)
        {
            cpuUsage = static_cast<double>((100.0 * ftProcessTimeDelta) / ftSystemTimeDelta);
        }
    }

    timeInfo_.lastSystemTime = ftSystemTime;
    timeInfo_.lastProcessTime = ftProcessTime;
    timeInfo_.lastRunTick = GetTickCount64();

    return cpuUsage;
}

uint64_t Metrics::GetProcessTime()
{
    uint64_t totalProcessTime = 0;

    FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

    if (!GetProcessTimes(GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
    {
        return -1;
    }

    totalProcessTime = addTime(ftProcKernel, ftProcUser);

    return totalProcessTime;
}



bool    Metrics::isEnoughTimePassed(double elapsedTimeMs)
{
    uint64_t currentElapsedTime = 0;

    uint64_t currentTickCount = GetTickCount64();
    currentElapsedTime = currentTickCount - timeInfo_.lastRunTick;

    return currentElapsedTime > elapsedTimeMs;
}

} // end of namespace TEST_UTILS
} // end of namespace KRISP

#endif
