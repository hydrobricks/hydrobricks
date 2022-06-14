#ifndef HYDROBRICKS_PARAMETER_VARIABLE_H
#define HYDROBRICKS_PARAMETER_VARIABLE_H

#include <wx/datetime.h>

#include "Parameter.h"

class ParameterVariable : public Parameter {
  public:
    explicit ParameterVariable(const wxString &name, float value = NAN_F);

    ~ParameterVariable() override = default;

  protected:
    vecFloat m_values;

  private:
};

class ParameterVariableYearly : public ParameterVariable {
  public:
    explicit ParameterVariableYearly(const wxString &name, float value = NAN_F);

    ~ParameterVariableYearly() override = default;

    bool SetValues(int yearStart, int yearEnd, const vecFloat& values);

    bool UpdateParameter(int year);

  protected:
    vecInt m_reference;

  private:
};

class ParameterVariableMonthly : public ParameterVariable {
  public:
    explicit ParameterVariableMonthly(const wxString &name, float value = NAN_F);

    ~ParameterVariableMonthly() override = default;

    bool SetValues(const vecFloat& values);

    bool UpdateParameter(wxDateTime::Month month);

  protected:
  private:
};

class ParameterVariableDates : public ParameterVariable {
  public:
    explicit ParameterVariableDates(const wxString &name, float value = NAN_F);

    ~ParameterVariableDates() override = default;

    bool SetTimeAndValues(const vecDouble& time, const vecFloat& values);

    bool UpdateParameter(double timeReference);

  protected:
    vecFloat m_values;
    vecDouble m_reference;

  private:
};

#endif  // HYDROBRICKS_PARAMETER_VARIABLE_H
