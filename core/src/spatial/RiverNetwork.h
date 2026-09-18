#ifndef HYDROBRICKS_RIVER_NETWORK_H
#define HYDROBRICKS_RIVER_NETWORK_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "Includes.h"
#include "SettingsBasin.h"
#include "SubBasin.h"

/**
 * The river network: the tree of sub basins of a catchment. Each sub basin drains into at most one downstream
 * sub basin; exactly one sub basin (the outlet) is terminal. A model without declared subbasins gets a network
 * of one implicit sub basin holding every hydro unit.
 *
 * The network owns the sub basins, keeps them in processing order (upstream first) and knows the drained area
 * at every outlet. Water transfer between sub basins (reaches, routing) is not implemented yet.
 */
class RiverNetwork {
  public:
    RiverNetwork() = default;

    virtual ~RiverNetwork() = default;

    /**
     * Build the sub basins and their hydro units from the basin settings, then the processing order and the
     * drained areas. Validates the network first.
     *
     * @param basinSettings The basin settings.
     * @return an empty result on success, else the error message.
     */
    [[nodiscard]] ModelResult Initialize(SettingsBasin& basinSettings);

    [[nodiscard]] int GetSubbasinCount() const {
        return static_cast<int>(_subbasins.size());
    }

    /**
     * Get a sub basin by index, in declaration order.
     *
     * @param index The index of the sub basin.
     * @return the sub basin.
     */
    [[nodiscard]] SubBasin* GetSubbasin(size_t index) const;

    /**
     * Get a sub basin by ID.
     *
     * @param id The ID of the sub basin.
     * @return the sub basin, or nullptr if the ID is unknown.
     */
    [[nodiscard]] SubBasin* GetSubbasinById(int id) const;

    /**
     * Get the terminal sub basin (the catchment outlet).
     *
     * @return the outlet sub basin (nullptr before initialization).
     */
    [[nodiscard]] SubBasin* GetOutlet() const {
        return _outlet;
    }

    /**
     * Get the sub basins in processing order: every sub basin comes after all the sub basins draining into it.
     *
     * @return the sub basins, upstream first.
     */
    [[nodiscard]] const std::vector<SubBasin*>& GetProcessingOrder() const {
        return _order;
    }

    /**
     * Get the sub basins draining directly into the given one.
     *
     * @param subbasin The receiving sub basin.
     * @return its upstream neighbours (empty for a headwater).
     */
    [[nodiscard]] std::vector<SubBasin*> GetUpstreamSubbasins(const SubBasin* subbasin) const;

    /**
     * Get a hydro unit by ID, searching every sub basin.
     *
     * @param id The ID of the hydro unit.
     * @return the hydro unit, or nullptr if the ID is unknown.
     */
    [[nodiscard]] HydroUnit* GetHydroUnitById(int id) const;

    [[nodiscard]] int GetHydroUnitCount() const;

    [[nodiscard]] vecInt GetSubbasinIds() const;

    [[nodiscard]] vecInt GetSubbasinDownstreamIds() const;

    [[nodiscard]] vecDouble GetSubbasinLocalAreas() const;

    [[nodiscard]] vecDouble GetSubbasinDrainedAreas() const;

    /**
     * Get the total area of the catchment [m2] (the area drained at the outlet).
     *
     * @return the total area.
     */
    [[nodiscard]] double GetTotalArea() const;

  protected:
    std::vector<std::unique_ptr<SubBasin>> _subbasins;  // owning, in declaration order
    std::unordered_map<int, SubBasin*> _subbasinMap;    // non-owning views into _subbasins
    std::unordered_map<int, HydroUnit*> _hydroUnitMap;  // non-owning views into the sub basins' units
    std::vector<SubBasin*> _order;                      // non-owning, upstream first
    SubBasin* _outlet = nullptr;                        // non-owning

  private:
    void Build(SettingsBasin& basinSettings);

    void BuildProcessingOrder();

    void ComputeDrainedAreas();
};

#endif  // HYDROBRICKS_RIVER_NETWORK_H
