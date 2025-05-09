#include "ActionGlacierEvolutionDeltaH.h"

#include "HydroUnit.h"
#include "ModelHydro.h"

ActionGlacierEvolutionDeltaH::ActionGlacierEvolutionDeltaH() = default;

void ActionGlacierEvolutionDeltaH::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                   const axxd& areas, const axxd& volumes) {
    m_month = month;
    m_landCoverName = landCoverName;
    m_hydroUnitIds = hydroUnitIds;
    m_tableArea = areas;
    m_tableVolume = volumes;
}

bool ActionGlacierEvolutionDeltaH::Apply(double) {

    return 0.0;
}

