#include "Logger.h"

#include <cmath>

#include "ResultWriter.h"

namespace {
// Omitted (unit, label) cells are NaN (a label absent from a unit's structure
// variant). For water-balance totals such a unit contributes nothing, so NaN is
// treated as 0 while the full catchment area stays the denominator.
double NanToZero(double v) {
    return std::isnan(v) ? 0.0 : v;
}
}  // namespace

Logger::Logger()
    : _cursor(0),
      _recordFractions(false),
      _outletIndex(0) {}

void Logger::InitContainers(int timeSize, const std::vector<SubBasin*>& subbasins, SettingsModel& modelSettings) {
    assert(!subbasins.empty());

    // Sub basins in processing order; the hydro units follow, sub basin after sub basin.
    int subbasinCount = static_cast<int>(subbasins.size());
    _subbasinIds.clear();
    _subbasinDownstreamIds.clear();
    _subbasinLocalAreas.resize(subbasinCount);
    _subbasinDrainedAreas.resize(subbasinCount);
    _subbasinWeights.resize(subbasinCount);
    _outletIndex = 0;
    vecInt hydroUnitIds;
    vecInt hydroUnitStructureIds;
    vecDouble hydroUnitAreas;
    double totalArea = 0;
    for (int i = 0; i < subbasinCount; ++i) {
        SubBasin* subbasin = subbasins[i];
        _subbasinIds.push_back(subbasin->GetId());
        _subbasinDownstreamIds.push_back(subbasin->GetDownstreamId());
        _subbasinLocalAreas[i] = subbasin->GetLocalArea();
        // A sub basin outside a network (built directly) has no drained area set: it is its own.
        _subbasinDrainedAreas[i] = subbasin->GetDrainedArea() > 0 ? subbasin->GetDrainedArea()
                                                                  : subbasin->GetLocalArea();
        totalArea += subbasin->GetLocalArea();
        if (subbasin->IsTerminal()) {
            _outletIndex = i;
        }
        vecInt ids = subbasin->GetHydroUnitIds();
        vecInt structureIds = subbasin->GetHydroUnitStructureIds();
        vecDouble areas = subbasin->GetHydroUnitAreas();
        hydroUnitIds.insert(hydroUnitIds.end(), ids.begin(), ids.end());
        hydroUnitStructureIds.insert(hydroUnitStructureIds.end(), structureIds.begin(), structureIds.end());
        hydroUnitAreas.insert(hydroUnitAreas.end(), areas.begin(), areas.end());
    }
    // A single sub basin gets the weight 1.0 exactly (x / x), so its totals are unchanged.
    for (int i = 0; i < subbasinCount; ++i) {
        _subbasinWeights[i] = _subbasinLocalAreas[i] / totalArea;
    }

    vecStr subBasinLabels = modelSettings.GetSubBasinLogLabels();
    vecStr hydroUnitLabels = modelSettings.GetHydroUnitLogLabels();
    _time.resize(timeSize);
    _subBasinLabels = subBasinLabels;
    _subBasinInitialValues = vecAxd(subBasinLabels.size(), axd::Ones(subbasinCount) * NAN_D);
    _subBasinValues = vecAxxd(subBasinLabels.size(), axxd::Ones(timeSize, subbasinCount) * NAN_D);
    _subBasinValuesPt = vector<vecDoublePt>(subBasinLabels.size(), vecDoublePt(subbasinCount, nullptr));
    _hydroUnitIds = hydroUnitIds;
    _hydroUnitStructureIds = hydroUnitStructureIds;
    _hydroUnitAreas = Eigen::Map<axd>(hydroUnitAreas.data(), hydroUnitAreas.size());
    _hydroUnitLabels = hydroUnitLabels;
    _hydroUnitInitialValues = vecAxd(hydroUnitLabels.size(), axd::Ones(hydroUnitIds.size()) * NAN_D);
    _hydroUnitValues = vecAxxd(hydroUnitLabels.size(), axxd::Ones(timeSize, hydroUnitIds.size()) * NAN_D);
    _hydroUnitValuesPt = vector<vecDoublePt>(hydroUnitLabels.size(), vecDoublePt(hydroUnitIds.size(), nullptr));
    _subBasinEtIndices.clear();
    _hydroUnitEtIndices.clear();
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

void Logger::SetSubBasinValuePointer(int iSubbasin, int iLabel, double* valPt) {
    assert(_subBasinValuesPt.size() > iLabel);
    assert(_subBasinValuesPt[iLabel].size() > iSubbasin);
    _subBasinValuesPt[iLabel][iSubbasin] = valPt;
}

void Logger::SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt) {
    assert(_hydroUnitValuesPt.size() > iLabel);
    assert(_hydroUnitValuesPt[iLabel].size() > iUnit);
    _hydroUnitValuesPt[iLabel][iUnit] = valPt;
}

