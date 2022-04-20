#ifndef HYDROBRICKS_PROCESS_ET_SOCONT_H
#define HYDROBRICKS_PROCESS_ET_SOCONT_H

#include "Includes.h"
#include "Process.h"

class ProcessETSocont : public Process {
  public:
    ProcessETSocont();

    ~ProcessETSocont() override = default;

    double GetWaterExtraction() override;

  protected:
    double m_pet;
    double m_stock;
    double m_stockMax;
    float m_exponent;

  private:
};

#endif  // HYDROBRICKS_PROCESS_ET_SOCONT_H
