#include <netcdf.h>

#include "Logger.h"

Logger::Logger()
    : m_cursor(0)
{}

void Logger::InitContainer(int timeSize, int hydroUnitsNb, const vecStr &subBasinLabels, const vecStr &hydroUnitLabels) {
    m_time.resize(timeSize);
    m_subBasinLabels = subBasinLabels;
    m_subBasinValues = vecAxd(subBasinLabels.size(), axd::Zero(timeSize));
    m_subBasinValuesPt.resize(subBasinLabels.size());
    m_hydroUnitIds.resize(hydroUnitsNb);
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Zero(timeSize, hydroUnitsNb));
    m_hydroUnitValuesPt = std::vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitsNb, nullptr));
}

void Logger::SetSubBasinValuePointer(int iLabel, double* valPt) {
    wxASSERT(m_subBasinValuesPt.size() > iLabel);
    m_subBasinValuesPt[iLabel] = valPt;
}

void Logger::SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt) {
    wxASSERT(m_hydroUnitValuesPt.size() > iLabel);
    wxASSERT(m_hydroUnitValuesPt[iLabel].size() > iUnit);
    m_hydroUnitValuesPt[iLabel][iUnit] = valPt;
}

void Logger::SetDateTime(double dateTime) {
    wxASSERT(m_cursor < m_time.size());
    m_time[m_cursor] = dateTime;
}

void Logger::Record() {
    wxASSERT(m_cursor < m_time.size());

    for (int iSubBasin = 0; iSubBasin < m_subBasinValuesPt.size(); ++iSubBasin) {
        wxASSERT(m_subBasinValuesPt[iSubBasin]);
        m_subBasinValues[iSubBasin][m_cursor] = *m_subBasinValuesPt[iSubBasin];
    }

    for (int iUnitVal = 0; iUnitVal < m_hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < m_hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            wxASSERT(m_hydroUnitValuesPt[iUnitVal][iUnit]);
            m_hydroUnitValues[iUnitVal](m_cursor, iUnit) = *m_hydroUnitValuesPt[iUnitVal][iUnit];
        }
    }
}

void Logger::Increment() {
    m_cursor++;
}

bool Logger::DumpOutputs(const wxString &path) {
    if (!wxDirExists(path)) {
        wxLogError(_("The directory %s could not be found."), path);
        return false;
    }

    try {
        int ncId, varId, dimIdTime, dimIdUnit, dimIdItemsAgg, dimIdItemsDist;

        wxString filePath = path;
        filePath.Append(wxFileName::GetPathSeparator());
        filePath.Append("results.nc");

        int timeSize = int(m_time.size());
        int hydroUnitsNb = int(m_hydroUnitIds.size());
        int subBasinLabelsNb = int(m_subBasinLabels.size());
        int hydroUnitLabelsNb = int(m_hydroUnitLabels.size());

        // Create file
        CheckNcStatus(nc_create(filePath, NC_NETCDF4 | NC_CLOBBER, &ncId));

        // Create dimensions
        CheckNcStatus(nc_def_dim(ncId, "time", timeSize, &dimIdTime));
        CheckNcStatus(nc_def_dim(ncId, "hydro_units", hydroUnitsNb, &dimIdUnit));
        CheckNcStatus(nc_def_dim(ncId, "aggregated_values", subBasinLabelsNb, &dimIdItemsAgg));
        CheckNcStatus(nc_def_dim(ncId, "distributed_values", hydroUnitLabelsNb, &dimIdItemsDist));

        // Create variables and put data
        vecInt dimTimeVar = {dimIdTime};
        CheckNcStatus(nc_def_var(ncId, "time", NC_DOUBLE, 1, &dimTimeVar[0], &varId));
        CheckNcStatus(nc_put_var_double(ncId, varId, &m_time[0]));

        vecInt dimHydroUnitVar = {dimIdUnit};
        CheckNcStatus(nc_def_var(ncId, "hydro_units", NC_INT, 1, &dimHydroUnitVar[0], &varId));
        CheckNcStatus(nc_put_var_int(ncId, varId, &m_hydroUnitIds[0]));

        vecInt dimSubBasinValuesVar = {dimIdItemsAgg, dimIdTime};
        CheckNcStatus(nc_def_var(ncId, "sub_basin_values", NC_DOUBLE, 2, &dimSubBasinValuesVar[0], &varId));
        CheckNcStatus(nc_def_var_deflate(ncId, varId, true, true, 7));
        CheckNcStatus(nc_put_var_double(ncId, varId, &m_subBasinValues[0][0]));

        vecInt dimHydroUnitValuesVar = {dimIdItemsDist, dimIdUnit, dimIdTime};
        CheckNcStatus(nc_def_var(ncId, "hydro_units_values", NC_DOUBLE, 3, &dimHydroUnitValuesVar[0], &varId));
        CheckNcStatus(nc_def_var_deflate(ncId, varId, true, true, 7));
        CheckNcStatus(nc_put_var_double(ncId, varId, &m_hydroUnitValues[0](0, 0)));

        // Attributes
        //nc_put_att_string(ncId, NC_GLOBAL, "labels_aggregated", subBasinLabelsNb, m_subBasinLabels[0].mb_str());
        //nc_put_att_string(ncId, NC_GLOBAL, "labels_distributed", subBasinLabelsNb, m_hydroUnitLabels[0].c_str());

        CheckNcStatus(nc_close(ncId));

    } catch(std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}