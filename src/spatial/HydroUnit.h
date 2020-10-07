
#ifndef HYDRO_UNIT_H
#define HYDRO_UNIT_H

#include "Includes.h"

class HydroUnit : public wxObject {
  public:
    HydroUnit(float area);

    ~HydroUnit() override = default;

  protected:
    float m_area;

  private:
};

#endif
