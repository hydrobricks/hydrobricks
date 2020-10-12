
#ifndef FLHY_PARAMETERS_UPDATER_H
#define FLHY_PARAMETERS_UPDATER_H

#include <wx/datetime.h>

#include "Includes.h"
#include "ParameterVariable.h"

class ParametersUpdater : public wxObject {
  public:
    ParametersUpdater();

    ~ParametersUpdater() override = default;

    void DateUpdate(float date);

    void ChangingYear(int year);

    void ChangingMonth(wxDateTime::Month month);

    void ChangingDate(float date);

  protected:

  private:
    std::vector<ParameterVariableYearly*> m_parametersYearly;
    std::vector<ParameterVariableMonthly*> m_parametersMonthly;
    std::vector<ParameterVariableDates*> m_parametersDates;
};

#endif  // FLHY_PARAMETERS_UPDATER_H
