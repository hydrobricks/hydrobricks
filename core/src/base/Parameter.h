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
        return _linked;
    }

    /**
     * Set the parameter as linked to a process.
     *
     * @param value true if the parameter is linked to a process.
     */
    void SetAsLinked(bool value = true) {
        _linked = value;
    }

    /**
     * Get the name of the parameter.
     *
     * @return the name of the parameter.
     */
    string GetName() const {
        return _name;
    }

    /**
     * Set the name of the parameter.
     *
     * @param name the name of the parameter.
     */
    void SetName(const string& name) {
        _name = name;
    }

    /**
     * Get the parameter value.
     *
     * @return the parameter value.
     */
    float GetValue() const {
        return _value;
    }

    /**
     * Get the pointer to the parameter value.
     *
     * @return pointer to the parameter value.
     */
    float* GetValuePointer() {
        return &_value;
    }

    /**
     * Set the parameter value.
     *
     * @param val the parameter value.
     */
    void SetValue(float val) {
        _value = val;
    }

  protected:
    bool _linked;
    string _name;
    float _value;
};

#endif  // HYDROBRICKS_PARAMETER_H
