#include "IceContainer.h"

#include "Brick.h"

IceContainer::IceContainer(Brick* brick)
    : WaterContainer(brick),
      _noMeltWhenSnowCover(false),
      _relatedSnowpack(nullptr) {}

void IceContainer::ApplyConstraints(double timeStep) {
    if (_noMeltWhenSnowCover) {
        if (_relatedSnowpack == nullptr) {
            throw ConceptionIssue(_("No snowpack provided for the glacier melt limitation."));
        }
        if (_relatedSnowpack->HasSnow()) {
            SetOutgoingRatesToZero();
        }
    }
    WaterContainer::ApplyConstraints(timeStep);
}

bool IceContainer::ContentAccessible() const {
    if (_noMeltWhenSnowCover) {
        if (_relatedSnowpack == nullptr) {
            throw ConceptionIssue(_("No snowpack provided for the glacier melt limitation."));
        }
        if (_relatedSnowpack->HasSnow()) {
            return false;
        }
    }
    return WaterContainer::ContentAccessible();
}