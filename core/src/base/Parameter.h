#ifndef HYDROBRICKS_PARAMETER_H
#define HYDROBRICKS_PARAMETER_H

#include "Includes.h"

class Parameter : public wxObject {
  public:
    explicit Parameter(const string& name, float val = NAN_F);

    ~Parameter() override = default;

    /**
     * Check if the parameter is linked to a process.
     *
     * @return true if the parameter is linked to a process.
     */
    bool IsLinked() const {
        return m_linked;
    }

    /**
     * Set the parameter as linked to a process.
     *
     * @param value true if the parameter is linked to a process.
     */
    void SetAsLinked(bool value = true) {
        m_linked = value;
    }

    /**
     * Get the name of the parameter.
     *
     * @return the name of the parameter.
     */
    string GetName() const {
        return m_name;
    }

    /**
     * Set the name of the parameter.
     *
     * @param name the name of the parameter.
     */
    void SetName(const string& name) {
        m_name = name;
    }

    /**
     * Get the parameter value.
     *
     * @return the parameter value.
     */
    float GetValue() const {
        return m_value;
    }

    /**
     * Get the pointer to the parameter value.
     *
     * @return pointer to the parameter value.
     */
    float* GetValuePointer() {
        return &m_value;
    }

    /**
     * Set the parameter value.
     *
     * @param val the parameter value.
     */
    void SetValue(float val) {
        m_value = val;
    }

  protected:
    bool m_linked;
    string m_name;
    float m_value;
};

#endif  // HYDROBRICKS_PARAMETER_H
