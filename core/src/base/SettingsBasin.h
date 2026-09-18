#ifndef HYDROBRICKS_SETTING_BASIN_H
#define HYDROBRICKS_SETTING_BASIN_H

#include "Includes.h"
#include "Parameter.h"

class FileNetcdf;

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
    double elevation;
    int subbasinId = 1;  // subbasin the unit drains to (1 when no network is declared)
    vector<LandCoverSettings> landCovers;
    vector<SurfaceComponentSettings> surfaceComponents;
    vector<HydroUnitPropertyDouble> propertiesDouble;
    vector<HydroUnitPropertyString> propertiesString;
};

struct LateralConnectionSettings {
    string type;
    int giverHydroUnitId;
    int receiverHydroUnitId;
    double fraction;
};

/**
 * A subbasin of the river network: a node of the tree, holding hydro units and
 * draining into its downstream subbasin (0: the terminal outlet).
 */
struct SubbasinSettings {
    int id = 1;
    int downstreamId = 0;  // 0: terminal outlet of the network
    string name;
    vector<HydroUnitPropertyDouble> propertiesDouble;
    vector<HydroUnitPropertyString> propertiesString;
};

class SettingsBasin {
  public:
    explicit SettingsBasin();

    virtual ~SettingsBasin();

    /**
     * Add a new hydro unit.
     *
     * @param id ID of the hydro unit.
     * @param area area of the hydro unit.
     * @param elevation elevation of the hydro unit.
     * @param subbasinId ID of the subbasin the unit drains to (1 when no network is declared).
     */
    void AddHydroUnit(int id, double area, double elevation = -9999, int subbasinId = 1);

    /**
     * Add a subbasin to the river network and select it for the following property additions.
     *
     * @param id ID of the subbasin (> 0).
     * @param downstreamId ID of the downstream subbasin (0: terminal outlet).
     * @param name name of the subbasin (optional, e.g. the gauge name).
     */
    void AddSubbasin(int id, int downstreamId = 0, const string& name = "");

    /**
     * Add a numeric property to the selected subbasin (e.g. the reach length or slope).
     *
     * @param name name of the property.
     * @param value value of the property.
     * @param unit unit of the property.
     */
    void AddSubbasinPropertyDouble(const string& name, double value, const string& unit = "");

    /**
     * Add a string property to the selected subbasin.
     *
     * @param name name of the property.
     * @param value value of the property.
     */
    void AddSubbasinPropertyString(const string& name, const string& value);

    /**
     * Select a subbasin by index.
     *
     * @param index index of the subbasin.
     */
    void SelectSubbasin(int index);

    /**
     * Check the river network: unique subbasin IDs, one terminal outlet, no cycle, every hydro unit assigned
     * to a declared subbasin and every subbasin holding at least one hydro unit. A model without declared
     * subbasins is valid when all its units drain to subbasin 1 (the implicit single subbasin).
     *
     * @return an empty result when valid, else the description of the first problem found.
     */
    [[nodiscard]] ModelResult ValidateNetwork() const;

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
     * Add a lateral connection between two hydro units.
     *
     * @param giverHydroUnitId ID of the hydro unit giving the connection.
     * @param receiverHydroUnitId ID of the hydro unit receiving the connection.
     * @param fraction The fraction of the flow that is transferred.
     * @param type The type of the lateral connection (optional). It is unused in the current implementation, but can be
     * used for future extensions (for example, to differentiate between snow and groundwater).
     */
    void AddLateralConnection(int giverHydroUnitId, int receiverHydroUnitId, double fraction, const string& type = "");

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
     *
     * @param path path to the NetCDF file.
     * @return true if the file was parsed successfully.
     */
    bool Parse(const string& path);

    /**
     * Get hydro unit settings.
     *
     * @param index index of the hydro unit.
     * @return pointer to the selected hydro unit.
     */
    HydroUnitSettings GetHydroUnitSettings(int index) const {
        assert(_hydroUnits.size() > index);
        return _hydroUnits[index];
    }

    /**
     * Get the settings of a subbasin.
     *
     * @param index The index of the subbasin.
     * @return the subbasin settings.
     */
    const SubbasinSettings& GetSubbasinSettings(int index) const {
        assert(_subbasins.size() > index);
        return _subbasins[index];
    }

    /**
     * Get the settings of all the declared subbasins (empty when no network is declared).
     *
     * @return vector of subbasin settings.
     */
    const vector<SubbasinSettings>& GetSubbasins() const {
        return _subbasins;
    }

    /**
     * Get the number of declared subbasins (0 when no network is declared).
     *
     * @return number of subbasins.
     */
    int GetSubbasinCount() const {
        return static_cast<int>(_subbasins.size());
    }

    /**
     * Get land cover settings for the selected hydro unit.
     *
     * @param index index of the land cover.
     * @return pointer to the selected land cover.
     */
    LandCoverSettings GetLandCoverSettings(int index) const {
        assert(_selectedHydroUnit);
        assert(_selectedHydroUnit->landCovers.size() > index);
        return _selectedHydroUnit->landCovers[index];
    }

    /**
     * Get surface component settings for the selected hydro unit.
     *
     * @param index index of the surface component.
     * @return pointer to the selected surface component.
     */
    SurfaceComponentSettings GetSurfaceComponentSettings(int index) const {
        assert(_selectedHydroUnit);
        assert(_selectedHydroUnit->surfaceComponents.size() > index);
        return _selectedHydroUnit->surfaceComponents[index];
    }

    /**
     * Get the lateral connections settings.
     *
     * @return vector of lateral connection settings.
     */
    vector<LateralConnectionSettings> GetLateralConnections() const {
        return _lateralConnections;
    }

    /**
     * Get the number of hydro units.
     *
     * @return number of hydro units.
     */
    int GetHydroUnitCount() const {
        return static_cast<int>(_hydroUnits.size());
    }

    /**
     * Get the number of land covers for the selected hydro unit.
     *
     * @return number of land covers.
     */
    int GetLandCoverCount() const {
        assert(_selectedHydroUnit);
        return static_cast<int>(_selectedHydroUnit->landCovers.size());
    }

    /**
     * Get the number of surface components for the selected hydro unit.
     *
     * @return number of surface components.
     */
    int GetSurfaceComponentCount() const {
        assert(_selectedHydroUnit);
        return static_cast<int>(_selectedHydroUnit->surfaceComponents.size());
    }

    /**
     * Get the number of lateral connections.
     *
     * @return number of lateral connections.
     */
    int GetLateralConnectionCount() const {
        return static_cast<int>(_lateralConnections.size());
    }

    /**
     * Get the total area of the sub basin (all hydro units).
     *
     * @return total area of the sub basin.
     */
    [[nodiscard]] double GetTotalArea() const;

  private:
    /**
     * Parse the subbasins dimension of a hydro units file (IDs, downstream links, names, properties).
     *
     * @param file The open NetCDF file.
     */
    void ParseSubbasins(const FileNetcdf& file);

    vector<HydroUnitSettings> _hydroUnits;
    vector<LateralConnectionSettings> _lateralConnections;
    vector<SubbasinSettings> _subbasins;
    HydroUnitSettings* _selectedHydroUnit;          // non-owning reference
    SubbasinSettings* _selectedSubbasin = nullptr;  // non-owning reference
};

#endif  // HYDROBRICKS_SETTING_BASIN_H
