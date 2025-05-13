#ifndef HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H
#define HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H

#include "Action.h"
#include "Includes.h"

class ActionGlacierEvolutionDeltaH : public Action {
  public:
    ActionGlacierEvolutionDeltaH();

    ~ActionGlacierEvolutionDeltaH() override = default;

    void ActionGlacierEvolutionDeltaH::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                       const axxd& areas, const axxd& volumes);

    bool ActionGlacierEvolutionDeltaH::Init();

    bool Apply() override;

    string GetLandCoverName() {
        return m_landCoverName;
    }

    axi GetHydroUnitIds() {
        return m_hydroUnitIds;
    }

    axxd GetLookupTableArea() {
        return m_tableArea;
    }

    axxd GetLookupTableVolume() {
        return m_tableVolume;
    }

  protected:
    int m_lastRow{0};
    string m_landCoverName;
    axi m_hydroUnitIds;
    axxd m_tableArea;
    axxd m_tableVolume;
    double m_initialGlacierWE{0.0};
};

#endif  // HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H
