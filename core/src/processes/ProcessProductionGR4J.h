#ifndef HYDROBRICKS_PROCESS_PRODUCTION_GR4J_H
#define HYDROBRICKS_PROCESS_PRODUCTION_GR4J_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * GR4J production store, computed as an exact discrete step (Perrin et al., 2003).
 *
 * Lives on the production_store brick and receives the net precipitation Pn
 * (the throughfall from the ground land cover). From the store level S at the
 * start of the time step it computes — once per step, independently of the ODE
 * solver — the infiltration Ps, the evaporation Es, the percolation Perc and
 * therefore the routed output:
 *   PR = (Pn − Ps) + Perc
 *
 * The single output (PR) is sent to the routing input store (uh_input). The
 * evaporation Es is handled by a separate et:gr4j process so that it is logged
 * and accounted as an atmosphere flux; this process recomputes Ps/Es only to
 * obtain the correct store level for the percolation term.
 *
 * Because every term derives from the start-of-step state (GetContentWithoutChanges
 * and SumIncomingFluxes), GetRates() returns the same value at every solver
 * stage, so Euler, Heun and RK4 all reproduce the exact discrete GR4J result.
 */
class ProcessProductionGR4J : public ProcessOutflow {
  public:
    explicit ProcessProductionGR4J(WaterContainer* container);

    ~ProcessProductionGR4J() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::AttachForcing()
     */
    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* _pet;  // non-owning reference

    /**
     * @copydoc Process::GetRates()
     */
    vecDouble GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_PRODUCTION_GR4J_H
