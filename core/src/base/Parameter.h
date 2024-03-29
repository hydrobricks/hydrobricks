#ifndef HYDROBRICKS_PARAMETER_H
#define HYDROBRICKS_PARAMETER_H

#include "Includes.h"

class Parameter : public wxObject {
  public:
    explicit Parameter(const string& name, float val = NAN_F);

    ~Parameter() override = default;

    bool IsLinked() const {
        return m_linked;
    }

    void SetAsLinked(bool value = true) {
        m_linked = value;
    }

    string GetName() const {
        return m_name;
    }

    void SetName(const string& name) {
        m_name = name;
    }

    float GetValue() const {
        return m_value;
    }

    float* GetValuePointer() {
        return &m_value;
    }

    void SetValue(float val) {
        m_value = val;
    }

  protected:
    bool m_linked;
    string m_name;
    float m_value;

  private:
};

#endif  // HYDROBRICKS_PARAMETER_H
