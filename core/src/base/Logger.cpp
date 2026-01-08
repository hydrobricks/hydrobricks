#include "Logger.h"

#include <wx/filename.h>

#include "ResultWriter.h"

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
    // Delegate output writing to ResultWriter
    ResultWriter writer;

    return writer.WriteNetCDF(path, _time, _hydroUnitIds, _hydroUnitAreas, _subBasinLabels, _subBasinValues,
                              _hydroUnitLabels, _hydroUnitValues, _hydroUnitFractionLabels, _hydroUnitFractions);
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
        // Precompute areas matrix once (assuming all values have the same number of rows)
        axxd areas = _hydroUnitAreas.transpose().replicate(_hydroUnitValues[0].rows(), 1);
        double areasSum = _hydroUnitAreas.sum();

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
            sum += (values * areas).sum() / areasSum;
        }
    } else {
        // Not a storage content: fraction is already accounted for.
        if (needsAreaWeighting) {
            // Precompute areas matrix once (assuming all values have the same number of rows)
            axxd areas = _hydroUnitAreas.transpose().replicate(_hydroUnitValues[0].rows(), 1);
            double areasSum = _hydroUnitAreas.sum();

            for (int i : indices) {
                axxd values = _hydroUnitValues[i];
                sum += (values * areas).sum() / areasSum;
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