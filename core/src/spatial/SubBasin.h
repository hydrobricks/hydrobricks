#ifndef HYDROBRICKS_SUBBASIN_H
#define HYDROBRICKS_SUBBASIN_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "Connector.h"
#include "HydroUnit.h"
#include "HydroUnitProperty.h"
#include "Includes.h"
#include "SettingsBasin.h"
#include "TimeMachine.h"

class Reach;

class SubBasin {
  public:
    SubBasin();

    virtual ~SubBasin();

    /**
     * Initialize the sub-basin with the given settings.
     *
     * @param basinSettings The settings to initialize the sub-basin with.
     * @return True if the initialization was successful, false otherwise.
     */
    [[nodiscard]] ModelResult Initialize(SettingsBasin& basinSettings);

    /**
     * Create a hydro unit from its settings and add it to the sub basin.
     *
     * @param unitSettings The settings of the hydro unit.
     * @return the created hydro unit (owned by the sub basin).
     */
    HydroUnit* AddHydroUnitFromSettings(HydroUnitSettings& unitSettings);

    /**
     * Set the network properties of the sub basin (ID, downstream link, name, properties).
     *
     * @param subbasinSettings The settings of the subbasin.
     */
    void SetNetworkProperties(const SubbasinSettings& subbasinSettings);

    void SetId(int id) {
        _id = id;
    }

    [[nodiscard]] int GetId() const {
        return _id;
    }

    void SetDownstreamId(int id) {
        _downstreamId = id;
    }

    /**
     * Get the ID of the downstream sub basin (0: terminal outlet of the network).
     *
     * @return the downstream sub basin ID.
     */
    [[nodiscard]] int GetDownstreamId() const {
        return _downstreamId;
    }

    [[nodiscard]] bool IsTerminal() const {
        return _downstreamId == 0;
    }

    void SetName(const string& name) {
        _name = name;
    }

    [[nodiscard]] const string& GetName() const {
        return _name;
    }

    /**
     * Get the area of the sub basin's own hydro units [m2] (same as GetArea()).
     *
     * @return the local area.
     */
    [[nodiscard]] double GetLocalArea() const {
        return _area;
    }

    void SetDrainedArea(double area) {
        _drainedArea = area;
    }

    /**
     * Get the area drained at the sub basin outlet [m2]: the local area plus the area of every upstream sub basin.
     * Set by the river network; equals the local area for a single sub basin.
     *
     * @return the drained area.
     */
    [[nodiscard]] double GetDrainedArea() const {
        return _drainedArea;
    }

    /**
     * Declare whether other sub basins drain into this one (set by the river network). With an upstream
     * inflow, the outlet discharge is expressed over the drained area instead of the local one.
     */
    void SetHasUpstream(bool value) {
        _hasUpstream = value;
    }

    [[nodiscard]] bool HasUpstream() const {
        return _hasUpstream;
    }

    void SetReach(Reach* reach) {
        _reach = reach;
    }

    [[nodiscard]] Reach* GetReach() const {
        return _reach;
    }

    /**
     * Set the volume routed from the upstream sub basins for the current time step [m3].
     *
     * @param volume The routed inflow volume.
     */
    void SetInflowVolume(double volume) {
        _inflowVolume = volume;
    }

    [[nodiscard]] double GetInflowVolume() const {
        return _inflowVolume;
    }

    /**
     * Get the sub basin's own runoff of the current time step [mm over the local area].
     *
     * @return the local outlet total.
     */
    [[nodiscard]] double GetLocalOutlet() const {
        return _outletTotal;
    }

    /**
     * Get the volume leaving the sub basin outlet in the current time step [m3]: the local runoff plus the
     * routed upstream inflow.
     *
     * @return the outlet volume.
     */
    [[nodiscard]] double GetOutletVolume() const {
        return _outletVolume;
    }

    /**
     * Get the outlet discharge of the current time step [mm over the drained area]. Equals the local outlet
     * total for a sub basin without upstream inflow.
     *
     * @return the outlet discharge.
     */
    [[nodiscard]] double GetOutletDischarge() const {
        return _outletDischarge;
    }

    /**
     * Check if a hydro unit with the given ID belongs to this sub basin.
     *
     * @param id The hydro unit ID.
     * @return true if the unit is in this sub basin.
     */
    [[nodiscard]] bool HasHydroUnit(int id) const {
        return _hydroUnitMap.contains(id);
    }

    /**
     * Check if the sub basin has a property with the given name.
     *
     * @param name The property name.
     * @return true if the property exists.
     */
    [[nodiscard]] bool HasProperty(std::string_view name) const;

    /**
     * Get a numeric property of the sub basin (e.g. the reach length).
     *
     * @param name The property name.
     * @param unit The unit to convert the value to (optional).
     * @return the property value.
     */
    [[nodiscard]] double GetPropertyDouble(std::string_view name, std::string_view unit = "") const;

