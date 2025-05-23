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

    /**
     * Add a new hydro unit.
     *
     * @param id ID of the hydro unit.
     * @param area area of the hydro unit.
     */
    void AddHydroUnit(int id, double area);

    /**
     * Add a new land cover to the selected hydro unit.
     *
     * @param name name of the land cover.
     * @param type type of the land cover.
     * @param fraction fraction of the land cover.
     */
    void AddLandCover(const string& name, const string& type = "", double fraction = 1.0);

    /**
     * Add a numeric property to the selected hydro unit.
     *
     * @param name name of the property.
     * @param value value of the property.
     * @param unit unit of the property.
     */
    void AddHydroUnitPropertyDouble(const string& name, double value, const string& unit = "");

    /**
     * Add a string property to the selected hydro unit.
     *
     * @param name name of the property.
     * @param value value of the property.
     */
    void AddHydroUnitPropertyString(const string& name, const string& value);

    /**
     * Clear all hydro units.
     */
    void Clear();

    /**
     * Select a hydro unit.
     *
     * @param index index of the hydro unit to select.
     */
    void SelectUnit(int index);

    /**
     * Parse a NetCDF file to get the hydro unit settings.
     */
    bool Parse(const string& path);

    /**
     * Get hydro unit settings.
     *
     * @param index index of the hydro unit.
     * @return pointer to the selected hydro unit.
     */
    HydroUnitSettings GetHydroUnitSettings(int index) const {
        wxASSERT(m_hydroUnits.size() > index);
        return m_hydroUnits[index];
    }

    /**
     * Get land cover settings for the selected hydro unit.
     *
     * @param index index of the land cover.
     * @return pointer to the selected land cover.
     */
    LandCoverSettings GetLandCoverSettings(int index) const {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->landCovers.size() > index);
        return m_selectedHydroUnit->landCovers[index];
    }

    /**
     * Get surface component settings for the selected hydro unit.
     *
     * @param index index of the surface component.
     * @return pointer to the selected surface component.
     */
    SurfaceComponentSettings GetSurfaceComponentSettings(int index) const {
        wxASSERT(m_selectedHydroUnit);
        wxASSERT(m_selectedHydroUnit->surfaceComponents.size() > index);
        return m_selectedHydroUnit->surfaceComponents[index];
    }

    /**
     * Get the number of hydro units.
     *
     * @return number of hydro units.
     */
    int GetHydroUnitsNb() const {
        return static_cast<int>(m_hydroUnits.size());
    }

    /**
     * Get the number of land covers for the selected hydro unit.
     *
     * @return number of land covers.
     */
    int GetLandCoversNb() const {
        wxASSERT(m_selectedHydroUnit);
        return static_cast<int>(m_selectedHydroUnit->landCovers.size());
    }

    /**
     * Get the number of surface components for the selected hydro unit.
     *
     * @return number of surface components.
     */
    int GetSurfaceComponentsNb() const {
        wxASSERT(m_selectedHydroUnit);
        return static_cast<int>(m_selectedHydroUnit->surfaceComponents.size());
    }

    /**
     * Get the total area of the sub basin (all hydro units).
     *
     * @return total area of the sub basin.
     */
    double GetTotalArea() const;

  private:
    vector<HydroUnitSettings> m_hydroUnits;
    HydroUnitSettings* m_selectedHydroUnit;
};

#endif  // HYDROBRICKS_SETTING_BASIN_H
