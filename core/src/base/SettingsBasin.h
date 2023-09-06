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
    double slope;
    double aspect;
    vector<LandCoverSettings> landCovers;
    vector<SurfaceComponentSettings> surfaceComponents;
};

class SettingsBasin : public wxObject {
  public:
    explicit SettingsBasin();

    ~SettingsBasin() override;

    void AddHydroUnit(int id, double area, double elevation = NAN_D, double slope = NAN_D, double aspect = NAN_D);

    void AddLandCover(const string& name, const string& type = "", double fraction = 1.0);

    void SelectUnit(int index);

    bool Parse(const string& path);

    HydroUnitSettings GetHydroUnitSettings(int index) const {
        wxASSERT(m_hydroUnits.size() > index);
        return m_hydroUnits[index];
    }

    LandCoverSettings GetLandCoverSettings(int index) const {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->landCovers.size() > index);
        return m_selectedHydroUnit->landCovers[index];
    }

    SurfaceComponentSettings GetSurfaceComponentSettings(int index) const {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->surfaceComponents.size() > index);
        return m_selectedHydroUnit->surfaceComponents[index];
    }

    int GetHydroUnitsNb() const {
        return int(m_hydroUnits.size());
    }

    int GetLandCoversNb() const {
        wxASSERT(m_selectedHydroUnit);
        return int(m_selectedHydroUnit->landCovers.size());
    }

    int GetSurfaceComponentsNb() const {
        wxASSERT(m_selectedHydroUnit);
        return int(m_selectedHydroUnit->surfaceComponents.size());
    }

    double GetTotalArea() const;

  protected:
  private:
    vector<HydroUnitSettings> m_hydroUnits;
    HydroUnitSettings* m_selectedHydroUnit;
};

#endif  // HYDROBRICKS_SETTING_BASIN_H
