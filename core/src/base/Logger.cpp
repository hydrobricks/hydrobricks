#include "Logger.h"

#include "FileNetcdf.h"

Logger::Logger()
    : m_cursor(0),
      m_recordFractions(false) {}

void Logger::InitContainers(int timeSize, SubBasin* subBasin, SettingsModel& modelSettings) {
    vecInt hydroUnitIds = subBasin->GetHydroUnitIds();
    vecDouble hydroUnitAreas = subBasin->GetHydroUnitAreas();
    vecStr subBasinLabels = modelSettings.GetSubBasinLogLabels();
    vecStr hydroUnitLabels = modelSettings.GetHydroUnitLogLabels();
    m_time.resize(timeSize);
    m_subBasinLabels = subBasinLabels;
    m_subBasinInitialValues = axd::Ones(subBasinLabels.size()) * NAN_D;
    m_subBasinValues = vecAxd(subBasinLabels.size(), axd::Ones(timeSize) * NAN_D);
    m_subBasinValuesPt.resize(subBasinLabels.size());
    m_hydroUnitIds = hydroUnitIds;
    m_hydroUnitAreas = Eigen::Map<axd>(hydroUnitAreas.data(), hydroUnitAreas.size());
    m_hydroUnitLabels = hydroUnitLabels;
    m_hydroUnitInitialValues = vecAxd(hydroUnitLabels.size(), axd::Ones(hydroUnitIds.size()) * NAN_D);
    m_hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Ones(timeSize, hydroUnitIds.size()) * NAN_D);
    m_hydroUnitValuesPt = vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitIds.size(), nullptr));
    if (m_recordFractions) {
        m_hydroUnitFractionLabels = modelSettings.GetLandCoverBricksNames();
        m_hydroUnitFractions = vecAxxd(m_hydroUnitFractionLabels.size(),
                                       axxd::Ones(timeSize, hydroUnitIds.size()) * NAN_D);
        m_hydroUnitFractionsPt = vector<vecDoublePt>(m_hydroUnitFractionLabels.size(),
                                                     vecDoublePt(hydroUnitIds.size(), nullptr));
    }
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

void Logger::SetHydroUnitFractionPointer(int iUnit, int iLabel, double* valPt) {
    if (m_recordFractions) {
        wxASSERT(m_hydroUnitFractionsPt.size() > iLabel);
        wxASSERT(m_hydroUnitFractionsPt[iLabel].size() > iUnit);
        m_hydroUnitFractionsPt[iLabel][iUnit] = valPt;
    }
}

void Logger::SetDate(double date) {
    wxASSERT(m_cursor < m_time.size());
    m_time[m_cursor] = date;
}

