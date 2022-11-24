#ifndef HYDROBRICKS_SETTING_BASIN_H
#define HYDROBRICKS_SETTING_BASIN_H

#include "Includes.h"
#include "Parameter.h"

struct LandCoverSettings {
    string name;
    string type;
    double fraction;
};

struct SurfaceComponentSettings {
    string name;
    string type;
    double fraction;
};

struct HydroUnitSettings {
    int id;
    double area;
    double elevation;
    std::vector<LandCoverSettings> landCovers;
    std::vector<SurfaceComponentSettings> surfaceComponents;
};

class SettingsBasin : public wxObject {
  public:
    explicit SettingsBasin();

    ~SettingsBasin() override;

    void AddHydroUnit(int id, double area, double elevation = 0.0);

    void AddLandCover(const string& name, const string& type = "", double fraction = 1.0);

    void SelectUnit(int index);

    bool Parse(const string& path);

    HydroUnitSettings GetHydroUnitSettings(int index) {
        wxASSERT(m_hydroUnits.size() > index);
        return m_hydroUnits[index];
    }

    LandCoverSettings GetLandCoverSettings(int index) {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->landCovers.size() > index);
        return m_selectedHydroUnit->landCovers[index];
    }

    SurfaceComponentSettings GetSurfaceComponentSettings(int index) {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->surfaceComponents.size() > index);
        return m_selectedHydroUnit->surfaceComponents[index];
    }

    int GetHydroUnitsNb() {
        return int(m_hydroUnits.size());
    }

    int GetLandCoversNb() {
        wxASSERT(m_selectedHydroUnit);
        return int(m_selectedHydroUnit->landCovers.size());
    }

    int GetSurfaceComponentsNb() {
        wxASSERT(m_selectedHydroUnit);
        return int(m_selectedHydroUnit->surfaceComponents.size());
    }

  protected:
  private:
    std::vector<HydroUnitSettings> m_hydroUnits;
    HydroUnitSettings* m_selectedHydroUnit;
};

#endif  // HYDROBRICKS_SETTING_BASIN_H
