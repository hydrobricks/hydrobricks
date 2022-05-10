#include "ProcessET.h"
#include "Brick.h"

ProcessET::ProcessET(Brick* brick)
    : Process(brick)
{}

bool ProcessET::IsOk() {
    if (m_outputs.size() != 1) {
        wxLogError(_("ET should have a single output."));
        return false;
    }

    return true;
}

int ProcessET::GetConnectionsNb() {
    return 1;
}