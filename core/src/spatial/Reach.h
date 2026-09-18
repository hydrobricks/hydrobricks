#ifndef HYDROBRICKS_REACH_H
#define HYDROBRICKS_REACH_H

#include "Includes.h"

class SubBasin;
struct RoutingSettings;

/**
 * The channel reach of a sub basin: the main channel from the point where the upstream sub basins enter to
 * the sub basin outlet. It routes the volume arriving from upstream (m3 per time step) to the outlet, where the
 * sub basin's own runoff joins it.
 *
 * Three routing schemes exist: instantaneous (the inflow leaves the reach in the same step), a pure translation
 * (lag) and the Muskingum method. The last two use the reach length (a sub basin property) and the model-wide
 * routing parameters (the celerity and the Muskingum weighting factor), which the sub basin properties may
 * override reach by reach.
 */
class Reach {
  public:
    /**
     * The routing schemes: how the inflow of a time step is spread over the following ones.
     */
    enum class Scheme {
        None,      // instantaneous pass-through
        Lag,       // pure translation by length / celerity
        Muskingum  // Muskingum method with K = length / celerity and the weighting factor X
    };

    /**
     * Create the reach of a sub basin.
     *
     * @param subbasin The sub basin the reach belongs to (its outlet is the reach outlet).
     */
    explicit Reach(SubBasin* subbasin);

    virtual ~Reach() = default;

    /**
     * Parse a routing scheme name.
     *
     * @param name The name of the scheme ('none', 'lag' or 'muskingum').
     * @return the scheme.
     */
    static Scheme SchemeFromString(const string& name);

    /**
     * Read the reach geometry from the sub basin properties ('length' [m] and 'slope' [-]) when present, as
     * well as the reach-specific routing parameters ('celerity' [m/s] and 'muskingum_x' [-]) overriding the
     * model-wide ones.
     */
    void Initialize();

    /**
     * Set the routing scheme and bind the model-wide routing parameters.
     *
     * @param settings The routing settings of the model.
     */
    void SetRouting(const RoutingSettings& settings);

    /**
     * Route the inflow of the current time step through the reach.
     *
     * @param inflowVolume The volume entering the reach from upstream [m3 per time step].
     * @param timeStepInDays The time step [days].
     * @return the volume leaving the reach at the sub basin outlet [m3 per time step].
     */
    double Route(double inflowVolume, double timeStepInDays);

    /**
     * Reset the reach to its initial state (nothing in transit, or the saved initial storage).
     */
    void Reset();

    /**
     * Save the current state as the initial state restored by Reset().
     */
    void SaveAsInitialState();

    /**
     * Get the sub basin the reach belongs to.
     *
     * @return the sub basin.
     */
    [[nodiscard]] SubBasin* GetSubbasin() const {
        return _subbasin;
    }

    /**
     * Get the routing scheme.
     *
     * @return the routing scheme.
     */
    [[nodiscard]] Scheme GetScheme() const {
        return _scheme;
    }

    /**
     * Get the reach length [m] (0 when the sub basin has no 'length' property).
     *
     * @return the reach length.
     */
    [[nodiscard]] double GetLength() const {
        return _length;
    }

    /**
     * Get the reach slope [m/m] (0 when the sub basin has no 'slope' property).
     *
     * @return the reach slope.
     */
    [[nodiscard]] double GetSlope() const {
        return _slope;
    }

    /**
     * Get the celerity in use for this reach [m/s]: the sub basin property when present, else the model-wide
     * parameter, else 1.
     *
     * @return the celerity.
     */
    [[nodiscard]] double GetCelerity() const;

    /**
     * Get the Muskingum weighting factor in use for this reach [-]: the sub basin property when present, else
     * the model-wide parameter, else 0.2.
     *
     * @return the weighting factor.
     */
    [[nodiscard]] double GetMuskingumX() const;

    /**
     * Get the travel time of the reach [days]: length / celerity (0 without a length).
     *
     * @return the travel time.
     */
    [[nodiscard]] double GetTravelTimeInDays() const;

    /**
     * Get the volume that entered the reach in the current time step [m3].
     *
     * @return the inflow volume.
     */
    [[nodiscard]] double GetInflow() const {
        return _inflow;
    }

    /**
     * Get the volume that left the reach in the current time step [m3].
     *
     * @return the outflow volume.
     */
    [[nodiscard]] double GetOutflow() const {
        return _outflow;
    }

    /**
     * Get the volume in transit in the reach [m3].
     *
     * @return the storage.
     */
    [[nodiscard]] double GetStorage() const {
        return _storage;
    }

  protected:
    SubBasin* _subbasin;  // non-owning
    Scheme _scheme = Scheme::None;
    double _length = 0;                // m
    double _slope = 0;                 // m/m
    const float* _celerity = nullptr;  // non-owning: model-wide parameter [m/s]
    const float* _x = nullptr;         // non-owning: model-wide Muskingum weighting factor [-]
    double _celerityOverride = NAN_D;  // reach-specific celerity from the sub basin properties
    double _xOverride = NAN_D;         // reach-specific weighting factor from the sub basin properties
    double _inflow = 0;                // m3 per time step
    double _outflow = 0;
    double _storage = 0;  // m3 in transit
    double _initialStorage = 0;

    // Lag scheme: the delivery schedule (slot j: water due j time steps from now) and its ordinates.
    vecDouble _schedule;
    vecDouble _ordinates;
    vecDouble _initialSchedule;

    // Muskingum scheme: the coefficients of the sub-step, the sub-step count and the previous sub-step values.
    int _substeps = 1;
    double _c0 = 1, _c1 = 0, _c2 = 0;
    double _previousInflow = 0;   // m3 per sub-step
    double _previousOutflow = 0;  // m3 per sub-step
    bool _instantaneous = false;  // the travel time is too short for the step: pass-through

    // What the ordinates and coefficients were computed for, to recompute them when it changes.
    double _lastTravelTime = -1;
    double _lastX = -1;
    double _lastTimeStep = -1;

  private:
    double RouteLag(double inflowVolume, double timeStepInDays);

    double RouteMuskingum(double inflowVolume, double timeStepInDays);

    /**
     * Recompute the delivery ordinates of the lag scheme for the current travel time and time step.
     */
    void ComputeOrdinates(double timeStepInDays);

    /**
     * Recompute the Muskingum coefficients (and the sub-step count keeping them stable) for the current
     * parameters and time step.
     */
    void ComputeCoefficients(double timeStepInDays);
};

#endif  // HYDROBRICKS_REACH_H
