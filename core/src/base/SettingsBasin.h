#ifndef HYDROBRICKS_SETTING_BASIN_H
#define HYDROBRICKS_SETTING_BASIN_H

#include "Includes.h"
#include "Parameter.h"

struct SurfaceElementSettings {
    wxString name;
    wxString type;
    double fraction;
};

struct HydroUnitSettings {
    int id;
    double area;
    double elevation;
    std::vector<SurfaceElementSettings> surfaceElements;
};

class SettingsBasin : public wxObject {
  public:
    explicit SettingsBasin();

    ~SettingsBasin() override;

    void AddHydroUnit(int id, double area, double elevation = 0.0);

    void AddHydroUnitSurfaceElement(const wxString& name, double fraction = 1.0);

    void SelectUnit(int index);

    bool Parse(const wxString &path);

    HydroUnitSettings GetHydroUnitSettings(int index) {
        wxASSERT(m_hydroUnits.size() > index);
        return m_hydroUnits[index];
    }

    SurfaceElementSettings GetSurfaceElementSettings(int index) {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->surfaceElements.size() > index);
        return m_selectedHydroUnit->surfaceElements[index];
    }

    int GetHydroUnitsNb() {
        return int(m_hydroUnits.size());
    }

    int GetSurfaceElementsNb() {
        wxASSERT(m_selectedHydroUnit);
        return int(m_selectedHydroUnit->surfaceElements.size());
    }

  protected:

  private:
    std::vector<HydroUnitSettings> m_hydroUnits;
    HydroUnitSettings* m_selectedHydroUnit;
};

#endif  // HYDROBRICKS_SETTING_BASIN_H
