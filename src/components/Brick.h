#ifndef HYDROBRICKS_BRICK_H
#define HYDROBRICKS_BRICK_H

#include "Includes.h"
#include "Flux.h"
#include "Process.h"

class HydroUnit;

class Brick : public wxObject {
  public:
    explicit Brick(HydroUnit* hydroUnit);

    ~Brick() override = default;

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

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk() = 0;

    /**
     * Compute the behaviour of the element.
     *
     * @return true is there is no error.
     */
    virtual bool Compute() = 0;

    /**
     * Finalize the computing and copy the "next" values to "previous".
     */
    void Finalize();

    /**
     * Get the previous water content of the current object.
     *
     * @return water content [mm]
     */
    double GetContentPrev() const {
        return m_contentPrev;
    }

    /**
     * Get the next water content of the current object.
     *
     * @return water content [mm]
     */
    double GetContentNext() const {
        return m_contentNext;
    }

    /**
     * Get pointers to the values that need to be iterated.
     *
     * @return vector of pointers to the values that need to be iterated.
     */
    virtual std::vector<double*> GetIterableValues();

    std::vector<double*> GetIterableValuesFromProcesses();

    std::vector<double*> GetIterableValuesFromOutgoingFluxes();

  protected:
    double m_contentPrev; // [mm]
    double m_contentNext; // [mm]
    HydroUnit* m_hydroUnit;
    std::vector<FluxConnector*> m_inputs;
    std::vector<FluxConnector*> m_outputs;
    std::vector<Process*> m_processes;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
