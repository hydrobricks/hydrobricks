#ifndef HYDROBRICKS_RIVER_NETWORK_H
#define HYDROBRICKS_RIVER_NETWORK_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "Includes.h"
#include "Reach.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "Subbasin.h"

/**
 * The river network: the tree of subbasins of a catchment. Each subbasin drains into at most one downstream
 * subbasin; exactly one subbasin (the outlet) is terminal. A model without declared subbasins gets a network
 * of one implicit subbasin holding every hydro unit.
 *
 * The network owns the subbasins and their reaches, keeps the subbasins in processing order (upstream first)
 * and knows the drained area at every outlet. Within a time step, the subbasins are processed in that order:
 * before a subbasin is processed, the outlet volumes of its upstream neighbours (computed earlier in the same
 * step) are routed through its reach and deposited as its inflow (TransferInflow).
 */
class RiverNetwork {
  public:
    RiverNetwork() = default;

    virtual ~RiverNetwork() = default;

    /**
     * Build the subbasins and their hydro units from the basin settings, then the processing order and the
     * drained areas. Validates the network first.
     *
     * @param basinSettings The basin settings.
     * @return an empty result on success, else the error message.
     */
    [[nodiscard]] ModelResult Initialize(SettingsBasin& basinSettings);

    /**
     * Get the number of subbasins.
     *
     * @return the number of subbasins.
     */
    [[nodiscard]] int GetSubbasinCount() const {
        return static_cast<int>(_subbasins.size());
    }

    /**
     * Get a subbasin by index, in declaration order.
     *
     * @param index The index of the subbasin.
     * @return the subbasin.
     */
    [[nodiscard]] Subbasin* GetSubbasin(size_t index) const;

    /**
     * Get a subbasin by ID.
     *
     * @param id The ID of the subbasin.
     * @return the subbasin, or nullptr if the ID is unknown.
     */
    [[nodiscard]] Subbasin* GetSubbasinById(int id) const;

    /**
     * Get the terminal subbasin (the catchment outlet).
     *
     * @return the outlet subbasin (nullptr before initialization).
     */
    [[nodiscard]] Subbasin* GetOutlet() const {
        return _outlet;
    }

    /**
     * Get the subbasins in processing order: every subbasin comes after all the subbasins draining into it.
     *
     * @return the subbasins, upstream first.
     */
    [[nodiscard]] const std::vector<Subbasin*>& GetProcessingOrder() const {
        return _order;
    }

    /**
     * Get the subbasins draining directly into the given one.
     *
     * @param subbasin The receiving subbasin.
     * @return its upstream neighbours (empty for a headwater).
     */
    [[nodiscard]] const std::vector<Subbasin*>& GetUpstreamSubbasins(const Subbasin* subbasin) const;

    /**
     * Route the outlet volumes of the upstream subbasins through the reach of the given subbasin and set the
     * result as its inflow for the current time step. Must be called before the subbasin is processed and
     * after its upstream neighbours were.
     *
     * @param subbasin The receiving subbasin.
     * @param timeStepInDays The time step [days].
     */
    void TransferInflow(Subbasin* subbasin, double timeStepInDays);

    /**
     * Set the channel routing scheme and parameters of every reach.
     *
     * @param settings The channel routing settings of the model.
     */
    void SetChannelRouting(const ChannelRoutingSettings& settings);

    /**
     * Assign the land cover fractions of every hydro unit from the basin settings.
     *
     * @param basinSettings The basin settings.
     * @return an empty result on success, else the error message.
     */
    [[nodiscard]] ModelResult AssignFractions(SettingsBasin& basinSettings);

    /**
     * Reset the reaches (the subbasins are reset by the model).
     */
    void Reset();

    /**
     * Save the state of the reaches as the initial state restored by Reset().
     */
    void SaveAsInitialState();

    /**
     * Get the reach of a subbasin by index, in declaration order.
     *
     * @param index The index of the subbasin.
     * @return the reach.
     */
    [[nodiscard]] Reach* GetReach(size_t index) const;

    /**
     * Get a hydro unit by ID, searching every subbasin.
     *
     * @param id The ID of the hydro unit.
     * @return the hydro unit, or nullptr if the ID is unknown.
     */
    [[nodiscard]] HydroUnit* GetHydroUnitById(int id) const;

    /**
     * Get the number of hydro units over all the subbasins.
     *
     * @return the number of hydro units.
     */
    [[nodiscard]] int GetHydroUnitCount() const;

    /**
     * Get the subbasin IDs, in declaration order.
     *
     * @return the subbasin IDs.
     */
    [[nodiscard]] vecInt GetSubbasinIds() const;

    /**
     * Get the downstream subbasin IDs (0: the outlet), in declaration order.
     *
     * @return the downstream subbasin IDs.
     */
    [[nodiscard]] vecInt GetSubbasinDownstreamIds() const;

    /**
     * Get the local areas of the subbasins (their own hydro units) [m2], in declaration order.
     *
     * @return the local areas.
     */
    [[nodiscard]] vecDouble GetSubbasinLocalAreas() const;

    /**
     * Get the areas drained at the subbasin outlets (own plus upstream) [m2], in declaration order.
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
    std::vector<std::unique_ptr<Subbasin>> _subbasins;  // owning, in declaration order
    std::unordered_map<int, Subbasin*> _subbasinMap;    // non-owning views into _subbasins
    std::unordered_map<int, HydroUnit*> _hydroUnitMap;  // non-owning views into the subbasins' units
    std::vector<Subbasin*> _order;                      // non-owning, upstream first
    Subbasin* _outlet = nullptr;                        // non-owning
    std::vector<std::unique_ptr<Reach>> _reaches;       // owning, aligned with _subbasins
    std::unordered_map<const Subbasin*, std::vector<Subbasin*>> _upstream;  // non-owning views

  private:
    /**
     * Create the subbasins and their hydro units (one implicit subbasin when none is declared), the lateral
     * connections and the upstream links.
     *
     * @param basinSettings The basin settings.
     */
    void Build(SettingsBasin& basinSettings);

    /**
     * Order the subbasins upstream first (post-order traversal from the outlet).
     */
    void BuildProcessingOrder();

    /**
     * Accumulate the local areas along the tree into the drained area of every subbasin.
     */
    void ComputeDrainedAreas();

    /**
     * Create one reach per subbasin and read its geometry from the subbasin properties.
     */
    void BuildReaches();
};

#endif  // HYDROBRICKS_RIVER_NETWORK_H
