#ifndef HYDROBRICKS_BRICK_H
#define HYDROBRICKS_BRICK_H

#include "Flux.h"
#include "Includes.h"
#include "Process.h"
#include "SettingsModel.h"

class HydroUnit;

class Brick : public wxObject {
  public:
    explicit Brick(HydroUnit* hydroUnit);

    ~Brick() override = default;

    static Brick* Factory(const BrickSettings &brickSettings, HydroUnit* unit);

    static bool HasParameter(const BrickSettings &brickSettings, const wxString &name);

    static float* GetParameterValuePointer(const BrickSettings &brickSettings, const wxString &name);

    /**
     * Assign the parameters to the brick element.
     *
     * @param brickSettings settings of the brick containing the parameters.
     */
    virtual void AssignParameters(const BrickSettings &brickSettings);

    /**
     * Attach incoming flux.
     *
     * @param flux incoming flux
     */
    void AttachFluxIn(Flux* flux) {
        wxASSERT(flux);
        m_inputs.push_back(flux);
    }

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk() = 0;

    /**
     * Define if the brick needs to be handled by the solver.
     *
     * @return true if the brick needs to be handled by the solver.
     */
    bool NeedsSolver() {
        return m_needsSolver;
    }

    bool Compute();

    double GetOutputsSum(vecDouble &qOuts);

    /**
     * Finalize the computing and copy the "next" values to "previous".
     */
    void Finalize();

    /**
     * Get the water content of the current object.
     *
     * @return water content [mm]
     */
    double GetContent() const {
        return m_content;
    }

    /**
     * Set the water content of the current object.
     *
     * @param timeStepFraction fraction of the time step
     */
    void SetStateVariablesFor(float timeStepFraction);

    Process* GetProcess(int index);

    wxString GetName() {
        return m_name;
    }

    void SetName(const wxString &name) {
        m_name = name;
    }

    void SetCapacity(float* capacity) {
        m_capacity = capacity;
    }

    double* GetTimeStepPointer() {
        return m_timeStepInDays;
    }

    void SetTimeStepPointer(double* value) {
        m_timeStepInDays = value;
    }

    bool HasMaximumCapacity() const {
        return m_capacity != nullptr;
    }

    bool IsFull() const {
        return m_content >= *m_capacity;
    }

    /**
     * Get pointers to the values that need to be iterated.
     *
     * @return vector of pointers to the values that need to be iterated.
     */
    virtual vecDoublePt GetIterableValues();

    vecDoublePt GetIterableValuesFromProcesses();

    double* GetBaseValuePointer(const wxString& name);

    virtual double* GetValuePointer(const wxString& name);

  protected:
    wxString m_name;
    bool m_needsSolver;
    double m_content; // [mm]
    double m_contentPrev; // [mm]
    double m_contentChangeRate; // [mm/d]
    float* m_capacity;
    double* m_timeStepInDays; // [d]
    HydroUnit* m_hydroUnit;
    std::vector<Flux*> m_inputs;
    std::vector<Process*> m_processes;

    /**
     * Sums the water amount from the different fluxes.
     *
     * @return sum of the water amount [mm]
     */
    double SumIncomingFluxes();

    /**
     * Compute outputs of the element.
     *
     * @param timeStepFraction fraction of the time step
     * @return output values.
     */
    virtual vecDouble ComputeOutputs() = 0;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
