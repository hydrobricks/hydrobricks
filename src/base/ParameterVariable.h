#ifndef HYDROBRICKS_PARAMETER_VARIABLE_H
#define HYDROBRICKS_PARAMETER_VARIABLE_H

#include <wx/datetime.h>

#include "Parameter.h"

class ParameterVariable : public Parameter {
  public:
    ParameterVariable();

    ~ParameterVariable() override = default;

  protected:
    std::vector<float> m_values;

  private:
};

class ParameterVariableYearly : public ParameterVariable {
  public:
    ParameterVariableYearly();

    ~ParameterVariableYearly() override = default;

    bool SetValues(int yearStart, int yearEnd, const std::vector<float>& values);

    bool UpdateParameter(int year);

  protected:
    std::vector<int> m_reference;

  private:
};

class ParameterVariableMonthly : public ParameterVariable {
  public:
    ParameterVariableMonthly();

    ~ParameterVariableMonthly() override = default;

    bool SetValues(const std::vector<float>& values);

    bool UpdateParameter(wxDateTime::Month month);

  protected:
  private:
};

class ParameterVariableDates : public ParameterVariable {
  public:
    ParameterVariableDates();

    ~ParameterVariableDates() override = default;

    bool SetTimeAndValues(const std::vector<double>& time, const std::vector<float>& values);

    bool UpdateParameter(double timeReference);

  protected:
    std::vector<float> m_values;
    std::vector<double> m_reference;

  private:
};

#endif  // HYDROBRICKS_PARAMETER_VARIABLE_H
