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
     * Define if the brick needs to be handled by the solver.
     *
     * @return true if the brick needs to be handled by the solver.
     */
    bool NeedsSolver() {
        return m_needsSolver;
    }

    bool Compute();

    double getOutputsSum(std::vector<double> &qOuts);

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
    void SetContentFor(float timeStepFraction);

    void SetCapacity(double capacity) {
        m_capacity = capacity;
    }

    bool HasMaximumCapacity() const {
        return m_capacity != UNDEFINED;
    }

    bool IsFull() const {
        return m_content >= m_capacity;
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
    bool m_needsSolver;
    double m_content; // [mm]
    double m_contentPrev; // [mm]
    double m_contentChangeRate; // [mm/T]
    double m_capacity;
    HydroUnit* m_hydroUnit;
    std::vector<Flux*> m_inputs;
    std::vector<Flux*> m_outputs;
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
    virtual std::vector<double> ComputeOutputs() = 0;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
