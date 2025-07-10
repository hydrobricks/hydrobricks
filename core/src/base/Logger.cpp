#include "Logger.h"

#include <wx/filename.h>

#include "FileNetcdf.h"

Logger::Logger()
    : _cursor(0),
      _recordFractions(false) {}

void Logger::InitContainers(int timeSize, SubBasin* subBasin, SettingsModel& modelSettings) {
    vecInt hydroUnitIds = subBasin->GetHydroUnitIds();
    vecDouble hydroUnitAreas = subBasin->GetHydroUnitAreas();
    vecStr subBasinLabels = modelSettings.GetSubBasinLogLabels();
    vecStr hydroUnitLabels = modelSettings.GetHydroUnitLogLabels();
    _time.resize(timeSize);
    _subBasinLabels = subBasinLabels;
    _subBasinInitialValues = axd::Ones(subBasinLabels.size()) * NAN_D;
    _subBasinValues = vecAxd(subBasinLabels.size(), axd::Ones(timeSize) * NAN_D);
    _subBasinValuesPt.resize(subBasinLabels.size());
    _hydroUnitIds = hydroUnitIds;
    _hydroUnitAreas = Eigen::Map<axd>(hydroUnitAreas.data(), hydroUnitAreas.size());
    _hydroUnitLabels = hydroUnitLabels;
    _hydroUnitInitialValues = vecAxd(hydroUnitLabels.size(), axd::Ones(hydroUnitIds.size()) * NAN_D);
    _hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Ones(timeSize, hydroUnitIds.size()) * NAN_D);
    _hydroUnitValuesPt = vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitIds.size(), nullptr));
    if (_recordFractions) {
        _hydroUnitFractionLabels = modelSettings.GetLandCoverBricksNames();
        _hydroUnitFractions = vecAxxd(_hydroUnitFractionLabels.size(),
                                      axxd::Ones(timeSize, hydroUnitIds.size()) * NAN_D);
        _hydroUnitFractionsPt = vector<vecDoublePt>(_hydroUnitFractionLabels.size(),
                                                    vecDoublePt(hydroUnitIds.size(), nullptr));
    }
}

void Logger::Reset() {
    _cursor = 0;
}

void Logger::SetSubBasinValuePointer(int iLabel, double* valPt) {
    wxASSERT(_subBasinValuesPt.size() > iLabel);
    _subBasinValuesPt[iLabel] = valPt;
}

void Logger::SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt) {
    wxASSERT(_hydroUnitValuesPt.size() > iLabel);
    wxASSERT(_hydroUnitValuesPt[iLabel].size() > iUnit);
    _hydroUnitValuesPt[iLabel][iUnit] = valPt;
}

void Logger::SetHydroUnitFractionPointer(int iUnit, int iLabel, double* valPt) {
    if (_recordFractions) {
        wxASSERT(_hydroUnitFractionsPt.size() > iLabel);
        wxASSERT(_hydroUnitFractionsPt[iLabel].size() > iUnit);
        _hydroUnitFractionsPt[iLabel][iUnit] = valPt;
    }
}

void Logger::SetDate(double date) {
    wxASSERT(_cursor < _time.size());
    _time[_cursor] = date;
}

void Logger::SaveInitialValues() {
    for (int iSubBasin = 0; iSubBasin < _subBasinValuesPt.size(); ++iSubBasin) {
        wxASSERT(_subBasinValuesPt[iSubBasin]);
        _subBasinInitialValues[iSubBasin] = *_subBasinValuesPt[iSubBasin];
    }

    for (int iUnitVal = 0; iUnitVal < _hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < _hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            wxASSERT(_hydroUnitValuesPt[iUnitVal][iUnit]);
            _hydroUnitInitialValues[iUnitVal](iUnit) = *_hydroUnitValuesPt[iUnitVal][iUnit];
        }
    }
}

