
#include "TimeSeriesData.h"

/*
 * TimeSeriesData
 */

TimeSeriesData::TimeSeriesData() {}

double TimeSeriesData::GetValueFor(const wxDateTime &date) {
    wxFAIL;
    return 0;
}


/*
 * TimeSeriesDataRegular
 */

TimeSeriesDataRegular::TimeSeriesDataRegular(const wxDateTime &start, const wxDateTime &end, int timeStep,
                                             TimeUnit timeStepUnit) {}

double TimeSeriesDataRegular::GetValueFor(const wxDateTime &date) {
    wxFAIL;
    return 0;
}


/*
 * TimeSeriesDataIrregular
 */

TimeSeriesDataIrregular::TimeSeriesDataIrregular() {}

double TimeSeriesDataIrregular::GetValueFor(const wxDateTime &date) {
    wxFAIL;
    return 0;
}

