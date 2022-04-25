#include "SolverRungeKuttaMethod.h"

SolverRungeKuttaMethod::SolverRungeKuttaMethod()
    : Solver()
{
    m_nIterations = 4;
}

bool SolverRungeKuttaMethod::Solve(SubBasin* basin) {

    int nUnits = basin->GetHydroUnitsCount();
/*
    for (int iUnit = 0; iUnit < nUnits; ++iUnit) {
        HydroUnit* unit = basin->GetHydroUnit(iUnit);
        int nBricks = unit->GetBricksCount();

        for (int iBrick = 0; iBrick < nBricks; ++iBrick) {
            Brick* brick = unit->GetBrick(iBrick);
            brick->Compute();
        }
    }*/


    // https://www.codesansar.com/numerical-methods/runge-kutta-rk-fourth-order-using-cpp-output.htm
    // https://www.geeksforgeeks.org/runge-kutta-4th-order-method-solve-differential-equation/
    // https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods
    // https://keisan.casio.com/exec/system/1222997077


    return false;
}