    /**
     * Get a string property of the sub basin.
     *
     * @param name The property name.
     * @return the property value.
     */
    [[nodiscard]] string GetPropertyString(std::string_view name) const;

    /**
     * Build the basin with the given settings.
     *
     * @param basinSettings The settings to build the basin with.
     */
    void BuildBasin(SettingsBasin& basinSettings);

    /**
     * Reserve space for a number of hydro units in the sub-basin.
     *
     * @param count The number of hydro units to reserve space for.
     */
    void ReserveHydroUnits(size_t count) {
        _hydroUnits.reserve(_hydroUnits.size() + count);
    }

    /**
     * Reserve space for a number of bricks in the sub-basin.
     *
     * @param count The number of bricks to reserve space for.
     */
    void ReserveBricks(size_t count) {
        _bricks.reserve(_bricks.size() + count);
    }

    /**
     * Reserve space for a number of splitters in the sub-basin.
     *
     * @param count The number of splitters to reserve space for.
     */
    void ReserveSplitters(size_t count) {
        _splitters.reserve(_splitters.size() + count);
    }

    /**
     * Reserve space for a number of input connectors in the sub-basin.
     *
     * @param count The number of input connectors to reserve space for.
     */
    void ReserveInputConnectors(size_t count) {
        _inConnectors.reserve(_inConnectors.size() + count);
    }

    /**
     * Reserve space for a number of output connectors in the sub-basin.
     *
     * @param count The number of output connectors to reserve space for.
     */
    void ReserveOutputConnectors(size_t count) {
        _outConnectors.reserve(_outConnectors.size() + count);
    }

    /**
     * Reserve space for a number of outlet fluxes in the sub-basin.
     *
     * @param count The number of outlet fluxes to reserve space for.
     */
    void ReserveOutletFluxes(size_t count) {
        _outletFluxes.reserve(_outletFluxes.size() + count);
    }

    /**
     * Reserve space for a number of lateral connections for hydro units in the sub-basin.
     *
     * @param basinSettings The settings to reserve the lateral connections with.
     */
    void ReserveLateralConnectionsForUnits(SettingsBasin& basinSettings);

    /**
     * Assign the fractions of the basin.
     *
     * @param basinSettings The settings to assign the fractions with.
     * @return True if the assignment was successful, false otherwise.
     */
    [[nodiscard]] ModelResult AssignFractions(SettingsBasin& basinSettings);

    /**
     * Reset the sub-basin to its initial state.
     */
    void Reset();

    /**
     * Save the current state of the sub-basin as the initial state.
     */
    void SaveAsInitialState();

    /**
     * Restore the area fractions of all land covers to their initial extents without
     * touching the stored contents (used after the spin-up phase).
     */
    void RestoreInitialAreaFractions();

    /**
     * Check if the sub basin is correctly defined.
     *
     * @return True if everything is correctly defined, false otherwise.
     */
    [[nodiscard]] bool IsValid(bool checkProcesses = true) const;

    /**
     * Validate that the sub basin is correctly defined.
     * Throws an exception if validation fails.
     *
     * @throws ModelConfigError if validation fails.
     */
    void Validate() const;

    /**
     * Add a brick to the sub basin.
     *
     * @param brick The brick to add (ownership transferred).
     */
    void AddBrick(std::unique_ptr<Brick> brick);

    /**
     * Add a splitter to the sub-basin.
     *
     * @param splitter The splitter to add (ownership transferred).
     */
    void AddSplitter(std::unique_ptr<Splitter> splitter);

    /**
     * Add a hydro unit to the sub-basin.
     *
     * @param unit The hydro unit to add (ownership transferred).
     */
    void AddHydroUnit(std::unique_ptr<HydroUnit> unit);

    /**
     * Get the number of hydro units in the sub-basin.
     *
     * @return The number of hydro units.
     */
    [[nodiscard]] int GetHydroUnitCount() const;

    /**
     * Reset all dynamic forcing overrides in all hydro units.
     */
    void ResetForcingUpdates();

    /**
     * Get a hydro unit by its index.
     *
     * @param index The index of the hydro unit to get.
     * @return The hydro unit at the specified index.
     */
    [[nodiscard]] HydroUnit* GetHydroUnit(size_t index) const;

    /**
     * Check if the sub-basin has any hydro units.
     *
     * @return True if the sub-basin has hydro units, false otherwise.
     */
    [[nodiscard]] bool HasHydroUnits() const noexcept {
        return !_hydroUnits.empty();
    }

    /**
     * Get a hydro unit by its ID.
     *
     * @param id The ID of the hydro unit to get.
     * @return The hydro unit with the specified ID.
     */
    [[nodiscard]] HydroUnit* GetHydroUnitById(int id) const;

    /**
     * Get the IDs of all hydro units in the sub-basin.
     *
     * @return A vector of hydro unit IDs.
     */
    [[nodiscard]] vecInt GetHydroUnitIds() const;

