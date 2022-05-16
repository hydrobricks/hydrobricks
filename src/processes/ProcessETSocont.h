#ifndef HYDROBRICKS_PROCESS_ET_SOCONT_H
#define HYDROBRICKS_PROCESS_ET_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "ProcessET.h"

class ProcessETSocont : public ProcessET {
  public:
    explicit ProcessETSocont(Brick* brick);

    ~ProcessETSocont() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

    void AttachForcing(Forcing* forcing) override;

    vecDouble GetChangeRates() override;

  protected:
    Forcing* m_pet;
    float m_exponent;

  private:
};

#endif  // HYDROBRICKS_PROCESS_ET_SOCONT_H
