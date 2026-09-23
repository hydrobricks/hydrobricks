#ifndef HYDROBRICKS_REACH_H
#define HYDROBRICKS_REACH_H

#include "Includes.h"

class SubBasin;
struct ChannelRoutingSettings;

/**
 * The channel reach of a sub basin: the main channel from the point where the upstream sub basins enter to
 * the sub basin outlet. It routes the volume arriving from upstream (m3 per time step) to the outlet, where the
 * sub basin's own runoff joins it.
 *
 * Four routing schemes exist: instantaneous (the inflow leaves the reach in the same step), a pure translation
 * (lag), the Muskingum method and Muskingum-Cunge. The first three use a celerity: the model-wide parameter
 * (which a sub basin property may override reach by reach), optionally varying with the discharge through a
 * power law. Muskingum-Cunge instead derives the celerity and the weighting factor from the channel geometry
 * (length, slope, width and Manning roughness) and the discharge of the step, so its parameters are measured
 * rather than calibrated. A Muskingum-Cunge reach is also divided into sub reaches short enough for the wave
 * to be resolved (the Ponce and Theurer criterion), the sub reaches being routed in series.
 *
 * The sub basin's own runoff joins at the outlet by default. With 'route_local_runoff' it is routed too,
 * through half the reach: generated uniformly along the reach, it travels half of it on average.
 */
class Reach {
  public:
    /**
     * The routing schemes: how the inflow of a time step is spread over the following ones.
     */
    enum class Scheme {
        None,           // instantaneous pass-through
        Lag,            // pure translation by length / celerity
        Muskingum,      // Muskingum method with K = length / celerity and the weighting factor X
        MuskingumCunge  // Muskingum with K and X derived from the channel geometry and the discharge
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
     * @param name The name of the scheme ('none', 'lag', 'muskingum' or 'muskingum_cunge').
     * @return the scheme.
     */
    static Scheme SchemeFromString(const string& name);

    /**
     * Get the canonical name of a routing scheme.
     *
     * @param scheme The scheme.
     * @return its name ('none', 'lag', 'muskingum' or 'muskingum_cunge').
     */
    static string SchemeToString(Scheme scheme);

    /**
     * Read the reach geometry from the sub basin properties ('length' [m], 'slope' [-], 'width' [m] and
     * 'manning' [s/m^(1/3)]) when present, as well as the reach-specific routing parameters ('celerity' [m/s]
     * and 'muskingum_x' [-]) overriding the model-wide ones.
     */
    void Initialize();

    /**
     * Set the channel routing scheme and bind the model-wide routing parameters.
     *
     * @param settings The channel routing settings of the model.
     */
    void SetChannelRouting(const ChannelRoutingSettings& settings);

    /**
     * Route the volume arriving from the upstream sub basins through the reach, for the current time step.
     * Called once per step, before the sub basin is processed.
     *
     * @param inflowVolume The volume entering the reach from upstream [m3 per time step].
     * @param timeStepInDays The time step [days].
     * @return the volume leaving the reach at the sub basin outlet [m3 per time step].
     */
    double RouteUpstream(double inflowVolume, double timeStepInDays);

    /**
     * Route the sub basin's own runoff, for the current time step. Called once per step, after the sub basin
     * is processed. Without 'route_local_runoff' the runoff is returned unchanged (it joins at the outlet);
     * with it, the runoff travels half the reach.
     *
     * @param localVolume The sub basin's own runoff [m3 per time step].
     * @param timeStepInDays The time step [days].
     * @return the volume of that runoff reaching the outlet [m3 per time step].
     */
    double RouteLocal(double localVolume, double timeStepInDays);

    /**
     * Check whether the sub basin's own runoff is routed through (half) the reach.
     *
     * @return true when the local runoff is routed.
     */
    [[nodiscard]] bool RoutesLocalRunoff() const {
        return _routeLocal && _scheme != Scheme::None;
    }

