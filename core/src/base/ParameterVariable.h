#ifndef HYDROBRICKS_PARAMETER_VARIABLE_H
#define HYDROBRICKS_PARAMETER_VARIABLE_H

#include "Parameter.h"

class ParameterVariable : public Parameter {
  public:
    explicit ParameterVariable(const string& name);

    ~ParameterVariable() override = default;

  protected:
    vecFloat _values;
};

class ParameterVariableYearly : public ParameterVariable {
  public:
    explicit ParameterVariableYearly(const string& name);

    ~ParameterVariableYearly() override = default;

    /**
     * Set the values of the parameter for a range of years.
     *
     * @param yearStart start year
     * @param yearEnd end year
     * @param values values for each year
     * @return true if the values were set successfully, false otherwise
     */
    bool SetValues(int yearStart, int yearEnd, const vecFloat& values);

    /**
     * Update the parameter value for a specific year.
     *
     * @param year year to update
     * @return true if the parameter was updated successfully, false otherwise
     */
    bool UpdateParameter(int year);

  protected:
    vecInt _reference;
};

class ParameterVariableMonthly : public ParameterVariable {
  public:
    explicit ParameterVariableMonthly(const string& name);

    ~ParameterVariableMonthly() override = default;

    /**
     * Set the values of the parameter for each month.
     *
     * @param values values for each month
     * @return true if the values were set successfully, false otherwise
     */
    bool SetValues(const vecFloat& values);

    /**
     * Update the parameter value for a specific month.
     *
     * @param month month to update
     * @return true if the parameter was updated successfully, false otherwise
     */
    bool UpdateParameter(int month);
};

class ParameterVariableDates : public ParameterVariable {
  public:
    explicit ParameterVariableDates(const string& name);

    ~ParameterVariableDates() override = default;

    /**
     * Set the values of the parameter for a range of dates.
     *
     * @param time vector of dates
     * @param values values for each date
     * @return true if the values were set successfully, false otherwise
     */
    bool SetTimeAndValues(const vecDouble& time, const vecFloat& values);

    /**
     * Update the parameter value for a specific date.
     *
     * @param timeReference date to update
     * @return true if the parameter was updated successfully, false otherwise
     */
    bool UpdateParameter(double timeReference);

  protected:
    vecFloat _values;
    vecDouble _reference;
};

#endif  // HYDROBRICKS_PARAMETER_VARIABLE_H
