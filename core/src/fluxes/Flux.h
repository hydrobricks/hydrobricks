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
    virtual void UpdateFlux(double amount);

    void LinkChangeRate(double* rate) {
        m_changeRate = rate;
    }

    double* GetChangeRatePointer() {
        wxASSERT(m_changeRate);
        return m_changeRate;
    }

    double* GetAmountPointer() {
        return &m_amount;
    }

    virtual bool IsForcing() {
        return false;
    }

    void SetAsStatic() {
        m_static = true;
    }

    bool IsStatic() {
        return m_static;
    }

    bool NeedsWeighting() {
        return m_needsWeighting;
    }

    void NeedsWeighting(bool value) {
        m_needsWeighting = value;
    }

    void MultiplyFraction(double value) {
        m_fraction *= value;
    }

    std::string GetType() {
        return m_type;
    }

    void SetType(const std::string& type) {
        m_type = type;
    }

  protected:
    double m_amount;
    double* m_changeRate;
    bool m_static;
    bool m_needsWeighting;
    double m_fraction;
    Modifier* m_modifier;
    std::string m_type;

  private:
};

#endif  // HYDROBRICKS_FLUX_H
