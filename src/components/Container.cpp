
#include "Container.h"
#include "HydroUnit.h"

Container::Container(HydroUnit *hydroUnit)
    : m_hydroUnit(hydroUnit)
{}

void Container::TransferVertically(double &volume, double &rejected) {
    rejected = volume;
}

void Container::TransferHorizontally(double &volume, double &rejected) {
    rejected = volume;
}