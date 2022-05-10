#include "helpers.h"

TimeMachine GenerateTimeMachineDaily() {
    TimeMachine timer;
    timer.Initialize(wxDateTime(1, wxDateTime::Jan, 2020),
                     wxDateTime(31, wxDateTime::Dec, 2020), 1, Day);
    return timer;
}