void Logger::SetHydroUnitFractionPointer(int iUnit, int iLabel, double* valPt) {
    if (_recordFractions) {
        assert(_hydroUnitFractionsPt.size() > iLabel);
        assert(_hydroUnitFractionsPt[iLabel].size() > iUnit);
        _hydroUnitFractionsPt[iLabel][iUnit] = valPt;
    }
}

void Logger::AddSubBasinEtIndex(int iLabel) {
    if (std::find(_subBasinEtIndices.begin(), _subBasinEtIndices.end(), iLabel) == _subBasinEtIndices.end()) {
        _subBasinEtIndices.push_back(iLabel);
    }
}

void Logger::SetDate(double date) {
    assert(_cursor < _time.size());
    _time[_cursor] = date;
}

void Logger::SaveInitialValues() {
    for (int iLabel = 0; iLabel < static_cast<int>(_subBasinValuesPt.size()); ++iLabel) {
        for (int iSubbasin = 0; iSubbasin < static_cast<int>(_subBasinValuesPt[iLabel].size()); ++iSubbasin) {
            // A label absent from a sub basin is left unconnected (NaN).
            if (_subBasinValuesPt[iLabel][iSubbasin] != nullptr) {
                _subBasinInitialValues[iLabel](iSubbasin) = *_subBasinValuesPt[iLabel][iSubbasin];
            }
        }
    }

    for (int iUnitVal = 0; iUnitVal < _hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < _hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            // A label absent from a unit's structure variant is left unconnected
            // (NaN); skip it so the initial value stays NaN.
            if (_hydroUnitValuesPt[iUnitVal][iUnit] != nullptr) {
                _hydroUnitInitialValues[iUnitVal](iUnit) = *_hydroUnitValuesPt[iUnitVal][iUnit];
            }
        }
    }
}

