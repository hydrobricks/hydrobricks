
#ifndef FLHY_PARAMETER_VARIABLE_H
#define FLHY_PARAMETER_VARIABLE_H

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

    void UpdateParameter(int year);

  protected:
    std::vector<int> m_reference;

  private:
};

class ParameterVariableMonthly : public ParameterVariable {
  public:
    ParameterVariableMonthly();

    ~ParameterVariableMonthly() override = default;

    bool SetValues(const std::vector<float>& values);

    void UpdateParameter(wxDateTime::Month month);

  protected:
  private:
};

class ParameterVariableDates : public ParameterVariable {
  public:
    ParameterVariableDates();

    ~ParameterVariableDates() override = default;

    bool SetTimeAndValues(const std::vector<float>& time, const std::vector<float>& values);

    void UpdateParameter(float timeReference);

  protected:
    std::vector<float> m_values;
    std::vector<float> m_reference;

  private:
};

#endif  // FLHY_PARAMETER_VARIABLE_H
