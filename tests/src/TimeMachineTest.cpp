
#include <gtest/gtest.h>

#include "TimeMachine.h"

TEST(TimeMachine, IncrementMonth) {
    TimeMachine timer(wxDateTime(1, wxDateTime::Jan, 2020),
                      wxDateTime(1, wxDateTime::Mar, 2020),
                      1, Month);
    timer.IncrementTime();

    EXPECT_TRUE(timer.GetDate().IsEqualTo(wxDateTime(1, wxDateTime::Feb, 2020)));
}

TEST(TimeMachine, IncrementWeek) {
    TimeMachine timer(wxDateTime(1, wxDateTime::Jan, 2020),
                      wxDateTime(1, wxDateTime::Mar, 2020),
                      1, Week);
    timer.IncrementTime();

    EXPECT_TRUE(timer.GetDate().IsEqualTo(wxDateTime(8, wxDateTime::Jan, 2020)));
}

TEST(TimeMachine, IncrementDay) {
    TimeMachine timer(wxDateTime(1, wxDateTime::Jan, 2020),
                      wxDateTime(1, wxDateTime::Mar, 2020),
                      1, Day);
    timer.IncrementTime();

    EXPECT_TRUE(timer.GetDate().IsEqualTo(wxDateTime(2, wxDateTime::Jan, 2020)));
}

TEST(TimeMachine, IncrementHour) {
    TimeMachine timer(wxDateTime(1, wxDateTime::Jan, 2020),
                      wxDateTime(1, wxDateTime::Mar, 2020),
                      1, Hour);
    timer.IncrementTime();

    EXPECT_TRUE(timer.GetDate().IsEqualTo(wxDateTime(1, wxDateTime::Jan, 2020, 1)));
}

TEST(TimeMachine, IncrementMinute) {
    TimeMachine timer(wxDateTime(1, wxDateTime::Jan, 2020),
                      wxDateTime(1, wxDateTime::Mar, 2020),
                      1, Minute);
    timer.IncrementTime();

    EXPECT_TRUE(timer.GetDate().IsEqualTo(wxDateTime(1, wxDateTime::Jan, 2020, 0, 1)));
}