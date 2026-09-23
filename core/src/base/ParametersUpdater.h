#ifndef HYDROBRICKS_PARAMETERS_UPDATER_H
#define HYDROBRICKS_PARAMETERS_UPDATER_H

#include "Includes.h"

class Parameter;

class ParametersUpdater {
  public:
    ParametersUpdater();

    virtual ~ParametersUpdater() = default;

    /**
     * Add a parameter that needs to be updated over time.
     *
     * @param parameter pointer to the parameter.
     */
    void AddParameter(Parameter* parameter);

    /**
     * Add a per-unit monthly parameter override (a parameter that is both spatial and
     * monthly). The target holds the value the brick or process reads; it is rewritten
     * with the month's entry at every month change.
     *
     * @param target pointer to the per-unit value read by the brick or process.
     * @param values pointer to the unit's 12 monthly values (January to December).
     */
    void AddUnitMonthlyOverride(float* target, const vecFloat* values);

    /**
     * Clear the registered parameters and reset the previous date. Called before
     * (re)registering parameters on each run (e.g. during calibration) so the same
     * parameter is not registered several times and the start-date values are
     * re-applied from a clean state.
     */
    void Reset();

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
    // non-owning, per-unit monthly overrides: value to write and the 12 monthly values
    vector<std::pair<float*, const vecFloat*>> _unitMonthlyOverrides;
};

#endif  // HYDROBRICKS_PARAMETERS_UPDATER_H
