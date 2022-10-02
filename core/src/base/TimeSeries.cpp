#include "TimeSeries.h"

#include "FileNetcdf.h"
#include "TimeSeriesDistributed.h"
#include "TimeSeriesUniform.h"

TimeSeries::TimeSeries(VariableType type) : m_type(type) {}

bool TimeSeries::Parse(const std::string &path, std::vector<TimeSeries *> &vecTimeSeries) {
    try {
        FileNetcdf file;

        if (!file.OpenReadOnly(path)) {
            return false;
        }

        // Get number of units
        int unitsNb = file.GetDimLen("hydro_units");

        // Get time length
        int timeLength = file.GetDimLen("time");

        // Get time
        vecFloat time = file.GetVarFloat1D("time", timeLength);
        Time startSt = GetTimeStructFromMJD(time[0]);
        Time endSt = GetTimeStructFromMJD(time[time.size() - 1]);
        double start = GetMJD(startSt.year, startSt.month, startSt.day, startSt.hour, startSt.min);
        double end = GetMJD(endSt.year, endSt.month, endSt.day, endSt.hour, endSt.min);

        // Time step
        double timeStepData = time[1] - time[0];
        int timeStep = 0;
        TimeUnit timeUnit = Day;
        if (timeStepData == 1.0) {
            timeStep = 1;
        } else if (timeStepData > 1.0) {
            timeStep = int(round(timeStepData));
        } else if (timeStepData < 1.0) {
            timeUnit = Hour;
            timeStep = int(round(timeStepData * 24));
        } else {
            throw ShouldNotHappen();
        }

        // Get ids
        vecInt ids = file.GetVarInt1D("id", unitsNb);

        // Extract variables
        int varsNb = file.GetVarsNb();

        int dimIdTime = file.GetDimId("time");

        for (int iVar = 0; iVar < varsNb; ++iVar) {
            // Get variable name
            std::string varName = file.GetVarName(iVar);

            if (varName == "id" || varName == "time") {
                continue;
            }

            // Get forcing type
            VariableType varType;
            if (StringsMatch(varName, "Precipitation")) {
                varType = Precipitation;
            } else if (StringsMatch(varName, "Temperature")) {
                varType = Temperature;
            } else if (StringsMatch(varName, "PET") || StringsMatch(varName, "ETP")) {
                varType = PET;
            } else if (StringsMatch(varName, "Custom1")) {
                varType = Custom1;
            } else if (StringsMatch(varName, "Custom2")) {
                varType = Custom2;
            } else if (StringsMatch(varName, "Custom3")) {
                varType = Custom3;
            } else {
                throw InvalidArgument(
                    wxString::Format(_("Unrecognized variable type (%s) in the provided data."), varName));
            }

            // Instantiate time series
            auto timeSeries = new TimeSeriesDistributed(varType);

            // Retrieve values from netCDF
            vecInt dimIds = file.GetVarDimIds(iVar, 2);

            if (dimIds[0] == dimIdTime) {
                axxd values = file.GetVarDouble2D(iVar, unitsNb, timeLength);
                for (int i = 0; i < values.rows(); ++i) {
                    axd valuesUnit = values.row(i);
                    auto forcingData = new TimeSeriesDataRegular(start, end, timeStep, timeUnit);
                    forcingData->SetValues(
                        std::vector<double>(valuesUnit.data(), valuesUnit.data() + valuesUnit.rows()));
                    timeSeries->AddData(forcingData, ids[i]);
                }
            } else {
                axxd values = file.GetVarDouble2D(iVar, timeLength, unitsNb);
                for (int i = 0; i < values.cols(); ++i) {
                    axd valuesUnit = values.col(i);
                    auto forcingData = new TimeSeriesDataRegular(start, end, timeStep, timeUnit);
                    forcingData->SetValues(
                        std::vector<double>(valuesUnit.data(), valuesUnit.data() + valuesUnit.rows()));
                    timeSeries->AddData(forcingData, ids[i]);
                }
            }

            vecTimeSeries.push_back(timeSeries);
        }

    } catch (std::exception &e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}
