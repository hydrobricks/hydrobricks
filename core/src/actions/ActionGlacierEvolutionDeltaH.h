#ifndef HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H
#define HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H

#include "Action.h"
#include "Includes.h"

class ActionGlacierEvolutionDeltaH : public Action {
  public:
    ActionGlacierEvolutionDeltaH();

    ~ActionGlacierEvolutionDeltaH() override = default;

    void ActionGlacierEvolutionDeltaH::AddLookupTables(int month, const string& landCoverName, const axi& hydroUnitIds,
                                                       const axi& increments, const axxd& areas, const axxd& volumes);

    bool Apply(double date) override;

    int GetMonth() {
        return m_month;
    }

    string GetLandCoverName() {
        return m_landCoverName;
    }

    axi GetHydroUnitIds() {
        return m_hydroUnitIds;
    }

    axi GetIncrements() {
        return m_increments;
    }

    axxd GetAreas() {
        return m_areas;
    }

    axxd GetVolumes() {
        return m_volumes;
    }

  protected:
    int m_month;
    string m_landCoverName;
    axi m_hydroUnitIds;
    axi m_increments;
    axxd m_areas;
    axxd m_volumes;
};

#endif  // HYDROBRICKS_ACTION_GLACIER_EVOLUTION_DELTA_H_H
