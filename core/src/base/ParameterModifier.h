#ifndef HYDROBRICKS_PARAMETER_MODIFIER_H
#define HYDROBRICKS_PARAMETER_MODIFIER_H

#include "Includes.h"

/**
 * Type of parameter modifier.
 */
enum class ParameterModifierType {
    Yearly,   // Values change yearly
    Monthly,  // Values change monthly (12 values)
    Dates     // Values change at specific dates
};

/**
 * Parameter modifier that changes parameter values over time.
 * Supports yearly, monthly, and date-based modifications.
 */
class ParameterModifier {
  public:
    ParameterModifier() = default;

    explicit ParameterModifier(ParameterModifierType type)
        : _type(type) {}

    ~ParameterModifier() = default;

    /**
     * Set the type of the modifier.
     *
     * @param type the modifier type.
     */
    void SetType(ParameterModifierType type) {
        _type = type;
    }

    /**
     * Get the type of the modifier.
     *
     * @return the modifier type.
     */
    ParameterModifierType GetType() const {
        return _type;
    }

    /**
     * Set the values of the parameter for a range of years.
     *
     * @param yearStart start year
     * @param yearEnd end year
     * @param values values for each year
     * @return true if the values were set successfully, false otherwise
     */
    bool SetYearlyValues(int yearStart, int yearEnd, const vecFloat& values);

    /**
     * Set the values of the parameter for each month.
     *
     * @param values values for each month (must be size 12)
     * @return true if the values were set successfully, false otherwise
     */
    bool SetMonthlyValues(const vecFloat& values);

    /**
     * Set the values of the parameter for a range of dates.
     *
     * @param dates vector of dates in MJD format
     * @param values values for each date
     * @return true if the values were set successfully, false otherwise
     */
    bool SetDatesAndValues(const vecDouble& dates, const vecFloat& values);

    /**
     * Update the parameter value based on the current date.
     *
     * @param date current date in MJD format.
     * @return the updated parameter value.
     */
    float UpdateValue(double date);

    /**
     * Check if the modifier needs to update for a year change.
     *
     * @return true if the modifier updates on year changes.
     */
    bool UpdatesOnYearChange() const {
        return _type == ParameterModifierType::Yearly;
    }

    /**
     * Check if the modifier needs to update for a month change.
     *
     * @return true if the modifier updates on month changes.
     */
    bool UpdatesOnMonthChange() const {
        return _type == ParameterModifierType::Monthly;
    }

    /**
     * Check if the modifier needs to update for a date change.
     *
     * @return true if the modifier updates on date changes.
     */
    bool UpdatesOnDateChange() const {
        return _type == ParameterModifierType::Dates;
    }

  private:
    ParameterModifierType _type = ParameterModifierType::Dates;
    vecFloat _values;
    vecDouble _dates;  // Used for Yearly (as years) and Dates (as MJD dates)
};

#endif  // HYDROBRICKS_PARAMETER_MODIFIER_H
