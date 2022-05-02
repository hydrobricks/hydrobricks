#ifndef HYDROBRICKS_FLUX_H
#define HYDROBRICKS_FLUX_H

#include "Includes.h"

class Modifier;

class Flux : public wxObject {
  public:
    explicit Flux();

    /**
     * Check that everything is correctly defined.
     *
     * @return true is everything is correctly defined.
     */
    virtual bool IsOk() = 0;

    /**
     * Get the amount of water outgoing the flux.
     *
     * @return the amount of water outgoing the flux
     */
    virtual double GetAmount() = 0;

    /**
     * Set the water amount of the flux.
     *
     * @param amount the water amount of the flux.
     */
    void SetAmount(double amount) {
        m_amount = amount;
    }

    double* GetAmountPointer() {
        return &m_amount;
    }

  protected:
    bool m_isConstant;
    double m_amount;
    Modifier* m_modifier;

  private:
};

#endif  // HYDROBRICKS_FLUX_H