    /**
     * Reset the reach to its initial state (nothing in transit, or the saved initial state).
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
     * Get the channel width in use for this reach [m]: the sub basin property when present, else the
     * model-wide parameter. Used by the Muskingum-Cunge scheme only.
     *
     * @return the channel width.
     */
    [[nodiscard]] double GetWidth() const;

    /**
     * Get the Manning roughness in use for this reach [s/m^(1/3)]: the sub basin property when present, else
     * the model-wide parameter. Used by the Muskingum-Cunge scheme only.
     *
     * @return the Manning roughness.
     */
    [[nodiscard]] double GetManning() const;

    /**
     * Get the celerity of this reach at the reference discharge [m/s]: the sub basin property when present,
     * else the model-wide parameter, else 1.
     *
     * @return the reference celerity.
     */
    [[nodiscard]] double GetCelerity() const;

    /**
     * Get the celerity of this reach at a given discharge [m/s].
     *
     * With the Muskingum-Cunge scheme it comes from the Manning relation for a wide channel: the kinematic
     * celerity is 5/3 of the flow velocity. With the other schemes it is the reference celerity scaled by the
     * discharge ratio raised to 'celerity_exponent' (0, the default, keeps it constant). The result is clamped
     * to a plausible range for a river so that a vanishing discharge cannot give an unbounded travel time.
     *
     * @param dischargeM3s The discharge [m3/s]; a non-positive value returns the reference celerity.
     * @return the celerity.
     */
    [[nodiscard]] double GetCelerityForDischarge(double dischargeM3s) const;

    /**
     * Get the Muskingum weighting factor in use for this reach [-]: the sub basin property when present, else
     * the model-wide parameter, else 0.2. The Muskingum-Cunge scheme computes its own instead.
     *
     * @return the weighting factor.
     */
    [[nodiscard]] double GetMuskingumX() const;

    /**
     * Get the travel time of the reach at the reference discharge [days]: length / celerity (0 without a
     * length). With Muskingum-Cunge the celerity is the one the geometry gives at that discharge; with the
     * other schemes it is the celerity parameter, which is defined at that same discharge.
     *
     * @return the travel time.
     */
    [[nodiscard]] double GetTravelTimeInDays() const;

    /**
     * Get the number of sub reaches the main channel is divided into for the routing. Always 1 except with
     * the Muskingum-Cunge scheme, and only once the first time step has fixed it.
     *
     * @return the number of sub reaches.
     */
    [[nodiscard]] int GetSubreachCount() const {
        return _main.subreaches;
    }

    /**
     * Get the volume that entered the reach in the current time step [m3]: the upstream inflow, plus the sub
     * basin's own runoff when it is routed.
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
     * Get the volume in transit in the reach [m3], in both the main channel and the local-runoff branch.
     *
     * @return the storage.
     */
    [[nodiscard]] double GetStorage() const {
        return _main.storage + _local.storage;
    }

  protected:
    /**
     * The state of one routing branch: the main channel (the upstream inflow over the whole reach) or the
     * local-runoff branch (the sub basin's own runoff over half of it). Holds the delivery schedule of the lag
     * scheme, the Muskingum coefficients and the volume in transit.
     */
    struct RoutingState {
        // Lag scheme: the delivery schedule (slot j: water due j time steps from now) and its ordinates.
        vecDouble schedule;
        vecDouble ordinates;
        vecDouble initialSchedule;

        // Muskingum schemes: the coefficients of the sub-step, the sub-step count and the previous values.
        int substeps = 1;
        double c0 = 1, c1 = 0, c2 = 0;
        bool instantaneous = false;  // the travel time is too short for the step: pass-through

        // The sub reaches routed in series (one, unless the Muskingum-Cunge criterion asks for more), each
        // with its own memory of the previous sub-step and its own share of the volume in transit.
        int subreaches = 1;
        vecDouble previousInflow;   // m3 per sub-step, per sub reach
        vecDouble previousOutflow;  // m3 per sub-step, per sub reach
        vecDouble subStorage;       // m3 in transit, per sub reach
        vecDouble initialSubStorage;
        double subreachTimeStep = -1;  // the time step the sub reach count was computed for

