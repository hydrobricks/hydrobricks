#ifndef HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H
#define HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessInfiltration.h"

class ProcessInfiltrationSocont : public ProcessInfiltration {
  public:
    explicit ProcessInfiltrationSocont(WaterContainer* container);

    ~ProcessInfiltrationSocont() override = default;

    /**
     * @copydoc Process::AssignParameters()
     */
    void AssignParameters(const ProcessSettings &processSettings) override;

  protected:
    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_INFILTRATION_SOCONT_H
