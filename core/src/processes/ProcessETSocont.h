#ifndef HYDROBRICKS_PROCESS_ET_SOCONT_H
#define HYDROBRICKS_PROCESS_ET_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

class ProcessETSocont : public ProcessET {
  public:
    explicit ProcessETSocont(WaterContainer* container);

    ~ProcessETSocont() override = default;

    static void RegisterProcessParametersAndForcing(SettingsModel* modelSettings);

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    void AttachForcing(Forcing* forcing) override;

  protected:
    Forcing* m_pet;
    float m_exponent;

    vecDouble GetRates() override;

  private:
};

#endif  // HYDROBRICKS_PROCESS_ET_SOCONT_H
