#ifndef HYDROBRICKS_BRICK_H
#define HYDROBRICKS_BRICK_H

#include "Includes.h"
#include "Flux.h"

class HydroUnit;

class Brick : public wxObject {
  public:
    explicit Brick(HydroUnit* hydroUnit);

    ~Brick() override = default;

    /**
     * Get the water content of the current object.
     *
     * @return water content [mm]
     */
    double GetWaterContent() {
        return m_waterContent;
    }

    /**
     * Attach incoming flux.
     *
     * @param flux incoming flux
     */
    void AttachFluxIn(Flux* flux) {
        wxASSERT(flux);
        m_Inputs.push_back(flux);
    }

    /**
     * Attach outgoing flux.
     *
     * @param flux outgoing flux
     */
    void AttachFluxOut(Flux* flux) {
        wxASSERT(flux);
        m_Outputs.push_back(flux);
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

  protected:
    double m_waterContent; // [mm]
    HydroUnit* m_hydroUnit;
    std::vector<Flux*> m_Inputs;
    std::vector<Flux*> m_Outputs;

  private:
};

#endif  // HYDROBRICKS_BRICK_H
