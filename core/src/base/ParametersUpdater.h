#ifndef HYDROBRICKS_PARAMETERS_UPDATER_H
#define HYDROBRICKS_PARAMETERS_UPDATER_H

#include "Includes.h"

class Parameter;

class ParametersUpdater : public wxObject {
  public:
    ParametersUpdater();

    ~ParametersUpdater() override = default;

    /**
     * Add a parameter that needs to be updated over time.
     *
     * @param parameter pointer to the parameter.
     */
    void AddParameter(Parameter* parameter);

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
    double GetPreviousDate() const {
        return _previousDate;
    }

  protected:
    /**
     * Update the parameters for the new year.
     *
     * @param date new date in MJD format.
     */
    void ChangingYear(double date);

    /**
     * Update the parameters for the new month.
     *
     * @param date new date in MJD format.
     */
    void ChangingMonth(double date);

    /**
     * Update the parameters for the new date.
     *
     * @param date new date in MJD format.
     */
    void ChangingDate(double date);

  private:
    bool _active;
    double _previousDate;
    vector<Parameter*> _parametersYearly;   // non-owning, parameters with yearly modifiers
    vector<Parameter*> _parametersMonthly;  // non-owning, parameters with monthly modifiers
    vector<Parameter*> _parametersDates;    // non-owning, parameters with date modifiers
};

#endif  // HYDROBRICKS_PARAMETERS_UPDATER_H
