#ifndef HYDROBRICKS_PROCESS_ET_SOCONT_H
#define HYDROBRICKS_PROCESS_ET_SOCONT_H

#include "Forcing.h"
#include "Includes.h"
#include "Process.h"

class ProcessETSocont : public Process {
  public:
    ProcessETSocont(Brick* brick);

    ~ProcessETSocont() override = default;

    double GetWaterExtraction() override;

    void SetPetForcing(Forcing* pet) {
        m_pet = pet;
    }

    void SetStockState(double* stock) {
        m_stock = stock;
    }

    void SetStockMaxValue(double stockMax) {
        m_stockMax = stockMax;
    }

    void SetExponent(float exponent) {
        m_exponent = exponent;
    }

  protected:
    Forcing* m_pet;
    double* m_stock;
    double m_stockMax;
    float m_exponent;

  private:
};

#endif  // HYDROBRICKS_PROCESS_ET_SOCONT_H
