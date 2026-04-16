#ifndef HYDROBRICKS_PARAMETER_H
#define HYDROBRICKS_PARAMETER_H

#include "Includes.h"
#include "ParameterModifier.h"

class Parameter {
  public:
    explicit Parameter(const string& name, float val = NAN_F);

    virtual ~Parameter();

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
    const float* GetValuePointer() const {
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

    /**
     * Set a modifier for this parameter that changes its value over time.
     *
     * @param modifier the parameter modifier.
     */
    void SetModifier(const ParameterModifier& modifier) {
        _modifier = modifier;
        _hasModifier = true;
    }

    /**
     * Get the modifier for this parameter.
     *
     * @return pointer to the parameter modifier, or nullptr if none.
     */
    ParameterModifier* GetModifier() {
        return _hasModifier ? &_modifier : nullptr;
    }

    /**
     * Get the modifier for this parameter (const version).
     *
     * @return pointer to the parameter modifier, or nullptr if none.
     */
    const ParameterModifier* GetModifier() const {
        return _hasModifier ? &_modifier : nullptr;
    }

    /**
     * Check if this parameter has a modifier.
     *
     * @return true if the parameter has a modifier.
     */
    bool HasModifier() const {
        return _hasModifier;
    }

    /**
     * Update the parameter value using its modifier.
     *
     * @param date current date in MJD format.
     * @return true if the parameter was updated successfully.
     */
    bool UpdateFromModifier(double date);

    /**
     * Check if the parameter is valid.
     * Verifies that the parameter has a name and value.
     *
     * @return true if the parameter is valid, false otherwise.
     */
    [[nodiscard]] virtual bool IsValid() const;

    /**
     * Validate the parameter.
     * Throws an exception if the parameter is invalid.
     *
     * @throws ModelConfigError if validation fails.
     */
    void Validate() const;

  protected:
    string _name;
    float _value;
    ParameterModifier _modifier;
    bool _hasModifier = false;
};

#endif  // HYDROBRICKS_PARAMETER_H
