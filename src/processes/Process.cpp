#include "Process.h"

#include "Brick.h"

Process::Process(Brick* brick)
    : m_brick(brick)
{}

double Process::GetWaterExtraction() {
    return 0;
}

double Process::GetWaterAddition() {
    return 0;
}