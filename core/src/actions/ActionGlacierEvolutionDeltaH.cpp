#include "ActionGlacierEvolutionDeltaH.h"

#include "HydroUnit.h"
#include "ModelHydro.h"

ActionGlacierEvolutionDeltaH::ActionGlacierEvolutionDeltaH() = default;

void ActionGlacierEvolutionDeltaH::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                   const axi& increments, const axxd& areas, const axxd& volumes) {
    m_month = month;
    m_landCoverName = landCoverName;
    m_hydroUnitIds = hydroUnitIds;
    m_increments = increments;
    m_areas = areas;
    m_volumes = volumes;
}

bool ActionGlacierEvolutionDeltaH::Apply(double) {

    return 0.0;
}