void Logger::Record() {
    wxASSERT(_cursor < _time.size());

    for (int iSubBasin = 0; iSubBasin < _subBasinValuesPt.size(); ++iSubBasin) {
        wxASSERT(_subBasinValuesPt[iSubBasin]);
        _subBasinValues[iSubBasin][_cursor] = *_subBasinValuesPt[iSubBasin];
    }

    for (int iUnitVal = 0; iUnitVal < _hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < _hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            wxASSERT(_hydroUnitValuesPt[iUnitVal][iUnit]);
            _hydroUnitValues[iUnitVal](_cursor, iUnit) = *_hydroUnitValuesPt[iUnitVal][iUnit];
        }
    }

    if (_recordFractions) {
        for (int iUnitVal = 0; iUnitVal < _hydroUnitFractionsPt.size(); ++iUnitVal) {
            for (int iUnit = 0; iUnit < _hydroUnitFractions[iUnitVal].cols(); ++iUnit) {
                wxASSERT(_hydroUnitFractionsPt[iUnitVal][iUnit]);
                _hydroUnitFractions[iUnitVal](_cursor, iUnit) = *_hydroUnitFractionsPt[iUnitVal][iUnit];
            }
        }
    }
}

void Logger::Increment() {
    _cursor++;
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
        int dimIdTime = file.DefDim("time", (int)_time.size());
        int dimIdUnit = file.DefDim("hydro_units", (int)_hydroUnitIds.size());
        int dimIdItemsAgg = file.DefDim("aggregated_values", (int)_subBasinLabels.size());
        int dimIdItemsDist = file.DefDim("distributed_values", (int)_hydroUnitLabels.size());
        int dimIdFractions = 0;
        if (_recordFractions) {
            dimIdFractions = file.DefDim("land_covers", (int)_hydroUnitFractionLabels.size());
        }

        // Create variables and put data
        int varId = file.DefVarDouble("time", {dimIdTime});
        file.PutVar(varId, _time);
        file.PutAttText("long_name", "time", varId);
        file.PutAttText("units", "days since 1858-11-17 00:00:00.0", varId);

        varId = file.DefVarInt("hydro_units_ids", {dimIdUnit});
        file.PutVar(varId, _hydroUnitIds);
        file.PutAttText("long_name", "hydrological units ids", varId);

        varId = file.DefVarDouble("hydro_units_areas", {dimIdUnit});
        file.PutVar(varId, _hydroUnitAreas);
        file.PutAttText("long_name", "hydrological units areas", varId);

        varId = file.DefVarDouble("sub_basin_values", {dimIdItemsAgg, dimIdTime}, 2, true);
        file.PutVar(varId, _subBasinValues);
        file.PutAttText("long_name", "aggregated values over the sub basin", varId);
        file.PutAttText("units", "mm", varId);

        varId = file.DefVarDouble("hydro_units_values", {dimIdItemsDist, dimIdUnit, dimIdTime}, 3, true);
        file.PutVar(varId, _hydroUnitValues);
        file.PutAttText("long_name", "values for each hydrological units", varId);
        file.PutAttText("units", "mm", varId);

        if (_recordFractions) {
            varId = file.DefVarDouble("land_cover_fractions", {dimIdFractions, dimIdUnit, dimIdTime}, 3, true);
            file.PutVar(varId, _hydroUnitFractions);
            file.PutAttText("long_name", "land cover fractions for each hydrological units", varId);
            file.PutAttText("units", "percent", varId);
        }

        // Global attributes
        file.PutAttString("labels_aggregated", _subBasinLabels);
        file.PutAttString("labels_distributed", _hydroUnitLabels);
        if (_recordFractions && !_hydroUnitFractionLabels.empty()) {
            file.PutAttString("labels_land_covers", _hydroUnitFractionLabels);
        }

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    wxLogMessage(_("Output file written."));

    return true;
}

axd Logger::GetOutletDischarge() {
    for (int i = 0; i < _subBasinLabels.size(); i++) {
        if (_subBasinLabels[i] == "outlet") {
            return _subBasinValues[i];
        }
    }
    throw ConceptionIssue(_("No 'outlet' component found in logger."));
}

