#ifndef HYDROBRICKS_PARAMETER_H
#define HYDROBRICKS_PARAMETER_H

#include "Includes.h"

class Parameter : public wxObject {
  public:
    explicit Parameter(float val = NaNf);

    ~Parameter() override = default;

    float GetValue() {
        return m_value;
    }

    float *GetValuePointer() {
        return &m_value;
    }

    void SetValue(float val) {
        m_value = val;
    }

    void SetName(const wxString& name) {
        m_name = name;
    }

  protected:
    float m_value;
    wxString m_name;

  private:
};

#endif  // HYDROBRICKS_PARAMETER_H
