#ifndef HYDROBRICKS_PARAMETER_VARIABLE_H
#define HYDROBRICKS_PARAMETER_VARIABLE_H

#include "Parameter.h"

class ParameterVariable : public Parameter {
  public:
    explicit ParameterVariable(const string& name);

    ~ParameterVariable() override = default;

  protected:
    vecFloat m_values;

  private:
};

class ParameterVariableYearly : public ParameterVariable {
  public:
    explicit ParameterVariableYearly(const string& name);

    ~ParameterVariableYearly() override = default;

    bool SetValues(int yearStart, int yearEnd, const vecFloat& values);

    bool UpdateParameter(int year);

  protected:
    vecInt m_reference;

  private:
};

class ParameterVariableMonthly : public ParameterVariable {
  public:
    explicit ParameterVariableMonthly(const string& name);

    ~ParameterVariableMonthly() override = default;

    bool SetValues(const vecFloat& values);

    bool UpdateParameter(int month);

  protected:
  private:
};

class ParameterVariableDates : public ParameterVariable {
  public:
    explicit ParameterVariableDates(const string& name);

    ~ParameterVariableDates() override = default;

    bool SetTimeAndValues(const vecDouble& time, const vecFloat& values);

    bool UpdateParameter(double timeReference);

  protected:
    vecFloat m_values;
    vecDouble m_reference;

  private:
};

#endif  // HYDROBRICKS_PARAMETER_VARIABLE_H
