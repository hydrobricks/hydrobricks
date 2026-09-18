#ifndef HYDROBRICKS_REACH_H
#define HYDROBRICKS_REACH_H

#include "Includes.h"

class SubBasin;

/**
 * The channel reach of a sub basin: the main channel from the point where the upstream sub basins enter to
 * the sub basin outlet. It routes the volume arriving from upstream (m3 per time step) to the outlet, where the
 * sub basin's own runoff joins it.
 *
 * Only the instantaneous scheme exists for now (the inflow leaves the reach in the same step). The translation
 * and Muskingum schemes will use the reach geometry (length, slope) read from the sub basin properties.
 */
class Reach {
  public:
    /**
     * The routing schemes: how the inflow of a time step is spread over the following ones.
     */
    enum class Scheme {
        None  // instantaneous pass-through
    };

    /**
     * Create the reach of a sub basin.
     *
     * @param subbasin The sub basin the reach belongs to (its outlet is the reach outlet).
     */
    explicit Reach(SubBasin* subbasin);

    virtual ~Reach() = default;

    /**
     * Read the reach geometry from the sub basin properties ('length' [m] and 'slope' [-]) when present.
     */
    void Initialize();

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
     * Save the current storage as the initial state restored by Reset().
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
    double _length = 0;  // m
    double _slope = 0;   // m/m
    double _inflow = 0;  // m3 per time step
    double _outflow = 0;
    double _storage = 0;  // m3 in transit
    double _initialStorage = 0;
};

#endif  // HYDROBRICKS_REACH_H
