#ifndef HYDROBRICKS_RIVER_NETWORK_H
#define HYDROBRICKS_RIVER_NETWORK_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "Includes.h"
#include "Reach.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"

/**
 * The river network: the tree of sub basins of a catchment. Each sub basin drains into at most one downstream
 * sub basin; exactly one sub basin (the outlet) is terminal. A model without declared subbasins gets a network
 * of one implicit sub basin holding every hydro unit.
 *
 * The network owns the sub basins and their reaches, keeps the sub basins in processing order (upstream first)
 * and knows the drained area at every outlet. Within a time step, the sub basins are processed in that order:
 * before a sub basin is processed, the outlet volumes of its upstream neighbours (computed earlier in the same
 * step) are routed through its reach and deposited as its inflow (TransferInflow).
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

    /**
     * Get the number of sub basins.
     *
     * @return the number of sub basins.
     */
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
    [[nodiscard]] const std::vector<SubBasin*>& GetUpstreamSubbasins(const SubBasin* subbasin) const;

    /**
     * Route the outlet volumes of the upstream sub basins through the reach of the given sub basin and set the
     * result as its inflow for the current time step. Must be called before the sub basin is processed and
     * after its upstream neighbours were.
     *
     * @param subbasin The receiving sub basin.
     * @param timeStepInDays The time step [days].
     */
    void TransferInflow(SubBasin* subbasin, double timeStepInDays);

    /**
     * Set the routing scheme and parameters of every reach.
     *
     * @param settings The routing settings of the model.
     */
    void SetRouting(const RoutingSettings& settings);

    /**
     * Assign the land cover fractions of every hydro unit from the basin settings.
     *
     * @param basinSettings The basin settings.
     * @return an empty result on success, else the error message.
     */
    [[nodiscard]] ModelResult AssignFractions(SettingsBasin& basinSettings);

    /**
     * Reset the reaches (the sub basins are reset by the model).
     */
    void Reset();

    /**
     * Save the state of the reaches as the initial state restored by Reset().
     */
    void SaveAsInitialState();

    /**
     * Get the reach of a sub basin by index, in declaration order.
     *
     * @param index The index of the sub basin.
     * @return the reach.
     */
    [[nodiscard]] Reach* GetReach(size_t index) const;

    /**
     * Get a hydro unit by ID, searching every sub basin.
     *
     * @param id The ID of the hydro unit.
     * @return the hydro unit, or nullptr if the ID is unknown.
     */
    [[nodiscard]] HydroUnit* GetHydroUnitById(int id) const;

    /**
     * Get the number of hydro units over all the sub basins.
     *
     * @return the number of hydro units.
     */
    [[nodiscard]] int GetHydroUnitCount() const;

    /**
     * Get the sub basin IDs, in declaration order.
     *
     * @return the sub basin IDs.
     */
    [[nodiscard]] vecInt GetSubbasinIds() const;

    /**
     * Get the downstream sub basin IDs (0: the outlet), in declaration order.
     *
     * @return the downstream sub basin IDs.
     */
    [[nodiscard]] vecInt GetSubbasinDownstreamIds() const;

    /**
     * Get the local areas of the sub basins (their own hydro units) [m2], in declaration order.
     *
     * @return the local areas.
     */
    [[nodiscard]] vecDouble GetSubbasinLocalAreas() const;

    /**
     * Get the areas drained at the sub basin outlets (own plus upstream) [m2], in declaration order.
     *
     * @return the drained areas.
     */
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
    std::vector<std::unique_ptr<Reach>> _reaches;       // owning, aligned with _subbasins
    std::unordered_map<const SubBasin*, std::vector<SubBasin*>> _upstream;  // non-owning views

  private:
    /**
     * Create the sub basins and their hydro units (one implicit sub basin when none is declared), the lateral
     * connections and the upstream links.
     *
     * @param basinSettings The basin settings.
     */
    void Build(SettingsBasin& basinSettings);

    /**
     * Order the sub basins upstream first (post-order traversal from the outlet).
     */
    void BuildProcessingOrder();

    /**
     * Accumulate the local areas along the tree into the drained area of every sub basin.
     */
    void ComputeDrainedAreas();

    /**
     * Create one reach per sub basin and read its geometry from the sub basin properties.
     */
    void BuildReaches();
};

#endif  // HYDROBRICKS_RIVER_NETWORK_H
