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
        _changeRate = rate;
    }

    /**
     * Get the change rate value pointer.
     *
     * @return the change rate value pointer.
     */
    double* GetChangeRatePointer() {
        return _changeRate;
    }

    /**
     * Get the amount of water outgoing the flux.
     *
     * @return the amount of water outgoing the flux
     */
    double* GetAmountPointer() {
        return &_amount;
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
        _static = true;
    }

    /**
     * Check if the flux is static.
     *
     * @return true if the flux is static.
     */
    bool IsStatic() {
        return _static;
    }

    /**
     * Check if the flux needs weighting.
     *
     * @return true if the flux needs weighting.
     */
    bool NeedsWeighting() {
        return _needsWeighting;
    }

    /**
     * Set the flux as needing weighting.
     *
     * @param value true if the flux needs weighting.
     */
    void NeedsWeighting(bool value) {
        _needsWeighting = value;
    }

    /**
     * Set a static weight to the flux. This can be a fraction of the unit area (not land cover) or a fraction of
     * the redistributed snow, for example.
     *
     * @param value the weight of the flux.
     */
    void SetWeight(double value) {
        _weight = value;
        UpdateFractionTotal();
    }

    /**
     * Set the fraction of the land cover.
     *
     * @param value the fraction of the land cover.
     */
    void SetFractionLandCover(double value) {
        _fractionLandCover = value;
        UpdateFractionTotal();
    }

    /**
     * Update the fraction total.
     */
    void UpdateFractionTotal() {
        _fractionTotal = _weight * _fractionLandCover;
    }

    /**
     * Get the flux type.
     *
     * @return the flux type.
     */
    string GetType() {
        return _type;
    }

    /**
     * Set the flux type.
     *
     * @param type the flux type.
     */
    void SetType(const string& type) {
        _type = type;
    }

  protected:
    double _amount;
    double* _changeRate;
    bool _static;
    bool _needsWeighting;
    double _weight;
    double _fractionLandCover;
    double _fractionTotal;
    Modifier* _modifier;
    string _type;
};

#endif  // HYDROBRICKS_FLUX_H
