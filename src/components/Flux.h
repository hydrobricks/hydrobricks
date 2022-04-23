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
     * Get the amount of water outgoing the flux.
     *
     * @return the amount of water outgoing the flux
     */
    virtual double GetOutgoingAmount() = 0;

    /**
     * Finalize the computing and copy the "next" values to "previous".
     */
    void Finalize();

    /**
     * Get the previous water amount of the flux.
     *
     * @return the previous water amount of the flux.
     */
    double GetAmountPrev() {
        return m_amountPrev;
    }

    /**
     * Set the previous water amount of the flux.
     *
     * @param amount the previous water amount of the flux.
     */
    void SetAmountPrev(double amount) {
        m_amountPrev = amount;
    }

    /**
     * Get the next water amount of the flux.
     *
     * @return the next water amount of the flux.
     */
    double GetAmountNext() {
        return m_amountNext;
    }

    /**
     * Set the next water amount of the flux.
     *
     * @param amount the next water amount of the flux.
     */
    void SetAmountNext(double amount) {
        m_amountNext = amount;
    }

    /**
     * Get pointers to the values that need to be iterated.
     *
     * @return vector of pointers to the values that need to be iterated.
     */
    virtual std::vector<double*> GetIterableValues() {
        return std::vector<double*> {&m_amountNext};
    }

  protected:
    Brick* m_in;
    Brick* m_out;
    double m_amountPrev;
    double m_amountNext;
    Modifier* m_modifier;

  private:
};

#endif  // HYDROBRICKS_FLUX_H