vecInt Logger::GetIndicesForSubBasinElements(const string& item) {
    vecInt indices;
    for (int i = 0; i < _subBasinLabels.size(); ++i) {
        size_t found = _subBasinLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

vecInt Logger::GetIndicesForHydroUnitElements(const string& item) {
    vecInt indices;
    for (int i = 0; i < _hydroUnitLabels.size(); ++i) {
        size_t found = _hydroUnitLabels[i].find(item);
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
        sum += _subBasinValues[index].sum();
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
            axxd fraction = axxd::Ones(_hydroUnitValues[i].rows(), _hydroUnitValues[i].cols());
            string componentName = _hydroUnitLabels[i];
            for (int j = 0; j < _hydroUnitFractionLabels.size(); ++j) {
                string fractionLabel = _hydroUnitFractionLabels[j];
                if (componentName == fractionLabel + ":content") {
                    fraction = _hydroUnitFractions[j];
                    break;
                }
            }
            axxd values = fraction * _hydroUnitValues[i];
            axxd areas = _hydroUnitAreas.transpose().replicate(values.rows(), 1);
            sum += (values * areas).sum() / _hydroUnitAreas.sum();
        }
    } else {
        // Not a storage content: fraction is already accounted for.
        if (needsAreaWeighting) {
            for (int i : indices) {
                axxd values = _hydroUnitValues[i];
                axxd areas = _hydroUnitAreas.transpose().replicate(values.rows(), 1);
                sum += (values * areas).sum() / _hydroUnitAreas.sum();
            }
        } else {
            for (int i : indices) {
                sum += _hydroUnitValues[i].sum();
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
        sum += _subBasinInitialValues[index];
    }

    return sum;
}

double Logger::GetSubBasinFinalStorageState(const string& tag) {
    vecInt indices = GetIndicesForSubBasinElements(tag);
    double sum = 0;
    for (int index : indices) {
        sum += _subBasinValues[index].tail(1)[0];
    }

    return sum;
}

double Logger::GetHydroUnitsInitialStorageState(const string& tag) {
    vecInt indices = GetIndicesForHydroUnitElements(tag);
    double sum = 0;
    for (int i : indices) {
        axd fraction = axd::Ones(_hydroUnitInitialValues[i].size());
        string componentName = _hydroUnitLabels[i];
        for (int j = 0; j < _hydroUnitFractionLabels.size(); ++j) {
            string fractionLabel = _hydroUnitFractionLabels[j];
            if (wxString(componentName).StartsWith(fractionLabel + ":") ||
                wxString(componentName).StartsWith(fractionLabel + "_snowpack:")) {
                fraction = _hydroUnitFractions[j](0, Eigen::all);
                break;
            }
        }
        axd values = _hydroUnitInitialValues[i];
        values *= fraction;
        sum += (values * _hydroUnitAreas).sum() / _hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetHydroUnitsFinalStorageState(const string& tag) {
    vecInt indices = GetIndicesForHydroUnitElements(tag);
    double sum = 0;
    for (int i : indices) {
        axd fraction = axd::Ones(_hydroUnitValues[i].cols());
        string componentName = _hydroUnitLabels[i];
        for (int j = 0; j < _hydroUnitFractionLabels.size(); ++j) {
            string fractionLabel = _hydroUnitFractionLabels[j];
            if (wxString(componentName).StartsWith(fractionLabel + ":") ||
                wxString(componentName).StartsWith(fractionLabel + "_snowpack:")) {
                fraction = _hydroUnitFractions[j](Eigen::last, Eigen::all);
                break;
            }
        }
        axd values = _hydroUnitValues[i](Eigen::last, Eigen::all);
        values *= fraction;
        sum += (values * _hydroUnitAreas).sum() / _hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetTotalWaterStorageChanges() {
    return GetSubBasinFinalStorageState(":water_content") - GetSubBasinInitialStorageState(":water_content") +
           GetHydroUnitsFinalStorageState(":water_content") - GetHydroUnitsInitialStorageState(":water_content");
}

double Logger::GetTotalSnowStorageChanges() {
    return GetSubBasinFinalStorageState(":snow_content") - GetSubBasinInitialStorageState(":snow_content") +
           GetHydroUnitsFinalStorageState(":snow_content") - GetHydroUnitsInitialStorageState(":snow_content");
}

double Logger::GetTotalGlacierStorageChanges() {
    return GetSubBasinFinalStorageState(":ice_content") - GetSubBasinInitialStorageState(":ice_content") +
           GetHydroUnitsFinalStorageState(":ice_content") - GetHydroUnitsInitialStorageState(":ice_content");
}