void Logger::SaveInitialValues() {
    for (int iSubBasin = 0; iSubBasin < m_subBasinValuesPt.size(); ++iSubBasin) {
        wxASSERT(m_subBasinValuesPt[iSubBasin]);
        m_subBasinInitialValues[iSubBasin] = *m_subBasinValuesPt[iSubBasin];
    }

    for (int iUnitVal = 0; iUnitVal < m_hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < m_hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            wxASSERT(m_hydroUnitValuesPt[iUnitVal][iUnit]);
            m_hydroUnitInitialValues[iUnitVal](iUnit) = *m_hydroUnitValuesPt[iUnitVal][iUnit];
        }
    }
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

    if (m_recordFractions) {
        for (int iUnitVal = 0; iUnitVal < m_hydroUnitFractionsPt.size(); ++iUnitVal) {
            for (int iUnit = 0; iUnit < m_hydroUnitFractions[iUnitVal].cols(); ++iUnit) {
                wxASSERT(m_hydroUnitFractionsPt[iUnitVal][iUnit]);
                m_hydroUnitFractions[iUnitVal](m_cursor, iUnit) = *m_hydroUnitFractionsPt[iUnitVal][iUnit];
            }
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

    wxLogMessage(_("Writing output file."));

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
        int dimIdFractions = 0;
        if (m_recordFractions) {
            dimIdFractions = file.DefDim("land_covers", (int)m_hydroUnitFractionLabels.size());
        }

        // Create variables and put data
        int varId = file.DefVarDouble("time", {dimIdTime});
        file.PutVar(varId, m_time);
        file.PutAttText("long_name", "time", varId);
        file.PutAttText("units", "days since 1858-11-17 00:00:00.0", varId);

        varId = file.DefVarInt("hydro_units_ids", {dimIdUnit});
        file.PutVar(varId, m_hydroUnitIds);
        file.PutAttText("long_name", "hydrological units ids", varId);

        varId = file.DefVarDouble("hydro_units_areas", {dimIdUnit});
        file.PutVar(varId, m_hydroUnitAreas);
        file.PutAttText("long_name", "hydrological units areas", varId);

        varId = file.DefVarDouble("sub_basin_values", {dimIdItemsAgg, dimIdTime}, 2, true);
        file.PutVar(varId, m_subBasinValues);
        file.PutAttText("long_name", "aggregated values over the sub basin", varId);
        file.PutAttText("units", "mm", varId);

        varId = file.DefVarDouble("hydro_units_values", {dimIdItemsDist, dimIdUnit, dimIdTime}, 3, true);
        file.PutVar(varId, m_hydroUnitValues);
        file.PutAttText("long_name", "values for each hydrological units", varId);
        file.PutAttText("units", "mm", varId);

        if (m_recordFractions) {
            varId = file.DefVarDouble("land_cover_fractions", {dimIdFractions, dimIdUnit, dimIdTime}, 3, true);
            file.PutVar(varId, m_hydroUnitFractions);
            file.PutAttText("long_name", "land cover fractions for each hydrological units", varId);
            file.PutAttText("units", "percent", varId);
        }

        // Global attributes
        file.PutAttString("labels_aggregated", m_subBasinLabels);
        file.PutAttString("labels_distributed", m_hydroUnitLabels);
        if (m_recordFractions) {
            file.PutAttString("labels_land_covers", m_hydroUnitFractionLabels);
        }

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    wxLogMessage(_("Output file written."));

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

vecInt Logger::GetIndicesForSubBasinElements(const string& item) {
    vecInt indices;
    for (int i = 0; i < m_subBasinLabels.size(); ++i) {
        size_t found = m_subBasinLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

vecInt Logger::GetIndicesForHydroUnitElements(const string& item) {
    vecInt indices;
    for (int i = 0; i < m_hydroUnitLabels.size(); ++i) {
        size_t found = m_hydroUnitLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

double Logger::GetTotalSubBasin(const string& item) {
    vecInt indices = GetIndicesForSubBasinElements(item);
    double sum = 0;
    for (int index : indices) {
        sum += m_subBasinValues[index].sum();
    }

    return sum;
}

double Logger::GetTotalHydroUnits(const string& item, bool needsAreaWeighting) {
    vecInt indices = GetIndicesForHydroUnitElements(item);
    double sum = 0;
    size_t found = item.find(":content");
    if (found != std::string::npos) {
        // Storage content: fraction must be accounted for.
        for (int i : indices) {
            axxd fraction = axxd::Ones(m_hydroUnitValues[i].rows(), m_hydroUnitValues[i].cols());
            string componentName = m_hydroUnitLabels[i];
            for (int j = 0; j < m_hydroUnitFractionLabels.size(); ++j) {
                string fractionLabel = m_hydroUnitFractionLabels[j];
                if (componentName == fractionLabel + ":content") {
                    fraction = m_hydroUnitFractions[j];
                    break;
                }
            }
            axxd values = fraction * m_hydroUnitValues[i];
            axxd areas = m_hydroUnitAreas.transpose().replicate(values.rows(), 1);
            sum += (values * areas).sum() / m_hydroUnitAreas.sum();
        }
    } else {
        // Not a storage content: fraction is already accounted for.
        if (needsAreaWeighting) {
            for (int i : indices) {
                axxd values = m_hydroUnitValues[i];
                axxd areas = m_hydroUnitAreas.transpose().replicate(values.rows(), 1);
                sum += (values * areas).sum() / m_hydroUnitAreas.sum();
            }
        } else {
            for (int i : indices) {
                sum += m_hydroUnitValues[i].sum();
            }
        }
    }

    return sum;
}

double Logger::GetTotalOutletDischarge() {
    return GetTotalSubBasin("outlet");
}

double Logger::GetTotalET() {
    return GetTotalHydroUnits("et:output", true);
}

double Logger::GetSubBasinInitialStorageState(const string& tag) {
    vecInt indices = GetIndicesForSubBasinElements(tag);
    double sum = 0;
    for (int index : indices) {
        sum += m_subBasinInitialValues[index];
    }

    return sum;
}

double Logger::GetSubBasinFinalStorageState(const string& tag) {
    vecInt indices = GetIndicesForSubBasinElements(tag);
    double sum = 0;
    for (int index : indices) {
        sum += m_subBasinValues[index].tail(1)[0];
    }

    return sum;
}

double Logger::GetHydroUnitsInitialStorageState(const string& tag) {
    vecInt indices = GetIndicesForHydroUnitElements(tag);
    double sum = 0;
    for (int i : indices) {
        axd fraction = axd::Ones(m_hydroUnitInitialValues[i].size());
        string componentName = m_hydroUnitLabels[i];
        for (int j = 0; j < m_hydroUnitFractionLabels.size(); ++j) {
            string fractionLabel = m_hydroUnitFractionLabels[j];
            if (wxString(componentName).StartsWith(fractionLabel + ":")) {
                fraction = m_hydroUnitFractions[j](0, Eigen::all);
                break;
            }
        }
        axd values = m_hydroUnitInitialValues[i];
        values *= fraction;
        sum += (values * m_hydroUnitAreas).sum() / m_hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetHydroUnitsFinalStorageState(const string& tag) {
    vecInt indices = GetIndicesForHydroUnitElements(tag);
    double sum = 0;
    for (int i : indices) {
        axd fraction = axd::Ones(m_hydroUnitValues[i].cols());
        string componentName = m_hydroUnitLabels[i];
        for (int j = 0; j < m_hydroUnitFractionLabels.size(); ++j) {
            string fractionLabel = m_hydroUnitFractionLabels[j];
            if (wxString(componentName).StartsWith(fractionLabel + ":")) {
                fraction = m_hydroUnitFractions[j](Eigen::last, Eigen::all);
                break;
            }
        }
        axd values = m_hydroUnitValues[i](Eigen::last, Eigen::all);
        values *= fraction;
        sum += (values * m_hydroUnitAreas).sum() / m_hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetTotalWaterStorageChanges() {
    return GetSubBasinFinalStorageState(":content") - GetSubBasinInitialStorageState(":content") +
           GetHydroUnitsFinalStorageState(":content") - GetHydroUnitsInitialStorageState(":content");
}

double Logger::GetTotalSnowStorageChanges() {
    return GetHydroUnitsFinalStorageState(":snow") - GetHydroUnitsInitialStorageState(":snow");
}