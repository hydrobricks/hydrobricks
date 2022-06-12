#include <netcdf.h>

#include "TimeSeries.h"
#include "TimeSeriesUniform.h"
#include "TimeSeriesDistributed.h"

TimeSeries::TimeSeries(VariableType type)
    : m_type(type)
{}

bool TimeSeries::Parse(const wxString &path, std::vector<TimeSeries*> &vecTimeSeries) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    try {
        int ncId, varId, dimIdUnit, dimIdTime;

        CheckNcStatus(nc_open(path, NC_NOWRITE, &ncId));

        // Get number of units
        size_t unitsNb;
        CheckNcStatus(nc_inq_dimid(ncId, "hydro_units", &dimIdUnit));
        CheckNcStatus(nc_inq_dimlen(ncId, dimIdUnit, &unitsNb));

        // Get time length
        size_t timeLength;
        CheckNcStatus(nc_inq_dimid(ncId, "time", &dimIdTime));
        CheckNcStatus(nc_inq_dimlen(ncId, dimIdTime, &timeLength));

        // Get time
        CheckNcStatus(nc_inq_varid(ncId, "time", &varId));
        vecFloat time(timeLength);
        CheckNcStatus(nc_get_var_float(ncId, varId, &time[0]));
        Time startStruct = GetTimeStructFromMJD(time[0]);
        Time endStruct = GetTimeStructFromMJD(time[time.size() - 1]);
        wxDateTime start = wxDateTime(startStruct.day, wxDateTime::Month(startStruct.month - 1),
                                      startStruct.year, startStruct.min);
        wxDateTime end = wxDateTime(endStruct.day, wxDateTime::Month(endStruct.month - 1),
                                    endStruct.year, endStruct.min);

        // Time step
        double timeStepData = time[1] - time[0];
        int timeStep = 0;
        TimeUnit timeUnit = Day;
        if (timeStepData == 1.0) {
            timeStep = 1;
        } else if (timeStepData > 1.0) {
            timeStep = int(round(timeStepData));
        } else if(timeStepData < 1.0) {
            timeUnit = Hour;
            timeStep = int(round(timeStepData * 24));
        } else {
            throw ShouldNotHappen();
        }

        // Get ids
        CheckNcStatus(nc_inq_varid(ncId, "id", &varId));
        vecInt ids(unitsNb);
        CheckNcStatus(nc_get_var_int(ncId, varId, &ids[0]));

        // Extract variables
        int varsNb, gattNb;
        CheckNcStatus(nc_inq(ncId, nullptr, &varsNb, &gattNb, nullptr));

        for (int iVar = 0; iVar < varsNb; ++iVar) {
            // Get variable name
            char varNameChar[NC_MAX_NAME + 1];
            CheckNcStatus(nc_inq_varname(ncId, iVar, varNameChar));
            wxString varName = wxString(varNameChar);
            if (varName.IsSameAs("id", false) ||
                varName.IsSameAs("time", false)) {
                continue;
            }

            // Get forcing type
            VariableType varType;
            if (varName.IsSameAs("Precipitation", false)) {
                varType = Precipitation;
            } else if (varName.IsSameAs("Temperature", false)) {
                varType = Temperature;
            } else if (varName.IsSameAs("PET", false) ||
                       varName.IsSameAs("ETP", false)) {
                varType = PET;
            } else if (varName.IsSameAs("Custom1", false)) {
                varType = Custom1;
            } else if (varName.IsSameAs("Custom2", false)) {
                varType = Custom2;
            } else if (varName.IsSameAs("Custom3", false)) {
                varType = Custom3;
            } else {
                throw InvalidArgument(wxString::Format(_("Unrecognized variable type (%s) in the provided data."), varName));
            }

            // Instantiate time series
            auto timeSeries = new TimeSeriesDistributed(varType);

            // Retrieve values from netCDF
            vecInt dimIds(2);
            CheckNcStatus(nc_inq_vardimid(ncId, iVar, &dimIds[0]));

            if (dimIds[0] == dimIdTime) {
                axxd values = axxd::Zero((long long)unitsNb, (long long)timeLength);
                CheckNcStatus(nc_get_var_double(ncId, iVar, values.data()));
                for (int i = 0; i < values.rows(); ++i) {
                    axd valuesUnit = values.row(i);
                    auto forcingData = new TimeSeriesDataRegular(start, end, timeStep, timeUnit);
                    forcingData->SetValues(std::vector<double>(valuesUnit.data(), valuesUnit.data() + valuesUnit.rows()));
                    timeSeries->AddData(forcingData, ids[i]);
                }
            } else {
                axxd values = axxd::Zero((long long)timeLength, (long long)unitsNb);
                CheckNcStatus(nc_get_var_double(ncId, iVar, values.data()));
                for (int i = 0; i < values.cols(); ++i) {
                    axd valuesUnit = values.col(i);
                    auto forcingData = new TimeSeriesDataRegular(start, end, timeStep, timeUnit);
                    forcingData->SetValues(std::vector<double>(valuesUnit.data(), valuesUnit.data() + valuesUnit.rows()));
                    timeSeries->AddData(forcingData, ids[i]);
                }
            }

            vecTimeSeries.push_back(timeSeries);
        }

        CheckNcStatus(nc_close(ncId));

    } catch(std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}