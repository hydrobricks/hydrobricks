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
     * Reset the flux to its initial state.
     */
    virtual void Reset();

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

    /**
     * Link the flux to a change rate value pointer.
     *
     * @param rate the change rate value pointer.
     */
    void LinkChangeRate(double* rate) {
        m_changeRate = rate;
    }

    /**
     * Get the change rate value pointer.
     *
     * @return the change rate value pointer.
     */
    double* GetChangeRatePointer() {
        return m_changeRate;
    }

    /**
     * Get the amount of water outgoing the flux.
     *
     * @return the amount of water outgoing the flux
     */
    double* GetAmountPointer() {
        return &m_amount;
    }

    /**
     * Check if the flux is a forcing.
     *
     * @return true if the flux is a forcing.
     */
    virtual bool IsForcing() {
        return false;
    }

    /**
     * Check if the flux is instantaneous.
     *
     * @return true if the flux is instantaneous.
     */
    virtual bool IsInstantaneous() {
        return false;
    }

    /**
     * Set the flux as static.
     */
    void SetAsStatic() {
        m_static = true;
    }

    /**
     * Check if the flux is static.
     *
     * @return true if the flux is static.
     */
    bool IsStatic() {
        return m_static;
    }

    /**
     * Check if the flux needs weighting.
     *
     * @return true if the flux needs weighting.
     */
    bool NeedsWeighting() {
        return m_needsWeighting;
    }

    /**
     * Set the flux as needing weighting.
     *
     * @param value true if the flux needs weighting.
     */
    void NeedsWeighting(bool value) {
        m_needsWeighting = value;
    }

    /**
     * Get the fraction of the unit area.
     *
     * @return the fraction of the unit area.
     */
    void SetFractionUnitArea(double value) {
        m_fractionUnitArea = value;
        UpdateFractionTotal();
    }

    /**
     * Set the fraction of the land cover.
     *
     * @param value the fraction of the land cover.
     */
    void SetFractionLandCover(double value) {
        m_fractionLandCover = value;
        UpdateFractionTotal();
    }

    /**
     * Update the fraction total.
     */
    void UpdateFractionTotal() {
        m_fractionTotal = m_fractionUnitArea * m_fractionLandCover;
    }

    /**
     * Get the flux type.
     *
     * @return the flux type.
     */
    string GetType() {
        return m_type;
    }

    /**
     * Set the flux type.
     *
     * @param type the flux type.
     */
    void SetType(const string& type) {
        m_type = type;
    }

  protected:
    double m_amount;
    double* m_changeRate;
    bool m_static;
    bool m_needsWeighting;
    double m_fractionUnitArea;
    double m_fractionLandCover;
    double m_fractionTotal;
    Modifier* m_modifier;
    string m_type;
};

#endif  // HYDROBRICKS_FLUX_H
