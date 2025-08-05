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

    /**
     * Factory method to create a splitter.
     *
     * @param splitterSettings settings of the splitter containing the parameters.
     * @return a pointer to the splitter.
     */
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
    virtual void SetParameters(const SplitterSettings& splitterSettings) = 0;

    /**
     * Get the value pointer of a parameter.
     *
     * @param splitterSettings settings of the splitter containing the parameters.
     * @param name name of the parameter.
     * @return pointer to the value of the parameter.
     */
    float* GetParameterValuePointer(const SplitterSettings& splitterSettings, const string& name);

    /**
     * Attach forcing.
     *
     * @param forcing incoming forcing
     */
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
        _inputs.push_back(flux);
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
     * Get the pointer to an output value.
     *
     * @param name name of the output value.
     */
    virtual double* GetValuePointer(const string& name) = 0;

    /**
     * Compute the output value.
     */
    virtual void Compute() = 0;

    /**
     * Get the name of the splitter.
     *
     * @return name of the splitter.
     */
    string GetName() {
        return _name;
    }

    /**
     * Set the name of the splitter.
     *
     * @param name name of the splitter.
     */
    void SetName(const string& name) {
        _name = name;
    }

  protected:
    string _name;
    vector<Flux*> _inputs;
    vector<Flux*> _outputs;
};

#endif  // HYDROBRICKS_SPLITTER_H
