#ifndef HYDROBRICKS_PROCESS_H
#define HYDROBRICKS_PROCESS_H

#include "Includes.h"
#include "SettingsModel.h"
#include "Forcing.h"
#include "Flux.h"

class Brick;

class Process : public wxObject {
  public:
    explicit Process(Brick* brick);

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

    virtual int GetConnectionsNb() = 0;

    virtual vecDouble GetChangeRates() = 0;

    void ApplyChange(int connectionIndex, double rate, double timeStepInDays);

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetStateVariables() {
        return vecDoublePt {};
    }

    virtual double* GetValuePointer(const wxString& name);

    wxString GetName() {
        return m_name;
    }

    void SetName(const wxString &name) {
        m_name = name;
    }

  protected:
    wxString m_name;
    Brick* m_brick;
    std::vector<Flux*> m_outputs;

  private:
};

#endif  // HYDROBRICKS_PROCESS_H
