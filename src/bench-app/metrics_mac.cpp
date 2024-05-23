#if defined __MACH__
#include "metrics.hpp"

#include <mach/mach.h>
#include <mach/clock.h>
#include <unistd.h>
#include <libproc.h>

namespace KRISP {
namespace TEST_UTILS {

#define NANOSECONDS_PER_MICROSECOND 1000
#define MICROSECONDS_PER_SECOND     NANOSECONDS_PER_MICROSECOND * 1000

#define TIME_VALUE_TO_TIMEVAL(a, r) do {  \
            (r)->tv_sec = (a)->seconds;             \
            (r)->tv_usec = (a)->microseconds;       \
        } while (0)

static int64_t convertTimespecToMicros(const struct timespec& ts)
{
    // On 32-bit systems, the calculation cannot overflow int64_t.
    // 2**32 * 1000000 + 2**64 / 1000 < 2**63
    if (sizeof(ts.tv_sec) <= 4 && sizeof(ts.tv_nsec) <= 8) {
        int64_t result = ts.tv_sec;
        result *= MICROSECONDS_PER_SECOND;
        result += (ts.tv_nsec / NANOSECONDS_PER_MICROSECOND);
        return result;
    }
    int64_t result(ts.tv_sec);
    result *= MICROSECONDS_PER_SECOND;
    result += (ts.tv_nsec / NANOSECONDS_PER_MICROSECOND);
    return result;
}

static int64_t clockNow(clockid_t clk_id)
{
    struct timespec ts;
    clock_gettime(clk_id, &ts);
    return convertTimespecToMicros(ts);
}

static bool getTaskInfo(mach_port_t task, task_basic_info_64* pTaskInfoData)
{
    if (task == MACH_PORT_NULL)
        return false;
    mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
    kern_return_t kr = task_info(task,
        TASK_BASIC_INFO_64,
        reinterpret_cast<task_info_t>(pTaskInfoData),
        &count);
    // Most likely cause for failure: |task| is a zombie.
    return kr == KERN_SUCCESS;
}

int64_t timeValToMicroseconds(const struct timeval& tv)
{
    int64_t ret = tv.tv_sec;  // Avoid (int * int) integer overflow.
    ret *= MICROSECONDS_PER_SECOND;
    ret += tv.tv_usec;
    return ret;
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
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    struct task_basic_info taskInfo;

    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&taskInfo, &t_info_count))
    {
        return -1;
    }

    return (double)taskInfo.resident_size / 1024.0; // Kb
}

// Sleep: Time period in milliseconds for which execution of the program is suspended
void Metrics::sleepMs(uint32_t milliseconds)
{
    usleep(milliseconds * 1000);
}

// Functions for CPU-usage
double   Metrics::calculateCurrentProcessUsage()
{
    int64_t currentProcessTime = GetProcessTime();
    int64_t currentSystemTime = clockNow(CLOCK_MONOTONIC);

    if (0 == timeInfo_.lastProcessTime)
    {
        // First call, just set the last values.
        timeInfo_.lastProcessTime = currentProcessTime;
        timeInfo_.lastSystemTime = currentSystemTime;
        return 0;
    }

    int64_t processTimeDelta = currentProcessTime - timeInfo_.lastProcessTime;
    int64_t systemTimeDelta = currentSystemTime - timeInfo_.lastSystemTime;

    if (0 == systemTimeDelta)
        return 0;

    timeInfo_.lastProcessTime = currentProcessTime;
    timeInfo_.lastSystemTime = currentSystemTime;

    return (double)(100.0 * processTimeDelta) / systemTimeDelta;
}

uint64_t Metrics::GetProcessTime()
{
    uint64_t totalProcessTime = 0;

    mach_port_t task = mach_task_self();

    task_thread_times_info threadInfoData;
    mach_msg_type_number_t threadInfoCount = TASK_THREAD_TIMES_INFO_COUNT;
    kern_return_t kr = task_info(task,
        TASK_THREAD_TIMES_INFO,
        reinterpret_cast<task_info_t>(&threadInfoData),
        &threadInfoCount);

    if (kr != KERN_SUCCESS)
        return -1;

    task_basic_info_64 taskInfoData;
    if (!getTaskInfo(task, &taskInfoData))
        return -1;

    /* Set total_time. */
    // thread info contains live time...
    struct timeval userTimeval, systemTimeval, taskTimeval;
    TIME_VALUE_TO_TIMEVAL(&threadInfoData.user_time, &userTimeval);
    TIME_VALUE_TO_TIMEVAL(&threadInfoData.system_time, &systemTimeval);
    timeradd(&userTimeval, &systemTimeval, &taskTimeval);

    // ... task info contains terminated time.
    TIME_VALUE_TO_TIMEVAL(&taskInfoData.user_time, &userTimeval);
    TIME_VALUE_TO_TIMEVAL(&taskInfoData.system_time, &systemTimeval);
    timeradd(&userTimeval, &taskTimeval, &taskTimeval);
    timeradd(&systemTimeval, &taskTimeval, &taskTimeval);

    totalProcessTime = timeValToMicroseconds(taskTimeval);

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
