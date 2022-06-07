#ifndef HYDROBRICKS_PROCESS_H
#define HYDROBRICKS_PROCESS_H

#include "Includes.h"
#include "SettingsModel.h"
#include "Forcing.h"
#include "Flux.h"

class Brick;
class WaterContainer;

class Process : public wxObject {
  public:
    explicit Process(WaterContainer* container);

    ~Process() override = default;

    static Process* Factory(const ProcessSettings &processSettings, Brick* brick);

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk() = 0;

    static bool HasParameter(const ProcessSettings &processSettings, const wxString &name);

    static float* GetParameterValuePointer(const ProcessSettings &processSettings, const wxString &name);

    /**
     * Assign the parameters to the process.
     *
     * @param processSettings settings of the process containing the parameters.
     */
    virtual void AssignParameters(const ProcessSettings &processSettings);

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
        m_outputs.push_back(flux);
    }

    std::vector<Flux*> GetOutputFluxes() {
        return m_outputs;
    }

    virtual bool ToAtmosphere() {
        return false;
    }

    virtual int GetConnectionsNb() = 0;

    virtual vecDouble GetChangeRates() = 0;

    virtual void StoreInOutgoingFlux(double* rate, int index);

    void ApplyChange(int connectionIndex, double rate, double timeStepInDays);

    virtual void Finalize() {
        // Nothing to do here.
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetStateVariables() {
        return vecDoublePt {};
    }

    virtual double* GetValuePointer(const wxString& name);

    void SetOutputFluxesFraction(double value);

    wxString GetName() {
        return m_name;
    }

    void SetName(const wxString &name) {
        m_name = name;
    }

    WaterContainer* GetWaterContainer() {
        return m_container;
    }

  protected:
    wxString m_name;
    WaterContainer* m_container;
    std::vector<Flux*> m_outputs;

  private:
};

#endif  // HYDROBRICKS_PROCESS_H
