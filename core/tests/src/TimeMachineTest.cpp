#include <gtest/gtest.h>

#include "TimeMachine.h"

TEST(TimeMachine, IncrementWeek) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, Week);
    timer.IncrementTime();

    EXPECT_EQ(timer.GetDate(), GetMJD(2020, 1, 8));
}

TEST(TimeMachine, IncrementDay) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, Day);
    timer.IncrementTime();

    EXPECT_EQ(timer.GetDate(), GetMJD(2020, 1, 2));
}

TEST(TimeMachine, IncrementHour) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, Hour);
    timer.IncrementTime();

    EXPECT_FLOAT_EQ(timer.GetDate(), GetMJD(2020, 1, 1, 1));
}

TEST(TimeMachine, IncrementMinute) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 3, 1), 1, Minute);
    timer.IncrementTime();

    EXPECT_FLOAT_EQ(timer.GetDate(), GetMJD(2020, 1, 1, 0, 1));
}

TEST(TimeMachine, IsNotOver) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), 1, Day);
    timer.IncrementTime();

    EXPECT_FALSE(timer.IsOver());
}

TEST(TimeMachine, IsOver) {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 3), 1, Day);
    timer.IncrementTime();
    timer.IncrementTime();
    timer.IncrementTime();

    EXPECT_TRUE(timer.IsOver());
}
