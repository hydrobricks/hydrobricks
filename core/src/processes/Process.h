#ifndef HYDROBRICKS_PROCESS_H
#define HYDROBRICKS_PROCESS_H

#include "Flux.h"
#include "Forcing.h"
#include "Includes.h"
#include "SettingsModel.h"

class Brick;
class HydroUnit;
class WaterContainer;

class Process : public wxObject {
  public:
    explicit Process(WaterContainer* container);

    ~Process() override = default;

    /**
     * Factory method to create a process.
     *
     * @param processSettings settings of the process.
     * @param brick the related brick.
     * @return the created process.
     */
    static Process* Factory(const ProcessSettings& processSettings, Brick* brick);

    /**
     * Register the parameters and the needed forcing for the process.
     *
     * @param modelSettings settings of the model.
     * @param processType type of process.
     * @return true if everything is correctly defined.
     */
    static bool RegisterParametersAndForcing(SettingsModel* modelSettings, const string& processType);

    /**
     * Reset all the fluxes connected to the process.
     */
    void Reset();

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk() = 0;

    /**
     * Check if the process has a parameter with the provided name.
     *
     * @param processSettings settings of the process containing the parameters.
     * @param name name of the parameter to check.
     * @return true if the process has a parameter with the provided name.
     */
    static bool HasParameter(const ProcessSettings& processSettings, const string& name);

    /**
     * Get the value pointer of a parameter.
     *
     * @param processSettings settings of the process containing the parameters.
     * @param name name of the parameter to get.
     * @return pointer to the value of the parameter.
     */
    static float* GetParameterValuePointer(const ProcessSettings& processSettings, const string& name);

    /**
     * Set the properties of the hydro unit.
     *
     * @param unit the related hydro unit.
     * @param brick the related brick.
     */
    virtual void SetHydroUnitProperties(HydroUnit* unit, Brick* brick);

    /**
     * Assign the parameters to the process.
     *
     * @param processSettings settings of the process containing the parameters.
     */
    virtual void SetParameters(const ProcessSettings& processSettings);

    /**
     * Attach forcing to the process.
     *
     * @param forcing forcing to attach.
     */
    virtual void AttachForcing(Forcing*) {
        throw ShouldNotHappen();
    }

    /**
     * Attach outgoing flux.
     *
     * @param flux outgoing flux
     */
    void AttachFluxOut(Flux* flux) {
        wxASSERT(flux);
        _outputs.push_back(flux);
    }

    /**
     * Get the outgoing fluxes.
     *
     * @return vector of pointers to the outgoing fluxes.
     */
    vector<Flux*> GetOutputFluxes() {
        return _outputs;
    }

    /**
     * Get the number of outgoing fluxes.
     *
     * @return number of outgoing fluxes.
     */
    int GetOutputFluxesNb() {
        return static_cast<int>(_outputs.size());
    }

    /**
     * Check if the process sends water to the atmosphere.
     *
     * @return true if the process sends water to the atmosphere.
     */
    virtual bool ToAtmosphere() {
        return false;
    }

    /**
     * Check if the process needs to link the target brick.
     *
     * @return true if the process needs to link the target brick.
     */
    virtual bool NeedsTargetBrickLinking() {
        return false;
    }

    /**
     * Get the number of connections to the process.
     *
     * @return number of connections to the process.
     */
    virtual int GetConnectionsNb() = 0;

    /**
     * Get the change rates of the process.
     *
     * @return vector of change rates.
     */
    virtual vecDouble GetChangeRates();

    /**
     * Store the water corresponding to the change rates in the outgoing fluxes.
     *
     * @param rate change rates.
     * @param index index of the flux.
     */
    virtual void StoreInOutgoingFlux(double* rate, int index);

    /**
     * Apply the change rates to the process.
     *
     * @param connectionIndex index of the connection.
     * @param rate change rates.
     * @param timeStepInDays time step in days.
     */
    void ApplyChange(int connectionIndex, double rate, double timeStepInDays);

    /**
     * Finalize the process.
     */
    virtual void Finalize() {
        // Nothing to do here.
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetStateVariables() {
        return vecDoublePt{};
    }

    /**
     * Get the value pointer for a given element.
     *
     * @param name name of the element to get.
     * @return pointer to the value of the given element.
     */
    virtual double* GetValuePointer(const string& name);

    /**
     * Get the name of the process.
     *
     * @return name of the process.
     */
    string GetName() {
        return _name;
    }

    /**
     * Set the name of the process.
     *
     * @param name name of the process.
     */
    void SetName(const string& name) {
        _name = name;
    }

    /**
     * Get the water container associated with the process.
     *
     * @return pointer to the water container.
     */
    WaterContainer* GetWaterContainer() {
        return _container;
    }

    /**
     * Set the target brick for the process.
     *
     * @param brick target brick.
     */
    virtual void SetTargetBrick(Brick*) {
        throw ShouldNotHappen();
    }

  protected:
    string _name;
    WaterContainer* _container;
    vector<Flux*> _outputs;

    /**
     * Get the sum of change rates from other processes.
     *
     * @return sum of change rates from other processes.
     */
    double GetSumChangeRatesOtherProcesses();

    /**
     * Get the rates of the process.
     *
     * @return vector of rates.
     */
    virtual vecDouble GetRates() = 0;
};

#endif  // HYDROBRICKS_PROCESS_H
