#include "helpers.h"

TimeMachine GenerateTimeMachineDaily() {
    TimeMachine timer;
    timer.Initialize(GetMJD(2020, 1, 1), GetMJD(2020, 1, 31), 1, Day);
    return timer;
}
