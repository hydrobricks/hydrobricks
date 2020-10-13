
#include <gtest/gtest.h>

#include "TimeMachine.h"

TEST(TimeMachine, IncrementDay) {
    TimeMachine timer(wxDateTime(1, wxDateTime::Jan, 2020),
                      wxDateTime(1, wxDateTime::Mar, 2020),
                      1, Day);
    timer.IncrementTime();

    EXPECT_TRUE(timer.GetDate().IsEqualTo(wxDateTime(2, wxDateTime::Jan, 2020)));
}
