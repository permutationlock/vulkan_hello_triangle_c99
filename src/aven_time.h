#ifndef AVEN_TIME_H
#define AVEN_TIME_H

#include "aven.h"

#if defined(_WIN32)
    typedef int64_t TimeSpec;
#elif defined(__linux__)
    typedef struct {
        long tv_sec;
        long tv_nsec;
    } TimeSpec;
#endif

#if defined(_WIN32)
    void QueryPerformanceCounter(int64_t *ts);
    void QueryPerformanceFrequency(int64_t *qpf);

    static int timespec_now(TimeSpec *ts) {
        QueryPerformanceCounter(ts);
        return 0;
    }

    static int64_t timespec_diff(TimeSpec *end, TimeSpec *start) {
        int64_t qpf;
        QueryPerformanceFrequency(&qpf);
        double scale = (double)(1000.0 * 1000.0 * 1000.0) / (double)qpf;
        double qpc = (double)(*end - *start);
        return (int64_t)(qpc * scale);
    }
#elif defined(__linux__)
    int clock_gettime(int clockid, TimeSpec *ts);

    static int timespec_now(TimeSpec *ts) {
        return clock_gettime(1, ts);
    }

    static int64_t timespec_diff(TimeSpec *end, TimeSpec *start) {
        int64_t seconds = (int64_t)end->tv_sec - (int64_t)start->tv_sec;
        int64_t sec_diff = seconds * 1000L * 1000L * 1000L;
        int64_t nsec_diff = (int64_t)end->tv_nsec - (int64_t)start->tv_nsec;
        return sec_diff + nsec_diff;
    }
#endif

#endif // AVEN_TIME_H
