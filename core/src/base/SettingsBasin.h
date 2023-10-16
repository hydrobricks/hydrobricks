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

struct HydroUnitPropertyDouble {
    string name;
    double value;
    string unit;
};

struct HydroUnitPropertyString {
    string name;
    string value;
};

struct HydroUnitSettings {
    int id;
    double area;
    vector<LandCoverSettings> landCovers;
    vector<SurfaceComponentSettings> surfaceComponents;
    vector<HydroUnitPropertyDouble> propertiesDouble;
    vector<HydroUnitPropertyString> propertiesString;
};

class SettingsBasin : public wxObject {
  public:
    explicit SettingsBasin();

    ~SettingsBasin() override;

    void AddHydroUnit(int id, double area);

    void AddLandCover(const string& name, const string& type = "", double fraction = 1.0);

    void AddHydroUnitPropertyDouble(const string& name, double value, const string& unit = "");

    void AddHydroUnitPropertyString(const string& name, const string& value);

    void Clear();

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
