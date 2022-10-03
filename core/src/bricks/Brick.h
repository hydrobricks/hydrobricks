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

    static Brick* Factory(const BrickSettings& brickSettings);

    static bool HasParameter(const BrickSettings& brickSettings, const std::string& name);

    static float* GetParameterValuePointer(const BrickSettings& brickSettings, const std::string& name);

    /**
     * Assign the parameters to the brick element.
     *
     * @param brickSettings settings of the brick containing the parameters.
     */
    virtual void AssignParameters(const BrickSettings& brickSettings);

    /**
     * Attach incoming flux.
     *
     * @param flux incoming flux
     */
    virtual void AttachFluxIn(Flux* flux);

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

    virtual bool IsSurfaceComponent() {
        return false;
    }

    virtual bool IsSnowpack() {
        return false;
    }

    virtual bool IsGlacier() {
        return false;
    }

    virtual bool IsNull() {
        return false;
    }

    virtual void Finalize();

    virtual void UpdateContentFromInputs();

    virtual void ApplyConstraints(double timeStep, bool inSolver = true);

    WaterContainer* GetWaterContainer();

    Process* GetProcess(int index);

    std::vector<Process*>& GetProcesses() {
        return m_processes;
    }

    std::string GetName() {
        return m_name;
    }

    void SetName(const std::string& name) {
        m_name = name;
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetStateVariableChanges();

    vecDoublePt GetStateVariableChangesFromProcesses();

    int GetProcessesConnectionsNb();

    double* GetBaseValuePointer(const std::string& name);

    virtual double* GetValuePointer(const std::string& name);

  protected:
    std::string m_name;
    bool m_needsSolver;
    WaterContainer* m_container;
    std::vector<Process*> m_processes;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
