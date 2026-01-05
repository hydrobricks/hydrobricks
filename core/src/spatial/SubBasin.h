#ifndef HYDROBRICKS_SUBBASIN_H
#define HYDROBRICKS_SUBBASIN_H

#include "Connector.h"
#include "HydroUnit.h"
#include "Includes.h"
#include "SettingsBasin.h"
#include "TimeMachine.h"

class SubBasin : public wxObject {
  public:
    SubBasin();

    ~SubBasin() override;

    /**
     * Initialize the sub-basin with the given settings.
     *
     * @param basinSettings The settings to initialize the sub-basin with.
     * @return True if the initialization was successful, false otherwise.
     */
    [[nodiscard]] bool Initialize(SettingsBasin& basinSettings);

    /**
     * Build the basin with the given settings.
     *
     * @param basinSettings The settings to build the basin with.
     */
    void BuildBasin(SettingsBasin& basinSettings);

    /**
     * Assign the fractions of the basin.
     *
     * @param basinSettings The settings to assign the fractions with.
     * @return True if the assignment was successful, false otherwise.
     */
    [[nodiscard]] bool AssignFractions(SettingsBasin& basinSettings);

    /**
     * Reset the sub-basin to its initial state.
     */
    void Reset();

    /**
     * Save the current state of the sub-basin as the initial state.
     */
    void SaveAsInitialState();

    /**
     * Check if the sub-basin is valid.
     *
     * @return True if the sub-basin is valid, false otherwise.
     */
    [[nodiscard]] bool IsOk();

    /**
     * Add a brick to the sub-basin.
     *
     * @param brick The brick to add.
     */
    void AddBrick(Brick* brick);

    /**
     * Add a splitter to the sub-basin.
     *
     * @param splitter The splitter to add.
     */
    void AddSplitter(Splitter* splitter);

    /**
     * Check a hydro unit to the sub-basin.
     *
     * @param unit The hydro unit to add.
     */
    void AddHydroUnit(HydroUnit* unit);

    /**
     * Get the number of hydro units in the sub-basin.
     *
     * @return The number of hydro units.
     */
    int GetHydroUnitCount();

    /**
     * Get a hydro unit by its index.
     *
     * @param index The index of the hydro unit to get.
     * @return The hydro unit at the specified index.
     */
    HydroUnit* GetHydroUnit(int index);

    /**
     * Get all hydro units in the sub-basin.
     *
     * @return A vector of pointers to all hydro units.
     */
    vector<HydroUnit*> GetHydroUnits() {
        return _hydroUnits;
    }

    /**
     * Get a hydro unit by its ID.
     *
     * @param id The ID of the hydro unit to get.
     * @return The hydro unit with the specified ID.
     */
    HydroUnit* GetHydroUnitById(int id);

    /**
     * Get the IDs of all hydro units in the sub-basin.
     *
     * @return A vector of hydro unit IDs.
     */
    vecInt GetHydroUnitIds();

    /**
     * Get the areas of all hydro units in the sub-basin.
     *
     * @return A vector of hydro unit areas.
     */
    vecDouble GetHydroUnitAreas();

    /**
     * Get the number of bricks in the sub-basin.
     *
     * @return The number of bricks.
     */
    int GetBricksCount();

    /**
     * Get the number of splitters in the sub-basin.
     *
     * @return The number of splitters.
     */
    int GetSplittersCount();

    /**
     * Get a brick by its index.
     *
     * @param index The index of the brick to get.
     * @return The brick at the specified index.
     */
    Brick* GetBrick(int index);

    /**
     * Check if the sub-basin has a brick with a specific name.
     *
     * @param name The name of the brick to check for.
     * @return True if the sub-basin has the brick, false otherwise.
     */
    [[nodiscard]] bool HasBrick(const string& name);

    /**
     * Get a brick by its name.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name.
     */
    Brick* GetBrick(const string& name);

    /**
     * Get a splitter by its index.
     *
     * @param index The index of the splitter to get.
     * @return The splitter at the specified index.
     */
    Splitter* GetSplitter(int index);

    /**
     * Check if the sub-basin has a splitter with a specific name.
     *
     * @param name The name of the splitter to check for.
     * @return True if the sub-basin has the splitter, false otherwise.
     */
    [[nodiscard]] bool HasSplitter(const string& name);

    /**
     * Get a splitter by its name.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name.
     */
    Splitter* GetSplitter(const string& name);

    /**
     * Check if the sub-basin has an incoming flow.
     *
     * @return True if the sub-basin has an incoming flow, false otherwise.
     */
    [[nodiscard]] bool HasIncomingFlow();

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
    double* GetValuePointer(const string& name);

    /**
     * GCompute the outlet discharge for the sub-basin.
     *
     * @return True if the computation was successful, false otherwise.
     */
    [[nodiscard]] bool ComputeOutletDischarge();

    /**
     * Get the area of the sub-basin.
     *
     * @return The area of the sub-basin in square meters.
     */
    double GetArea() const {
        return _area;
    }

  protected:
    double _area;  // m2
    double _outletTotal;
    bool _needsCleanup;
    vector<Brick*> _bricks;
    vector<Splitter*> _splitters;
    vector<HydroUnit*> _hydroUnits;
    vector<Connector*> _inConnectors;
    vector<Connector*> _outConnectors;
    vector<Flux*> _outletFluxes;
};

#endif
