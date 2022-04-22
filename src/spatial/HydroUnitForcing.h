#ifndef HYDROBRICKS_HYDRO_UNIT_FORCING_H
#define HYDROBRICKS_HYDRO_UNIT_FORCING_H

#include "Includes.h"

class HydroUnitForcing : public wxObject {
  public:
    HydroUnitForcing();

    ~HydroUnitForcing() override = default;

  protected:
    VariableType m_type;

  private:
};

#endif  // HYDROBRICKS_HYDRO_UNIT_FORCING_H
