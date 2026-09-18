#include <gtest/gtest.h>

#include "TimeMachine.h"

TEST(TimeMachine, IncrementWeek) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, TimeUnit::Week);
    timer.IncrementTime();

    EXPECT_EQ(timer.GetDate(), GetMJD(2020, 1, 8));
}

TEST(TimeMachine, IncrementDay) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, TimeUnit::Day);
    timer.IncrementTime();

    EXPECT_EQ(timer.GetDate(), GetMJD(2020, 1, 2));
}

TEST(TimeMachine, IncrementHour) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, TimeUnit::Hour);
    timer.IncrementTime();

    EXPECT_DOUBLE_EQ(timer.GetDate(), GetMJD(2020, 1, 1, 1));
}

TEST(TimeMachine, IncrementMinute) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, TimeUnit::Minute);
    timer.IncrementTime();

    EXPECT_DOUBLE_EQ(timer.GetDate(), GetMJD(2020, 1, 1, 0, 1));
}

TEST(TimeMachine, IsNotOver) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), 1, TimeUnit::Day);
    timer.IncrementTime();

    EXPECT_FALSE(timer.IsOver());
}

TEST(TimeMachine, IsOver) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), 1, TimeUnit::Day);
    timer.IncrementTime();
    timer.IncrementTime();
    timer.IncrementTime();

    EXPECT_TRUE(timer.IsOver());
}

TEST(TimeMachine, HourlyStepsStayExactOverALongRun) {
    // The date is advanced on an exact count of minutes rather than by accumulating
    // 1/24 of a day, which is not representable in binary: after a year of hourly steps
    // the accumulated form drifts off the hour boundaries the forcing is indexed on.
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2021, 1, 1), 1, TimeUnit::Hour);
    for (int i = 0; i < 8760; ++i) {
        timer.IncrementTime();
    }

    EXPECT_DOUBLE_EQ(timer.GetDate(), GetMJD(2020, 12, 31, 0));
}

TEST(TimeMachine, HourlyStepCountKeepsTheLastStep) {
    // Truncating (end - start) / (1/24) can land just below the whole number of steps.
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 12, 31), 1, TimeUnit::Hour);

    EXPECT_EQ(timer.GetTimeStepCount(), 365 * 24 + 1);
}

TEST(TimeMachine, TwentyFourHourStepMatchesTheDailyStep) {
    TimeMachine hourly;
    hourly.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 24, TimeUnit::Hour);
    TimeMachine daily;
    daily.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, TimeUnit::Day);

    EXPECT_EQ(hourly.GetTimeStepCount(), daily.GetTimeStepCount());
    EXPECT_DOUBLE_EQ(*hourly.GetTimeStepPointer(), *daily.GetTimeStepPointer());

    for (int i = 0; i < 30; ++i) {
        hourly.IncrementTime();
        daily.IncrementTime();
    }
    EXPECT_DOUBLE_EQ(hourly.GetDate(), daily.GetDate());
}
