#include "TimeSeriesDistributed.h"

TimeSeriesDistributed::TimeSeriesDistributed(VariableType type)
    : TimeSeries(type) {}

TimeSeriesDistributed::~TimeSeriesDistributed() = default;  // Automatic cleanup via unique_ptr

void TimeSeriesDistributed::AddData(std::unique_ptr<TimeSeriesData> data, int unitId) {
    assert(data);
    _data.push_back(std::move(data));
    _unitIds.push_back(unitId);
}

bool TimeSeriesDistributed::SetCursorToDate(double date) {
    for (const auto& data : _data) {
        if (!data->SetCursorToDate(date)) {
            return false;
        }
    }

    return true;
}

bool TimeSeriesDistributed::AdvanceOneTimeStep() {
    for (const auto& data : _data) {
        if (!data->AdvanceOneTimeStep()) {
            return false;
        }
    }

    return true;
}

double TimeSeriesDistributed::GetStart() const {
    assert(!_data.empty());
    return _data[0]->GetStart();
}

double TimeSeriesDistributed::GetEnd() const {
    assert(!_data.empty());
    return _data[0]->GetEnd();
}

double TimeSeriesDistributed::GetTotal(const SettingsBasin* basinSettings) {
    double total = 0;
    double areaTotal = basinSettings->GetTotalArea();
    for (int i = 0; i < basinSettings->GetHydroUnitCount(); ++i) {
        double area = basinSettings->GetHydroUnitSettings(i).area;
        int id = basinSettings->GetHydroUnitSettings(i).id;
        double sumUnit = GetDataPointer(id)->GetSum();
        total += sumUnit * area / areaTotal;
    }

    return total;
}

TimeSeriesData* TimeSeriesDistributed::GetDataPointer(int unitId) {
    assert(_data.size() == _unitIds.size());

    for (int i = 0; i < _data.size(); ++i) {
        if (_unitIds[i] == unitId) {
            return _data[i].get();  // Extract raw pointer from unique_ptr
        }
    }

    throw ShouldNotHappen(std::format("TimeSeriesDistributed::GetDataPointer - Unit ID {} not found", unitId));
}

bool TimeSeriesDistributed::IsValid() const {
    // Check that data has been added
    if (_data.empty()) {
        LogError("TimeSeriesDistributed: No data added.");
        return false;
    }

    // Check that unit IDs and data vectors match in size
    if (_data.size() != _unitIds.size()) {
        LogError("TimeSeriesDistributed: Data count ({}) does not match unit ID count ({}).",
                 static_cast<int>(_data.size()), static_cast<int>(_unitIds.size()));
        return false;
    }

    // Check that all data pointers are valid
    for (size_t i = 0; i < _data.size(); ++i) {
        if (!_data[i]) {
            LogError("TimeSeriesDistributed: Data at index {} is null.", static_cast<int>(i));
            return false;
        }
    }

    // Check that all data elements are valid
    for (size_t i = 0; i < _data.size(); ++i) {
        if (!_data[i]->IsValid()) {
            LogError("TimeSeriesDistributed: Data at index {} (unit ID {}) is not valid.", static_cast<int>(i),
                     _unitIds[i]);
            return false;
        }
    }

    // Check for duplicate unit IDs
    for (size_t i = 0; i < _unitIds.size(); ++i) {
        for (size_t j = i + 1; j < _unitIds.size(); ++j) {
            if (_unitIds[i] == _unitIds[j]) {
                LogError("TimeSeriesDistributed: Duplicate unit ID {} found at indices {} and {}.", _unitIds[i],
                         static_cast<int>(i), static_cast<int>(j));
                return false;
            }
        }
    }

    return true;
}

void TimeSeriesDistributed::Validate() const {
    if (!IsValid()) {
        string msg = std::format("TimeSeriesDistributed validation failed. DataCount: {}, UnitIdCount: {}",
                                 static_cast<int>(_data.size()), static_cast<int>(_unitIds.size()));
        throw ModelConfigError(msg);
    }

    // Cascade validation to all data elements
    for (size_t i = 0; i < _data.size(); ++i) {
        try {
            _data[i]->Validate();
        } catch (const ModelConfigError& e) {
            string msg = std::format("TimeSeriesDistributed validation failed for unit ID {}: {}", _unitIds[i],
                                     e.what());
            throw ModelConfigError(msg);
        }
    }
}
