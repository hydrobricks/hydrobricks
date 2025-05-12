#include "ActionGlacierEvolutionDeltaH.h"

#include "HydroUnit.h"
#include "ModelHydro.h"

ActionGlacierEvolutionDeltaH::ActionGlacierEvolutionDeltaH() = default;

void ActionGlacierEvolutionDeltaH::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                   const axxd& areas, const axxd& volumes) {
    m_recursive = true;
    m_recursiveMonths.push_back(month);
    m_recursiveDays.push_back(1);
    m_landCoverName = landCoverName;
    m_hydroUnitIds = hydroUnitIds;
    m_tableArea = areas;
    m_tableVolume = volumes;
}

bool ActionGlacierEvolutionDeltaH::Apply() {

    return false;
}

