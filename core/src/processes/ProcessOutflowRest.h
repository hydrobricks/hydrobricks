#ifndef HYDROBRICKS_PROCESS_OUTFLOW_REST_H
#define HYDROBRICKS_PROCESS_OUTFLOW_REST_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Outputs the water remaining in the container after the other processes have taken their share.
 *
 * Works on both directly-computed and solver-handled bricks:
 *  - Direct brick: the sibling processes have already withdrawn their share (sequential
 *    application), so the remaining content is exactly "the rest".
 *  - Solver brick: all process rates are evaluated together before any is applied, so the content
 *    is still full; the siblings' CURRENT outflow rates (their linked change-rate pointers, not the
 *    already-stored amounts) are subtracted so the demands sum to the available content and are not
 *    distorted by the container's non-negativity constraint.
 *
 * It must be declared AFTER the other withdrawing processes so their rates are available when this
 * one is evaluated.
 */
class ProcessOutflowRest : public ProcessOutflow {
  public:
    explicit ProcessOutflowRest(WaterContainer* container);

    ~ProcessOutflowRest() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_REST_H
