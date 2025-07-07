#ifndef HYDROBRICKS_PARAMETERS_UPDATER_H
#define HYDROBRICKS_PARAMETERS_UPDATER_H

#include "Includes.h"
#include "ParameterVariable.h"

class ParametersUpdater : public wxObject {
  public:
    ParametersUpdater();

    ~ParametersUpdater() override = default;

    /**
     * Add a parameter that is variable on a yearly basis.
     *
     * @param parameter pointer to the parameter variable yearly.
     */
    void AddParameterVariableYearly(ParameterVariableYearly* parameter);

    /**
     * Add a parameter that is variable on a monthly basis.
     *
     * @param parameter pointer to the parameter variable monthly.
     */
    void AddParameterVariableMonthly(ParameterVariableMonthly* parameter);

    /**
     * Add a parameter that is variable at specific dates.
     *
     * @param parameter pointer to the parameter variable dates.
     */
    void AddParameterVariableDates(ParameterVariableDates* parameter);

    /**
     * Update the parameters based on the current date.
     *
     * @param date current date in MJD format.
     */
    void DateUpdate(double date);

    /**
     * Get the previous date used for updating parameters.
     *
     * @return previous date in MJD format.
     */
    double GetPreviousDate() {
        return _previousDate;
    }

  protected:
    /**
     * Update the parameters for the new year.
     *
     * @param year new year.
     */
    void ChangingYear(int year);

    /**
     * Update the parameters for the new month.
     *
     * @param month new month.
     */
    void ChangingMonth(int month);

    /**
     * Update the parameters for the new date.
     *
     * @param date new date in MJD format.
     */
    void ChangingDate(double date);

  private:
    bool _active;
    double _previousDate;
    vector<ParameterVariableYearly*> _parametersYearly;
    vector<ParameterVariableMonthly*> _parametersMonthly;
    vector<ParameterVariableDates*> _parametersDates;
};

#endif  // HYDROBRICKS_PARAMETERS_UPDATER_H