    /**
     * Get the areas of all hydro units in the sub-basin.
     *
     * @return A vector of hydro unit areas.
     */
    [[nodiscard]] vecDouble GetHydroUnitAreas() const;

    /**
     * Get the model-structure ID of each hydro unit in the sub-basin.
     *
     * @return A vector of hydro unit structure IDs (defaults to 1).
     */
    [[nodiscard]] vecInt GetHydroUnitStructureIds() const;

    /**
     * Get the number of bricks in the sub-basin.
     *
     * @return The number of bricks.
     */
    [[nodiscard]] int GetBrickCount() const;

    /**
     * Get the number of splitters in the sub-basin.
     *
     * @return The number of splitters.
     */
    [[nodiscard]] int GetSplitterCount() const;

    /**
     * Get a brick by its index.
     *
     * @param index The index of the brick to get.
     * @return The brick at the specified index.
     */
    [[nodiscard]] Brick* GetBrick(size_t index) const;

    /**
     * Check if the sub-basin has a brick with a specific name.
     *
     * @param name The name of the brick to check for.
     * @return True if the sub-basin has the brick, false otherwise.
     */
    [[nodiscard]] bool HasBrick(std::string_view name) const;

    /**
     * Get a brick by its name.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name.
     */
    [[nodiscard]] Brick* GetBrick(std::string_view name) const;

    /**
     * Get a splitter by its index.
     *
     * @param index The index of the splitter to get.
     * @return The splitter at the specified index.
     */
    [[nodiscard]] Splitter* GetSplitter(size_t index) const;

    /**
     * Check if the sub-basin has a splitter with a specific name.
     *
     * @param name The name of the splitter to check for.
     * @return True if the sub-basin has the splitter, false otherwise.
     */
    [[nodiscard]] bool HasSplitter(std::string_view name) const;

    /**
     * Get a splitter by its name.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name.
     */
    [[nodiscard]] Splitter* GetSplitter(std::string_view name) const;

    /**
     * Check if the sub-basin has an incoming flow.
     *
     * @return True if the sub-basin has an incoming flow, false otherwise.
     */
    [[nodiscard]] bool HasIncomingFlow() const;

    /**
     * Add an input connector to the sub-basin.
     *
     * @param connector The input connector to add.
     */
    void AddInputConnector(Connector* connector);

    /**
     * Add an output connector to the sub-basin.
     *
     * @param connector The output connector to add.
     */
    void AddOutputConnector(Connector* connector);

    /**
     * Attach an outlet flux to the sub-basin.
     *
     * @param pFlux The outlet flux to attach.
     */
    void AttachOutletFlux(Flux* pFlux);

    /**
     * Get the value pointer for a specific variable.
     *
     * @param name The name of the variable to get the pointer for.
     * @return A pointer to the variable's value.
     */
    double* GetValuePointer(std::string_view name);

    /**
     * Compute the outlet discharge for the sub-basin.
     *
     * @return True if the computation was successful, false otherwise.
     */
    [[nodiscard]] bool ComputeOutletDischarge();

    /**
     * Get the area of the sub-basin.
     *
     * @return The area of the sub-basin in square meters.
     */
    [[nodiscard]] double GetArea() const {
        return _area;
    }

  protected:
    int _id = 1;
    int _downstreamId = 0;  // 0: terminal outlet of the network
    string _name;
    double _area;                                                 // m2, own hydro units
    double _drainedArea = 0;                                      // m2, own + upstream (set by the network)
    bool _hasUpstream = false;                                    // other sub basins drain into this one
    Reach* _reach = nullptr;                                      // non-owning: owned by the river network
    double _outletTotal;                                          // mm over the local area, own runoff
    double _inflowVolume = 0;                                     // m3 per step, routed from upstream
    double _outletVolume = 0;                                     // m3 per step, at the outlet
    double _outletDischarge = 0;                                  // mm over the drained area, at the outlet
    std::vector<std::unique_ptr<HydroUnitProperty>> _properties;  // owning
    std::vector<std::unique_ptr<Brick>> _bricks;                  // owning: SubBasin-level bricks
    std::unordered_map<string, Brick*> _brickMap;                 // non-owning views into _bricks
    std::vector<std::unique_ptr<Splitter>> _splitters;            // owning: SubBasin-level splitters
    std::unordered_map<string, Splitter*> _splitterMap;           // non-owning views into _splitters
    std::vector<std::unique_ptr<HydroUnit>> _hydroUnits;          // owning
    std::unordered_map<int, HydroUnit*> _hydroUnitMap;            // non-owning views into _hydroUnits
    std::vector<Connector*> _inConnectors;                        // non-owning: lifetime managed externally
    std::vector<Connector*> _outConnectors;                       // non-owning: lifetime managed externally
    std::vector<Flux*> _outletFluxes;                             // non-owning: lifetime managed by process owners
};

#endif
