#ifndef HYDROBRICKS_PROCESS_SUBLIMATION_H
#define HYDROBRICKS_PROCESS_SUBLIMATION_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

/**
 * Base class for snow sublimation processes.
 *
 * Sublimation removes snow water equivalent from a snowpack snow container
 * directly to the atmosphere (the solid-to-vapour analog of evapotranspiration).
 * Like ET, it has a single output and reports ToAtmosphere() == true, so the
 * model builder attaches a FluxToAtmosphere automatically and no target brick is
 * required.
 *
 * Snow sublimation can be a sizeable term of the alpine/high-mountain snow mass
 * balance — 0.1 to 90 % of snowfall depending on site and weather, and of the
 * order of 30 % of the annual snow water equivalent in some semiarid mountains
 * (Strasser et al., 2008; Herrero & Polo, 2016).
 */
class ProcessSublimation : public Process {
  public:
    explicit ProcessSublimation(WaterContainer* container);

    ~ProcessSublimation() override = default;

    /**
     * @copydoc Process::IsValid()
     */
    [[nodiscard]] bool IsValid() const override;

    /**
     * @copydoc Process::GetConnectionCount()
     */
    [[nodiscard]] int GetConnectionCount() const override;

    /**
     * @copydoc Process::GetValuePointer()
     */
    double* GetValuePointer(std::string_view name) override;

    /**
     * @copydoc Process::ToAtmosphere()
     */
    [[nodiscard]] bool ToAtmosphere() const override {
        return true;
    }
};

#endif  // HYDROBRICKS_PROCESS_SUBLIMATION_H
