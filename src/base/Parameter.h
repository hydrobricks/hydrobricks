
#ifndef FLHY_PARAMETER_H
#define FLHY_PARAMETER_H

#include "Includes.h"

class Parameter : public wxObject {
  public:
    Parameter();

    ~Parameter() override = default;

    float GetValue() {
        return m_value;
    }

    void SetValue(float val) {
        m_value = val;
    }

  protected:
    float m_value;

  private:
};

#endif  // FLHY_PARAMETER_H
