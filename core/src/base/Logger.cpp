#include "Logger.h"

#include "FileNetcdf.h"

Logger::Logger()
    : m_cursor(0) {}

void Logger::InitContainer(int timeSize, const vecInt& hydroUnitIds, vecDouble hydroUnitAreas,
                           const vecStr& subBasinLabels, const vecStr& hydroUnitLabels) {
    m_time.resize(timeSize);
    m_subBasinLabels = subBasinLabels;
    m_subBasinValues = vecAxd(subBasinLabels.size(), axd::Ones(timeSize) * NAN_D);
    m_subBasinValuesPt.resize(subBasinLabels.size());
    m_hydroUnitIds = hydroUnitIds;
    m_hydroUnitAreas = Eigen::Map<axd>(hydroUnitAreas.data(), hydroUnitAreas.size());
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Ones(timeSize, hydroUnitIds.size()) * NAN_D);
    m_hydroUnitValuesPt = std::vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitIds.size(), nullptr));
}

void Logger::Reset() {
    m_cursor = 0;
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

bool Logger::DumpOutputs(const string& path) {
    if (!wxDirExists(path)) {
        wxLogError(_("The directory %s could not be found."), path);
        return false;
    }

    try {
        string filePath = path;
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

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}

axd Logger::GetOutletDischarge() {
    for (int i = 0; i < m_subBasinLabels.size(); i++) {
        if (m_subBasinLabels[i] == "outlet") {
            return m_subBasinValues[i];
        }
    }
    throw ConceptionIssue(_("No 'outlet' component found in logger."));
}

vecInt Logger::GetIndicesForSubBasinElements(const string &item) {
    vecInt indices;
    for (int i = 0; i < m_subBasinLabels.size(); ++i) {
        std::size_t found = m_subBasinLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

vecInt Logger::GetIndicesForHydroUnitElements(const string &item) {
    vecInt indices;
    for (int i = 0; i < m_hydroUnitLabels.size(); ++i) {
        std::size_t found = m_hydroUnitLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

double Logger::GetTotalSubBasin(const string &item) {
    vecInt indices = GetIndicesForSubBasinElements(item);
    double sum = 0;
    for (int index: indices) {
        sum += m_subBasinValues[index].sum();
    }

    return sum;
}

double Logger::GetTotalHydroUnits(const string &item) {
    vecInt indices = GetIndicesForHydroUnitElements(item);

    double sum = 0;
    for (int index : indices) {
        axd values = m_hydroUnitValues[index](Eigen::all, Eigen::all);
        axxd areas = m_hydroUnitAreas.replicate(values.size(), 1);
        sum += (values * areas).sum() / m_hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetTotalOutletDischarge() {
    return GetTotalSubBasin("outlet");
}

double Logger::GetTotalET() {
    return GetTotalHydroUnits("et:output");
}

double Logger::GetSubBasinInitialStorageState(){
    vecInt indices = GetIndicesForSubBasinElements(":content");
    double sum = 0;
    for (int index: indices) {
        sum += m_subBasinValues[index].head(1)[0];
    }

    return sum;
}

double Logger::GetSubBasinFinalStorageState(){
    vecInt indices = GetIndicesForSubBasinElements(":content");
    double sum = 0;
    for (int index: indices) {
        sum += m_subBasinValues[index].tail(1)[0];
    }

    return sum;
}

double Logger::GetHydroUnitsInitialStorageState(){
    vecInt indices = GetIndicesForHydroUnitElements(":content");
    double sum = 0;
    for (int index: indices) {
        axd values = m_hydroUnitValues[index](0, Eigen::all);
        sum += (values * m_hydroUnitAreas).sum() / m_hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetHydroUnitsFinalStorageState(){
    vecInt indices = GetIndicesForHydroUnitElements(":content");
    double sum = 0;
    for (int index: indices) {
        axd values = m_hydroUnitValues[index](Eigen::last, Eigen::all);
        sum += (values * m_hydroUnitAreas).sum() / m_hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetTotalStorageChanges(){
    return GetHydroUnitsInitialStorageState() - GetSubBasinInitialStorageState() +
           GetHydroUnitsFinalStorageState() - GetHydroUnitsInitialStorageState();
}