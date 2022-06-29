#include <netcdf.h>

#include "Logger.h"

Logger::Logger()
    : m_cursor(0)
{}

void Logger::InitContainer(int timeSize, const vecInt &hydroUnitsIds, const vecStr &subBasinLabels, const vecStr &hydroUnitLabels) {
    m_time.resize(timeSize);
    m_subBasinLabels = subBasinLabels;
    m_subBasinValues = vecAxd(subBasinLabels.size(), axd::Ones(timeSize) * NAN_D);
    m_subBasinValuesPt.resize(subBasinLabels.size());
    m_hydroUnitIds = hydroUnitsIds;
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Ones(timeSize, hydroUnitsIds.size()) * NAN_D);
    m_hydroUnitValuesPt = std::vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitsIds.size(), nullptr));
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

void Logger::SetDate(double date) {
    wxASSERT(m_cursor < m_time.size());
    m_time[m_cursor] = date;
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

        size_t timeSize = m_time.size();
        size_t hydroUnitsNb = m_hydroUnitIds.size();
        size_t subBasinLabelsNb = m_subBasinLabels.size();
        size_t hydroUnitLabelsNb = m_hydroUnitLabels.size();

        wxCharBuffer buffer;

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
        buffer = wxString("time").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "long_name", strlen(buffer.data()), buffer.data()));
        buffer = wxString("days since 1858-11-17 00:00:00.0").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "units", strlen(buffer.data()), buffer.data()));

        vecInt dimHydroUnitVar = {dimIdUnit};
        CheckNcStatus(nc_def_var(ncId, "hydro_units_ids", NC_INT, 1, &dimHydroUnitVar[0], &varId));
        CheckNcStatus(nc_put_var_int(ncId, varId, &m_hydroUnitIds[0]));
        buffer = wxString("hydrological units ids").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "long_name", strlen(buffer.data()), buffer.data()));

        vecInt dimSubBasinValuesVar = {dimIdItemsAgg, dimIdTime};
        CheckNcStatus(nc_def_var(ncId, "sub_basin_values", NC_DOUBLE, 2, &dimSubBasinValuesVar[0], &varId));
        CheckNcStatus(nc_def_var_deflate(ncId, varId, NC_SHUFFLE, true, 7));
        for (size_t i = 0; i < m_subBasinValues.size(); ++i) {
            size_t start[] = {i, 0};
            size_t count[] = {1, timeSize};
            CheckNcStatus(nc_put_vara_double(ncId, varId, start, count, &m_subBasinValues[i](0)));
        }
        buffer = wxString("aggregated values over the sub basin").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "long_name", strlen(buffer.data()), buffer.data()));
        buffer = wxString("mm").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "units", strlen(buffer.data()), buffer.data()));

        vecInt dimHydroUnitValuesVar = {dimIdItemsDist, dimIdUnit, dimIdTime};
        CheckNcStatus(nc_def_var(ncId, "hydro_units_values", NC_DOUBLE, 3, &dimHydroUnitValuesVar[0], &varId));
        CheckNcStatus(nc_def_var_deflate(ncId, varId, NC_SHUFFLE, true, 7));
        for (size_t i = 0; i < m_hydroUnitValues.size(); ++i) {
            size_t start[] = {i, 0, 0};
            size_t count[] = {1, hydroUnitsNb, timeSize};
            CheckNcStatus(nc_put_vara_double(ncId, varId, start, count, &m_hydroUnitValues[i](0, 0)));
        }
        buffer = wxString("values for each hydrological units").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "long_name", strlen(buffer.data()), buffer.data()));
        buffer = wxString("mm").ToUTF8();
        CheckNcStatus(nc_put_att_text(ncId, varId, "units", strlen(buffer.data()), buffer.data()));

        // Global attributes
        std::vector<const char *> subBasinLabels;
        for (const auto& label :m_subBasinLabels) {
            const char* str = (const char*)label.mb_str(wxConvUTF8);
            subBasinLabels.push_back(str);
        }
        CheckNcStatus(nc_put_att_string(ncId, NC_GLOBAL, "labels_aggregated", subBasinLabelsNb, &subBasinLabels[0]));

        std::vector<const char *> hydroUnitLabels;
        for (const auto& label :m_hydroUnitLabels) {
            const char* str = (const char*)label.mb_str(wxConvUTF8);
            hydroUnitLabels.push_back(str);
        }
        CheckNcStatus(nc_put_att_string(ncId, NC_GLOBAL, "labels_distributed", hydroUnitLabelsNb, &hydroUnitLabels[0]));

        CheckNcStatus(nc_close(ncId));

    } catch(std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}