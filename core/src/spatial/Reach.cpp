#include "Reach.h"

#include "SubBasin.h"

Reach::Reach(SubBasin* subbasin)
    : _subbasin(subbasin) {
    assert(subbasin);
}

void Reach::Initialize() {
    if (_subbasin->HasProperty("length")) {
        _length = _subbasin->GetPropertyDouble("length");
    }
    if (_subbasin->HasProperty("slope")) {
        _slope = _subbasin->GetPropertyDouble("slope");
    }
}

double Reach::Route(double inflowVolume, double /*timeStepInDays*/) {
    _inflow = inflowVolume;

    switch (_scheme) {
        case Scheme::None:
            // Instantaneous: what comes in leaves in the same step, nothing stays in transit.
            _outflow = _inflow;
            _storage = 0;
            break;
    }

    return _outflow;
}

void Reach::Reset() {
    _inflow = 0;
    _outflow = 0;
    _storage = _initialStorage;
}

void Reach::SaveAsInitialState() {
    _initialStorage = _storage;
}
