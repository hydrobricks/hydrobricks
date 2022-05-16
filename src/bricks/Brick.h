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

    void AddProcess(Process* process) {
        wxASSERT(process);
        m_processes.push_back(process);
    }

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
        return m_needsSolver;
    }

    void SubtractAmount(double change);

    void AddAmount(double change);

    void Finalize();

    void UpdateContentFromInputs();

    void ApplyConstraints(double timeStep);

    int GetInputsNb() {
        return int(m_inputs.size());
    }

    /**
     * Get the water content of the current object.
     *
     * @return water content [mm]
     */
    double GetContentWithChanges() const {
        return m_content + m_contentChange;
    }

    Process* GetProcess(int index);

    std::vector<Process*> GetProcesses() {
        return m_processes;
    }

    wxString GetName() {
        return m_name;
    }

    void SetName(const wxString &name) {
        m_name = name;
    }

    bool HasMaximumCapacity() const {
        return m_capacity != nullptr;
    }

    double GetMaximumCapacity() {
        return *m_capacity;
    }

    bool HasOverflow() {
        return m_overflow != nullptr;
    }

    void LinkOverflow(Process* overflow) {
        m_overflow = overflow;
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetStateVariableChanges();

    vecDoublePt GetStateVariableChangesFromProcesses();

    int GetProcessesConnectionsNb();

    double* GetBaseValuePointer(const wxString& name);

    virtual double* GetValuePointer(const wxString& name);

  protected:
    wxString m_name;
    bool m_needsSolver;
    float* m_capacity;
    HydroUnit* m_hydroUnit;
    std::vector<Flux*> m_inputs;
    std::vector<Process*> m_processes;
    Process* m_overflow;

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
    double m_content; // [mm]
    double m_contentChange; // [mm]
};

#endif  // HYDROBRICKS_BRICK_H
