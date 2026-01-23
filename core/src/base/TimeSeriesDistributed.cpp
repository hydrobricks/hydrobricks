#include "TimeSeriesDistributed.h"

TimeSeriesDistributed::TimeSeriesDistributed(VariableType type)
    : TimeSeries(type) {}

TimeSeriesDistributed::~TimeSeriesDistributed() = default;  // Automatic cleanup via unique_ptr

void TimeSeriesDistributed::AddData(std::unique_ptr<TimeSeriesData> data, int unitId) {
    wxASSERT(data);
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
    wxASSERT(!_data.empty());
    return _data[0]->GetStart();
}

double TimeSeriesDistributed::GetEnd() const {
    wxASSERT(!_data.empty());
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
    wxASSERT(_data.size() == _unitIds.size());

    for (int i = 0; i < _data.size(); ++i) {
        if (_unitIds[i] == unitId) {
            return _data[i].get();  // Extract raw pointer from unique_ptr
        }
    }

    throw ShouldNotHappen(wxString::Format("TimeSeriesDistributed::GetDataPointer - Unit ID %d not found", unitId));
}
