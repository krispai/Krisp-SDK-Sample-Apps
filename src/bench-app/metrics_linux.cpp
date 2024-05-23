#if defined __linux__
#include "metrics.hpp"
#include <time.h>
#include <unistd.h>
#include <string.h>

namespace KRISP {
namespace TEST_UTILS {

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
	double memorySize = -1;
    char statusFileName[256];
    sprintf(statusFileName, "/proc/%d/status", (uint32_t)getpid());

    FILE* pFile = fopen(statusFileName, "r");
    char lineBuf[2048];
    if (pFile)
    {
        while (fgets(lineBuf, 2048, pFile))
        {
            if (0 == strncmp(lineBuf, "VmRSS:", 6))
            {
                char* cursor = lineBuf + 6;
                /* Get rid of preceding blanks */
                while (!isdigit(*cursor))
                {
                    cursor++;
                }
                /* Get rid of following blanks */
                char* numString = cursor;
                while (isdigit(*cursor))
                {
                    cursor++;
                }
                *cursor = '\0';
                memorySize = (double)atoi(numString); // / 1024.0;
                break;
            }
         }

        fclose(pFile);
     }

    return memorySize; //  Kb
}

// Sleep: Time period in milliseconds for which execution of the program is suspended
void Metrics::sleepMs(uint32_t milliseconds)
{
    usleep(milliseconds * 1000);
}

// Functions for CPU-usage
double   Metrics::calculateCurrentProcessUsage()
{
    double cpuUsage = -1;
    uint64_t currSystemTime = 0;

    char statFileName[256];
    sprintf(statFileName, "/proc/%d/stat", (uint32_t)getpid());

    if (isFirstRun())
    {
        // calculate total system time from file /proc/stat,
        // the content is like: cpu 7967 550 4155 489328
        FILE* pStatFile = fopen("/proc/stat", "r"); // TODO: Move outside of 'if' statemant.
        if (pStatFile)
        {
            // skip unnecessary content
            fscanf(pStatFile, "cpu");
            unsigned long long sysTime;
            int valuesToRead = 4;
            for (int i = 0; i < valuesToRead; i++)
            {
                fscanf(pStatFile, "%llu", &sysTime);
                timeInfo_.lastSystemTime += sysTime;
            }
            fclose(pStatFile);
        }

        // get user mode time, kernel mode time, start time
        // for current process from file /proc/[pid]/stat
        pStatFile = fopen(statFileName, "r");
        if (pStatFile)
        {
            // skip unnecessary content
            int valuesToSkip = 13;
            char tempBuff[2048];
            for (int i = 0; i < valuesToSkip; i++)
                fscanf(pStatFile, "%s", tempBuff);

            uint64_t procUserTime = 0;
            uint64_t procKernelTime = 0;
            // fscanf(pStatFile, "%llu %llu", &timeInfo_.prevProcUserTime, &timeInfo_.prevProcKernelTime);
            fscanf(pStatFile, "%llu %llu", &procUserTime, &procKernelTime);
            timeInfo_.lastProcessTime = procUserTime + procKernelTime;

            fclose(pStatFile);
        }
    }

    // calculate total system time from file /proc/stat,
    // the content is like: cpu 7967 550 4155 489328
    FILE* pStatFile = fopen("/proc/stat", "r");
    if (pStatFile)
    {
        // skip unnecessary content
        fscanf(pStatFile, "cpu");
        unsigned long long sysTime;
        int valuesToRead = 4;
        for (int i = 0; i < valuesToRead; i++)
        {
            fscanf(pStatFile, "%llu", &sysTime);
            currSystemTime += sysTime;
        }
        fclose(pStatFile);
    }

    auto systemTimeDelta = currSystemTime - timeInfo_.lastSystemTime;
    auto currProcessTime = GetProcessTime();
    auto processTimeDelta = currProcessTime - timeInfo_.lastProcessTime;
    if (systemTimeDelta > 0)
        cpuUsage = (double)(100.0 * processTimeDelta) / systemTimeDelta;

    timeInfo_.lastSystemTime = currSystemTime;
    timeInfo_.lastProcessTime = currProcessTime;
    timeInfo_.lastRunTick = clock();

    return cpuUsage;
}

uint64_t Metrics::GetProcessTime()
{
    uint64_t totalProcessTime = 0;

    uint64_t currProcUserTime = 0;
    uint64_t currProcChildsUserTime = 0;
    uint64_t currProcKernelTime = 0;
    uint64_t currProcChildsKernelTime = 0;

    char statFileName[256];
    sprintf(statFileName, "/proc/%d/stat", (uint32_t)getpid());

    FILE* pStatFile = fopen(statFileName, "r");
    if (pStatFile)
    {
        // skip unnecessary content
        char tempBuff[2048];
        int valuesToSkip = 13;
        for (int i = 0; i < valuesToSkip; i++)
            fscanf(pStatFile, "%s", tempBuff);

        fscanf(pStatFile, "%llu %llu %llu %llu", &currProcUserTime, &currProcKernelTime, &currProcChildsUserTime, &currProcChildsKernelTime);
        fclose(pStatFile);
    }

    totalProcessTime = currProcUserTime + currProcChildsUserTime + currProcKernelTime + currProcChildsKernelTime;

    return totalProcessTime;
}

bool    Metrics::isEnoughTimePassed(double elapsedTimeMs)
{
    uint64_t currentElapsedTime = 0;
    double currentTickCount = static_cast<double>(clock());

    currentElapsedTime = (currentTickCount - timeInfo_.lastRunTick) / 1000/*CLOCKS_PER_SEC*/;

    return currentElapsedTime > elapsedTimeMs;
}

} // end of namespace TEST_UTILS
} // end of namespace KRISP

#endif
