#ifndef HYDROBRICKS_FLUX_H
#define HYDROBRICKS_FLUX_H

#include "Includes.h"

class Brick;
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
     * Get the water amount of the flux.
     *
     * @return the water amount of the flux.
     */
    virtual double GetAmount() = 0;

    /**
     * Set the water amount of the flux.
     *
     * @param amount the water amount of the flux.
     */
    virtual void SetAmount(double amount) {
        m_amount = amount;
    }

  protected:
    Brick* m_in;
    Brick* m_out;
    double m_amount;
    double m_amountIteration[4];
    Modifier* m_modifier;

  private:
};

#endif  // HYDROBRICKS_FLUX_H
