#ifndef HYDROBRICKS_BRICK_H
#define HYDROBRICKS_BRICK_H

#include "Flux.h"
#include "Includes.h"
#include "Process.h"
#include "SettingsModel.h"
#include "WaterContainer.h"

class Brick : public wxObject {
  public:
    explicit Brick();

    ~Brick() override;

    /**
     * Factory method to create a brick.
     *
     * @param brickSettings settings of the brick.
     * @return the created brick.
     */
    static Brick* Factory(const BrickSettings& brickSettings);

    /**
     * Check if the brick has a parameter with the provided name.
     *
     * @param brickSettings settings of the brick containing the parameters.
     * @param name name of the parameter to check.
     * @return true if the brick has a parameter with the provided name.
     */
    static bool HasParameter(const BrickSettings& brickSettings, const string& name);

    /**
     * Get the pointer to the parameter value.
     *
     * @param brickSettings settings of the brick containing the parameters.
     * @param name name of the parameter.
     * @return pointer to the parameter value.
     */
    static float* GetParameterValuePointer(const BrickSettings& brickSettings, const string& name);

    /**
     * Assign the parameters to the brick element.
     *
     * @param brickSettings settings of the brick containing the parameters.
     */
    virtual void SetParameters(const BrickSettings& brickSettings);

    /**
     * Attach incoming flux.
     *
     * @param flux incoming flux
     */
    virtual void AttachFluxIn(Flux* flux);

    /**
     * Add a process to the brick.
     *
     * @param process process to add.
     */
    void AddProcess(Process* process) {
        wxASSERT(process);
        _processes.push_back(process);
    }

    /**
     * Reset the brick to its initial state.
     */
    virtual void Reset();

    /**
     * Save the current state of the brick as the initial state.
     */
    virtual void SaveAsInitialState();

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk();

    /**
     * Define if the brick needs to be handled by the solver.
     *
     * @return true if the brick needs to be handled by the solver.
     */
    bool NeedsSolver() const {
        return _needsSolver;
    }

    /**
     * Check if the brick can have an area fraction.
     *
     * @return true if the brick can have an area fraction.
     */
    virtual bool CanHaveAreaFraction() {
        return false;
    }

    /**
     * Check if the brick is a snowpack.
     *
     * @return true if the brick is a snowpack.
     */
    virtual bool IsSnowpack() {
        return false;
    }

    /**
     * Check if the brick is a glacier.
     *
     * @return true if the brick is a glacier.
     */
    virtual bool IsGlacier() {
        return false;
    }

    /**
     * Check if the brick is a land cover.
     *
     * @return true if the brick is a land cover.
     */
    virtual bool IsLandCover() {
        return false;
    }

    /**
     * Check if the brick is null (e.g., if the area fraction is null).
     *
     * @return true if the brick is null.
     */
    virtual bool IsNull() {
        return false;
    }

    /**
     * Finalize the water transfer.
     */
    virtual void Finalize();

    /**
     * Set the initial state of the water container.
     *
     * @param value initial state value.
     * @param type type of the content (e.g., "water", "ice", or "snow").
     */
    virtual void SetInitialState(double value, const string& type = "water");

    /**
     * Get the content of the water container.
     *
     * @param type type of the content (e.g., "water", "ice", or "snow").
     * @return content of the water container.
     */
    virtual double GetContent(const string& type = "water");

    /**
     * Update the content of the water container.
     *
     * @param value new content value.
     * @param type type of the content (e.g., "water", "ice", or "snow").
     */
    virtual void UpdateContent(double value, const string& type = "water");

    /**
     * Update the content of the water container from the inputs.
     */
    virtual void UpdateContentFromInputs();

    /**
     * Apply the constraints to the water container.
     *
     * @param timeStep time step for the simulation.
     */
    virtual void ApplyConstraints(double timeStep);

    /**
     * Get the water container.
     *
     * @return pointer to the water container.
     */
    WaterContainer* GetWaterContainer();

    /**
     * Get a process by its index.
     *
     * @param index index of the process.
     * @return pointer to the process.
     */
    Process* GetProcess(int index);

    /**
     * Get all processes of the brick.
     *
     * @return vector of pointers to the processes.
     */
    vector<Process*>& GetProcesses() {
        return _processes;
    }

    /**
     * Get the name of the brick.
     *
     * @return name of the brick.
     */
    string GetName() {
        return _name;
    }

    /**
     * Set the name of the brick.
     *
     * @param name new name of the brick.
     */
    void SetName(const string& name) {
        _name = name;
    }

    /**
     * Get the hydro unit associated with the brick.
     *
     * @return pointer to the hydro unit.
     */
    HydroUnit* GetHydroUnit() {
        wxASSERT(_hydroUnit);
        return _hydroUnit;
    }

    /**
     * Set the hydro unit associated with the brick.
     *
     * @param hydroUnit pointer to the hydro unit.
     */
    void SetHydroUnit(HydroUnit* hydroUnit) {
        wxASSERT(hydroUnit);
        _hydroUnit = hydroUnit;
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetDynamicContentChanges();

    /**
     * Get the changes in state variables from processes.
     *
     * @return vector of pointers to the changes in state variables.
     */
    vecDoublePt GetStateVariableChangesFromProcesses();

    /**
     * Get the number of connections of the processes.
     *
     * @return number of connections of the processes.
     */
    int GetProcessesConnectionsNb();

    /**
     * Get the pointer to the water container content.
     *
     * @param name name of the container type (e.g., "water", "ice", or "snow").
     */
    double* GetBaseValuePointer(const string& name);

    /**
     * Get the pointer to the water container content.
     *
     * @param name name of the container type (e.g., "water", "ice", or "snow").
     */
    virtual double* GetValuePointer(const string& name);

  protected:
    string _name;
    bool _needsSolver;
    WaterContainer* _water;
    vector<Process*> _processes;
    HydroUnit* _hydroUnit;
};

#endif  // HYDROBRICKS_BRICK_H
