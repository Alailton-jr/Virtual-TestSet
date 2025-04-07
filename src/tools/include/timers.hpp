#ifndef TIMERS_HPP
#define TIMERS_HPP

#include <time.h>
#include <iostream>


class Timer{
public:
    struct timespec next_period;

    void increment_period(long period_ns) {
        next_period.tv_nsec += period_ns;
        while (next_period.tv_nsec >= 1000000000) {
            next_period.tv_sec += 1;
            next_period.tv_nsec -= 1000000000;
        }
    }

    void start_period(long period_ns) {
        clock_gettime(CLOCK_REALTIME, &next_period);
        increment_period(period_ns);
    }

    void start_period(struct timespec initial_time) {
        next_period.tv_sec = initial_time.tv_sec;
        next_period.tv_nsec = initial_time.tv_nsec;
    }

    void wait_period(long period_ns) {
        if(clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_period, NULL) == -1){
            std::cerr << "Error in clock_nanosleep" << std::endl;
        };
        increment_period(period_ns);
    }
};















#endif // TIMERS_HPP