void Logger::Record() {
    assert(_cursor < _time.size());

    assert(_subBasinValues.size() == _subBasinValuesPt.size());
    for (int iLabel = 0; iLabel < static_cast<int>(_subBasinValues.size()); ++iLabel) {
        for (int iSubbasin = 0; iSubbasin < _subBasinValues[iLabel].cols(); ++iSubbasin) {
            if (_subBasinValuesPt[iLabel][iSubbasin] != nullptr) {
                _subBasinValues[iLabel](_cursor, iSubbasin) = *_subBasinValuesPt[iLabel][iSubbasin];
            }
        }
    }

    for (int iUnitVal = 0; iUnitVal < _hydroUnitValuesPt.size(); ++iUnitVal) {
        for (int iUnit = 0; iUnit < _hydroUnitValues[iUnitVal].cols(); ++iUnit) {
            // Unconnected (unit, label) pairs — a label not in this unit's structure
            // variant — stay NaN (omitted).
            if (_hydroUnitValuesPt[iUnitVal][iUnit] != nullptr) {
                _hydroUnitValues[iUnitVal](_cursor, iUnit) = *_hydroUnitValuesPt[iUnitVal][iUnit];
            }
        }
    }

    if (_recordFractions) {
        for (int iUnitVal = 0; iUnitVal < _hydroUnitFractionsPt.size(); ++iUnitVal) {
            for (int iUnit = 0; iUnit < _hydroUnitFractions[iUnitVal].cols(); ++iUnit) {
                if (_hydroUnitFractionsPt[iUnitVal][iUnit] != nullptr) {
                    _hydroUnitFractions[iUnitVal](_cursor, iUnit) = *_hydroUnitFractionsPt[iUnitVal][iUnit];
                }
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

    return writer.WriteNetCDF(path, _time, _subbasinIds, _subbasinDownstreamIds, _subbasinLocalAreas,
                              _subbasinDrainedAreas, _hydroUnitIds, _hydroUnitStructureIds, _hydroUnitAreas,
                              _subBasinLabels, _subBasinValues, _hydroUnitLabels, _hydroUnitValues,
                              _hydroUnitFractionLabels, _hydroUnitFractions);
}

int Logger::GetOutletLabelIndex() const {
    assert(_subBasinLabels.size() == _subBasinValues.size());
    for (int i = 0; i < static_cast<int>(_subBasinLabels.size()); ++i) {
        if (_subBasinLabels[i] == "outlet") {
            return i;
        }
    }
    throw ModelConfigError("No 'outlet' component found in logger.");
}

int Logger::GetSubbasinIndex(int subbasinId) const {
    for (int i = 0; i < static_cast<int>(_subbasinIds.size()); ++i) {
        if (_subbasinIds[i] == subbasinId) {
            return i;
        }
    }
    throw ModelConfigError(std::format("No subbasin with the ID {} was found in the logger.", subbasinId));
}

vecAxd Logger::GetSubBasinValues() const {
    vecAxd values;
    values.reserve(_subBasinValues.size());
    for (const auto& matrix : _subBasinValues) {
        values.push_back(matrix.col(_outletIndex));
    }
    return values;
}

axd Logger::GetOutletDischarge() const {
    return _subBasinValues[GetOutletLabelIndex()].col(_outletIndex);
}

axd Logger::GetSubbasinDischarge(int subbasinId) const {
    return _subBasinValues[GetOutletLabelIndex()].col(GetSubbasinIndex(subbasinId));
}

vecInt Logger::GetIndicesForSubBasinElements(const string& item) const {
    vecInt indices;
    for (int i = 0; i < _subBasinLabels.size(); ++i) {
        size_t found = _subBasinLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

vecInt Logger::GetIndicesForHydroUnitElements(const string& item) const {
    vecInt indices;
    for (int i = 0; i < _hydroUnitLabels.size(); ++i) {
        size_t found = _hydroUnitLabels[i].find(item);
        if (found != std::string::npos) {
            indices.push_back(i);
        }
    }

    return indices;
}

double Logger::WeightedTotal(int iLabel) const {
    // Sub basin values are in mm over each sub basin's local area: weight them by the local area share to
    // express the sum over the catchment area. The weight is exactly 1.0 for a single sub basin.
    double sum = 0;
    const axxd& values = _subBasinValues[iLabel];
    for (int iSubbasin = 0; iSubbasin < values.cols(); ++iSubbasin) {
        sum += values.col(iSubbasin).unaryExpr(&NanToZero).sum() * _subbasinWeights[iSubbasin];
    }

    return sum;
}

double Logger::GetTotalSubBasin(const string& item) const {
    vecInt indices = GetIndicesForSubBasinElements(item);
    double sum = 0;
    for (int index : indices) {
        sum += WeightedTotal(index);
    }

    return sum;
}

double Logger::GetTotalHydroUnits(const string& item, bool needsAreaWeighting) const {
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
            axxd values = fraction * _hydroUnitValues[i].unaryExpr(&NanToZero);
            sum += (values * areas).sum() / areasSum;
        }
    } else {
        // Not a storage content: fraction is already accounted for.
        if (needsAreaWeighting) {
            // Precompute areas matrix once (assuming all values have the same number of rows)
            axxd areas = _hydroUnitAreas.transpose().replicate(_hydroUnitValues[0].rows(), 1);
            double areasSum = _hydroUnitAreas.sum();

            for (int i : indices) {
                axxd values = _hydroUnitValues[i].unaryExpr(&NanToZero);
                sum += (values * areas).sum() / areasSum;
            }
        } else {
            for (int i : indices) {
                sum += _hydroUnitValues[i].unaryExpr(&NanToZero).sum();
            }
        }
    }

    return sum;
}

double Logger::GetTotalOutletDischarge() const {
    // The outlet discharge is already expressed over the drained area of the terminal sub basin, which is the
    // catchment: no weighting.
    return _subBasinValues[GetOutletLabelIndex()].col(_outletIndex).unaryExpr(&NanToZero).sum();
}

double Logger::GetTotalET() const {
    // ET fluxes are identified by the to-atmosphere tag recorded during model building,
    // not by label matching: process names (e.g. "interception") need not contain "et".
    double sum = 0;

    // Sub-basin ET: in mm over each sub basin's local area, weighted to the catchment.
    for (int i : _subBasinEtIndices) {
        sum += WeightedTotal(i);
    }

    // Hydro unit ET: area-weighted basin average. ET on a full-unit brick (e.g. the soil
    // moisture) carries no land-cover fraction; ET on a per-cover surface component (e.g. a
    // forest canopy) must be weighted by that cover's (time-varying) fraction so it scales
    // to the basin like the matching storage change does.
    if (!_hydroUnitEtIndices.empty()) {
        axxd areas = _hydroUnitAreas.transpose().replicate(_hydroUnitValues[0].rows(), 1);
        double areasSum = _hydroUnitAreas.sum();
        for (int i : _hydroUnitEtIndices) {
            axxd values = _hydroUnitValues[i].unaryExpr(&NanToZero);
            int fractionIndex = GetFractionIndexForComponent(_hydroUnitLabels[i]);
            if (fractionIndex >= 0) {
                // A cover absent from a unit has a NaN fraction there; zero it so the
                // already-zeroed value contributes nothing (NaN * 0 would be NaN).
                values *= _hydroUnitFractions[fractionIndex].unaryExpr(&NanToZero);
            }
            sum += (values * areas).sum() / areasSum;
        }
    }

    return sum;
}

int Logger::GetFractionIndexForComponent(const string& componentName) const {
    for (int j = 0; j < static_cast<int>(_hydroUnitFractionLabels.size()); ++j) {
        const string& fractionLabel = _hydroUnitFractionLabels[j];
        if (componentName.starts_with(fractionLabel + ":") || componentName.starts_with(fractionLabel + "_snowpack:") ||
            componentName.starts_with(fractionLabel + "_canopy:")) {
            return j;
        }
    }
    return -1;
}

double Logger::GetSubBasinInitialStorageState(const string& tag) const {
    vecInt indices = GetIndicesForSubBasinElements(tag);
    double sum = 0;
    for (int index : indices) {
        sum += (_subBasinInitialValues[index].unaryExpr(&NanToZero) * _subbasinWeights).sum();
    }

    return sum;
}

double Logger::GetSubBasinFinalStorageState(const string& tag) const {
    vecInt indices = GetIndicesForSubBasinElements(tag);
    double sum = 0;
    for (int index : indices) {
        axd last = _subBasinValues[index](Eigen::placeholders::last, Eigen::placeholders::all).unaryExpr(&NanToZero);
        sum += (last * _subbasinWeights).sum();
    }

    return sum;
}

double Logger::GetHydroUnitsInitialStorageState(const string& tag) const {
    vecInt indices = GetIndicesForHydroUnitElements(tag);
    double sum = 0;
    for (int i : indices) {
        axd fraction = axd::Ones(_hydroUnitInitialValues[i].size());
        int fractionIndex = GetFractionIndexForComponent(_hydroUnitLabels[i]);
        if (fractionIndex >= 0) {
            // A cover absent from a unit has a NaN fraction there; zero it (NaN * 0 = NaN).
            fraction = _hydroUnitFractions[fractionIndex](0, Eigen::placeholders::all).unaryExpr(&NanToZero);
        }
        axd values = _hydroUnitInitialValues[i].unaryExpr(&NanToZero);
        values *= fraction;
        sum += (values * _hydroUnitAreas).sum() / _hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetHydroUnitsFinalStorageState(const string& tag) const {
    vecInt indices = GetIndicesForHydroUnitElements(tag);
    double sum = 0;
    for (int i : indices) {
        axd fraction = axd::Ones(_hydroUnitValues[i].cols());
        int fractionIndex = GetFractionIndexForComponent(_hydroUnitLabels[i]);
        if (fractionIndex >= 0) {
            // A cover absent from a unit has a NaN fraction there; zero it (NaN * 0 = NaN).
            fraction = _hydroUnitFractions[fractionIndex](Eigen::placeholders::last, Eigen::placeholders::all)
                           .unaryExpr(&NanToZero);
        }
        axd values = _hydroUnitValues[i](Eigen::placeholders::last, Eigen::placeholders::all).unaryExpr(&NanToZero);
        values *= fraction;
        sum += (values * _hydroUnitAreas).sum() / _hydroUnitAreas.sum();
    }

    return sum;
}

double Logger::GetTotalWaterStorageChanges() const {
    return GetSubBasinFinalStorageState(":water_content") - GetSubBasinInitialStorageState(":water_content") +
           GetHydroUnitsFinalStorageState(":water_content") - GetHydroUnitsInitialStorageState(":water_content");
}

double Logger::GetTotalSnowStorageChanges() const {
    return GetSubBasinFinalStorageState(":snow_content") - GetSubBasinInitialStorageState(":snow_content") +
           GetHydroUnitsFinalStorageState(":snow_content") - GetHydroUnitsInitialStorageState(":snow_content");
}

double Logger::GetTotalGlacierStorageChanges() const {
    return GetSubBasinFinalStorageState(":ice_content") - GetSubBasinInitialStorageState(":ice_content") +
           GetHydroUnitsFinalStorageState(":ice_content") - GetHydroUnitsInitialStorageState(":ice_content");
}
