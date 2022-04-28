#ifndef HYDROBRICKS_FORCING_H
#define HYDROBRICKS_FORCING_H

#include "Includes.h"

class Forcing : public wxObject {
  public:
    Forcing(VariableType type);

    ~Forcing() override = default;

    VariableType GetType() {
        return m_type;
    }

    double GetValue() {
        return m_value;
    }

    double* GetValuePointer() {
        return &m_value;
    }

  protected:
    VariableType m_type;
    double m_value;

  private:
};

#endif  // HYDROBRICKS_FORCING_H
