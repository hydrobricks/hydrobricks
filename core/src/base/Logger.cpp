#include "Logger.h"

#include "FileNetcdf.h"

Logger::Logger() : m_cursor(0) {}

void Logger::InitContainer(int timeSize, const vecInt &hydroUnitsIds, const vecStr &subBasinLabels,
                           const vecStr &hydroUnitLabels) {
    m_time.resize(timeSize);
    m_subBasinLabels = subBasinLabels;
    m_subBasinValues = vecAxd(subBasinLabels.size(), axd::Ones(timeSize) * NAN_D);
    m_subBasinValuesPt.resize(subBasinLabels.size());
    m_hydroUnitIds = hydroUnitsIds;
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Ones(timeSize, hydroUnitsIds.size()) * NAN_D);
    m_hydroUnitValuesPt = std::vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitsIds.size(), nullptr));
}

void Logger::SetSubBasinValuePointer(int iLabel, double *valPt) {
    wxASSERT(m_subBasinValuesPt.size() > iLabel);
    m_subBasinValuesPt[iLabel] = valPt;
}

void Logger::SetHydroUnitValuePointer(int iUnit, int iLabel, double *valPt) {
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

bool Logger::DumpOutputs(const std::string &path) {
    if (!wxDirExists(path)) {
        wxLogError(_("The directory %s could not be found."), path);
        return false;
    }

    try {
        std::string filePath = path;
        filePath.append(wxString(wxFileName::GetPathSeparator()).c_str());
        filePath.append("/results.nc");

        FileNetcdf file;

        if (!file.Create(filePath)) {
            return false;
        }

        // Create dimensions
        int dimIdTime = file.DefDim("time", (int)m_time.size());
        int dimIdUnit = file.DefDim("hydro_units", (int)m_hydroUnitIds.size());
        int dimIdItemsAgg = file.DefDim("aggregated_values", (int)m_subBasinLabels.size());
        int dimIdItemsDist = file.DefDim("distributed_values", (int)m_hydroUnitLabels.size());

        // Create variables and put data
        int varId = file.DefVarDouble("time", {dimIdTime});
        file.PutVar(varId, m_time);
        file.PutAttText("long_name", "time", varId);
        file.PutAttText("units", "days since 1858-11-17 00:00:00.0", varId);

        varId = file.DefVarInt("hydro_units_ids", {dimIdUnit});
        file.PutVar(varId, m_hydroUnitIds);
        file.PutAttText("long_name", "hydrological units ids", varId);

        varId = file.DefVarDouble("sub_basin_values", {dimIdItemsAgg, dimIdTime}, 2, true);
        file.PutVar(varId, m_subBasinValues);
        file.PutAttText("long_name", "aggregated values over the sub basin", varId);
        file.PutAttText("units", "mm", varId);

        varId = file.DefVarDouble("hydro_units_values", {dimIdItemsDist, dimIdUnit, dimIdTime}, 3, true);
        file.PutVar(varId, m_hydroUnitValues);
        file.PutAttText("long_name", "values for each hydrological units", varId);
        file.PutAttText("units", "mm", varId);

        // Global attributes
        file.PutAttString("labels_aggregated", m_subBasinLabels);
        file.PutAttString("labels_distributed", m_hydroUnitLabels);

    } catch (std::exception &e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}
