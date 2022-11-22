#ifndef HYDROBRICKS_SPLITTER_H
#define HYDROBRICKS_SPLITTER_H

#include "Flux.h"
#include "Forcing.h"
#include "Includes.h"
#include "SettingsModel.h"

class HydroUnit;

class Splitter : public wxObject {
  public:
    explicit Splitter();

    static Splitter* Factory(const SplitterSettings& splitterSettings);

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk() = 0;

    /**
     * Assign the parameters to the splitter.
     *
     * @param splitterSettings settings of the splitter containing the parameters.
     */
    virtual void AssignParameters(const SplitterSettings& splitterSettings) = 0;

    float* GetParameterValuePointer(const SplitterSettings& splitterSettings, const string& name);

    virtual void AttachForcing(Forcing*) {
        throw ShouldNotHappen();
    }

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
     * Attach outgoing flux.
     *
     * @param flux outgoing flux
     */
    void AttachFluxOut(Flux* flux) {
        wxASSERT(flux);
        m_outputs.push_back(flux);
    }

    virtual double* GetValuePointer(const string& name) = 0;

    virtual void Compute() = 0;

    string GetName() {
        return m_name;
    }

    void SetName(const string& name) {
        m_name = name;
    }

  protected:
    string m_name;
    std::vector<Flux*> m_inputs;
    std::vector<Flux*> m_outputs;

  private:
};

#endif  // HYDROBRICKS_SPLITTER_H
