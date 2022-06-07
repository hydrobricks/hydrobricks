#ifndef HYDROBRICKS_WATER_CONTAINER_H
#define HYDROBRICKS_WATER_CONTAINER_H

#include "Includes.h"
#include "Process.h"

class Brick;

class WaterContainer : public wxObject {
  public:
    WaterContainer(Brick* brick);

    void SubtractAmount(double change);

    void AddAmount(double change);

    void ApplyConstraints(double timeStep);

    void SetOutgoingRatesToZero();

    void Finalize();

    vecDoublePt GetStateVariableChanges();

    bool HasMaximumCapacity() const {
        return m_capacity != nullptr;
    }

    double GetMaximumCapacity() {
        return *m_capacity;
    }

    void SetMaximumCapacity(float* value) {
        m_capacity = value;
    }

    /**
     * Get the water content of the current object.
     *
     * @return water content [mm]
     */
    double GetContentWithChanges() const {
        return m_content + m_contentChange;
    }

    double* GetContentPointer() {
        return &m_content;
    }

    void UpdateContent(double value) {
        m_content = value;
    }

    bool IsNotEmpty() {
        return GetContentWithChanges() > 0.0;
    }

    bool HasOverflow() {
        return m_overflow != nullptr;
    }

    void LinkOverflow(Process* overflow) {
        m_overflow = overflow;
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
     * Sums the water amount from the different fluxes.
     *
     * @return sum of the water amount [mm]
     */
    virtual double SumIncomingFluxes();

  protected:

  private:
    double m_content; // [mm]
    double m_contentChange; // [mm]
    float* m_capacity;
    Brick* m_parent;
    Process* m_overflow;
    std::vector<Flux*> m_inputs;
};

#endif  // HYDROBRICKS_WATER_CONTAINER_H
