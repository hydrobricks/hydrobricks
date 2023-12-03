#ifndef HYDROBRICKS_HELPERS_H
#define HYDROBRICKS_HELPERS_H

#include "SettingsModel.h"
#include "TimeMachine.h"

TimeMachine GenerateTimeMachineDaily();

bool GenerateStructureSocont(SettingsModel& settings, vecStr& landCoverTypes, vecStr& landCoverNames,
                             int soilStorageNb = 1, const string& surfaceRunoff = "socont_runoff");

#endif  // HYDROBRICKS_HELPERS_H
