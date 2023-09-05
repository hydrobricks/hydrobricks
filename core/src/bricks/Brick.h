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

    static bool HasParameter(const BrickSettings& brickSettings, const string& name);

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

    void AddProcess(Process* process) {
        wxASSERT(process);
        m_processes.push_back(process);
    }

    virtual void Reset();

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
        return m_needsSolver;
    }

    virtual bool CanHaveAreaFraction() {
        return false;
    }

    virtual bool IsSnowpack() {
        return false;
    }

    virtual bool IsGlacier() {
        return false;
    }

    virtual bool IsLandCover() {
        return false;
    }

    virtual bool IsNull() {
        return false;
    }

    virtual void Finalize();

    virtual void UpdateContentFromInputs();

    virtual void ApplyConstraints(double timeStep);

    WaterContainer* GetWaterContainer();

    Process* GetProcess(int index);

    vector<Process*>& GetProcesses() {
        return m_processes;
    }

    string GetName() {
        return m_name;
    }

    void SetName(const string& name) {
        m_name = name;
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetDynamicContentChanges();

    vecDoublePt GetStateVariableChangesFromProcesses();

    int GetProcessesConnectionsNb();

    double* GetBaseValuePointer(const string& name);

    virtual double* GetValuePointer(const string& name);

  protected:
    string m_name;
    bool m_needsSolver;
    WaterContainer* m_container;
    vector<Process*> m_processes;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
