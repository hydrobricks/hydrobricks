#ifndef HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
#define HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessOutflow.h"

/**
 * Direct outflow: releases the entire container content within the timestep.
 *
 *   outflow = max(0, S)
 *
 * Because it claims the whole content, it cannot share a container with another
 * process drawing from it (IsValid() rejects that); use outflow:rest for shared
 * storages.
 */
class ProcessOutflowDirect : public ProcessOutflow {
  public:
    explicit ProcessOutflowDirect(WaterContainer* container);

    ~ProcessOutflowDirect() override = default;

    /**
     * Register the process parameters and forcing in the settings model.
     *
     * @param modelSettings The settings model to register the parameters in.
     */
    static void RegisterProcessSettings(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsValid()
     *
     * A direct outflow drains the whole container content, so it is incompatible with any sibling
     * process drawing from the same container.
     */
    [[nodiscard]] bool IsValid() const override;

  protected:
    /**
     * @copydoc Process::GetRates()
     */
    const vecDouble& GetRates() override;
};

#endif  // HYDROBRICKS_PROCESS_OUTFLOW_DIRECT_H
