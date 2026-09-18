#ifndef HYDROBRICKS_PROCESS_ET_WET_SURFACE_PREVAH_H
#define HYDROBRICKS_PROCESS_ET_WET_SURFACE_PREVAH_H

#include "Includes.h"
#include "ProcessETOpenWaterPrevah.h"

class LandCover;
class ProcessOutflowSplit;

/**
 * PREVAH wet-surface evaporation, drawn from a groundwater store.
 *
 * PREVAH evaporates its wet surfaces at et_pot * wet_surface from the groundwater,
 * where wet_surface is the share of the hydrotope that is water or saturated (0.7 for
 * wetlands). The same share routes the input of the wet surface directly to the
 * groundwater. Here the wet share of the hydro unit is derived from its wetland land
 * covers (the gate bricks): each contributes its live area fraction times its wet
 * fraction, read from the split process that routes its input to the groundwater:
 *   wet = sum_c fraction_c * wet_fraction_c
 *   Ea  = min(et_factor * wet * PET * (1 - albedo)/0.8, content / dt)
 *
 * A unit without any wetland area therefore evaporates nothing, the wet share follows a
 * calibrated wet_fraction and tracks land cover changes. The et_factor parameter
 * (default 1) remains as an overall scaling. Requires a hydro-unit context and at
 * least one gate brick (a wetland land cover whose input is split by 'outflow:split').
 */
class ProcessETWetSurfacePrevah : public ProcessETOpenWaterPrevah {
  public:
    explicit ProcessETWetSurfacePrevah(WaterContainer* container);

    ~ProcessETWetSurfacePrevah() override = default;

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::NeedsGateBrickLinking()
     */
    [[nodiscard]] bool NeedsGateBrickLinking() const override {
        return true;
    }

    /**
     * @copydoc Process::AddGateBrick()
     *
     * The gate brick must be a wetland land cover carrying an 'outflow:split' process,
     * whose split fraction is the wet fraction of the cover.
     */
    void AddGateBrick(Brick* brick) override;

    /**
     * Get the wet share of the hydro unit: the sum over the wetland covers of their
     * area fraction times their wet fraction.
     *
     * @return the wet share of the hydro unit [-].
     */
    [[nodiscard]] double GetWetShare() const;

  protected:
    vector<LandCover*> _wetlands;             // non-owning references to the wetland land covers
    vector<ProcessOutflowSplit*> _wetSplits;  // non-owning references to their wet-fraction splits

    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_ET_WET_SURFACE_PREVAH_H