        double storage = 0;  // m3 in transit, over all the sub reaches
        double initialStorage = 0;

        // What the ordinates and coefficients were computed for, to recompute them when it changes.
        double lastTravelTime = -1;
        double lastX = -1;
        double lastTimeStep = -1;
    };

    SubBasin* _subbasin;  // non-owning
    Scheme _scheme = Scheme::None;
    bool _routeLocal = false;
    double _length = 0;  // m
    double _slope = 0;   // m/m

    // Model-wide parameters (non-owning pointers into the channel routing settings).
    const float* _celerity = nullptr;
    const float* _celerityExponent = nullptr;
    const float* _referenceDischarge = nullptr;
    const float* _x = nullptr;
    const float* _width = nullptr;
    const float* _manning = nullptr;

    // Reach-specific values from the sub basin properties (NaN when absent).
    double _celerityOverride = NAN_D;
    double _xOverride = NAN_D;
    double _widthOverride = NAN_D;
    double _manningOverride = NAN_D;

    double _inflow = 0;  // m3 per time step
    double _outflow = 0;

    RoutingState _main;   // the upstream inflow, over the whole reach
    RoutingState _local;  // the sub basin's own runoff, over half the reach

  private:
    /**
     * Route a volume through one branch of the reach.
     *
     * @param state The routing state of the branch.
     * @param volume The volume entering the branch [m3 per time step].
     * @param length The length of the branch [m].
     * @param timeStepInDays The time step [days].
     * @return the volume leaving the branch [m3 per time step].
     */
    double RouteBranch(RoutingState& state, double volume, double length, double timeStepInDays);

    double RouteLag(RoutingState& state, double volume, double travelTime, double timeStepInDays);

    double RouteMuskingum(RoutingState& state, double volume, double travelTime, double x, double timeStepInDays);

    /**
     * Compute the Muskingum-Cunge travel time and weighting factor from the channel geometry and the
     * discharge of the step.
     *
     * @param dischargeM3s The reference discharge of the step [m3/s].
     * @param length The length of the branch [m].
     * @param travelTime The resulting travel time K [days].
     * @param x The resulting weighting factor X [-].
     */
    void ComputeCungeParameters(double dischargeM3s, double length, double& travelTime, double& x) const;

    /**
     * Compute the number of sub reaches a length must be divided into for the Muskingum-Cunge scheme to
     * resolve the wave, from the criterion of Ponce and Theurer (1982): a sub reach may not be longer than
     * (c dt + Q / (B S0 c)) / 2. The celerity and the discharge are those of the reference discharge, not of
     * the step: the count must stay fixed while the model runs, since the sub reaches carry state.
     *
     * @param length The length of the branch [m].
     * @param timeStepInDays The time step [days].
     * @return the number of sub reaches (at least 1).
     */
    [[nodiscard]] int ComputeSubreachCount(double length, double timeStepInDays) const;

    /**
     * Set the number of sub reaches of a branch, resizing its state. The volume in transit is spread evenly
     * over the new sub reaches and the memory of the previous sub-step is dropped: the discretization it was
     * expressed on no longer exists.
     */
    static void SetSubreachCount(RoutingState& state, int count);

    /**
     * Recompute the delivery ordinates of the lag scheme for the given travel time and time step.
     */
    static void ComputeOrdinates(RoutingState& state, double travelTime, double timeStepInDays);

    /**
     * Recompute the Muskingum coefficients (and the sub-step count keeping them stable) for the given
     * parameters and time step.
     */
    void ComputeCoefficients(RoutingState& state, double travelTime, double x, double timeStepInDays) const;

    static void ResetState(RoutingState& state);
};

#endif  // HYDROBRICKS_REACH_H
