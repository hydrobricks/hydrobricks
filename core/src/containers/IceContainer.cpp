#include "IceContainer.h"

#include "Brick.h"

IceContainer::IceContainer(Brick* brick)
    : WaterContainer(brick),
      m_noMeltWhenSnowCover(false),
      m_relatedSnowpack(nullptr) {}

void IceContainer::ApplyConstraints(double timeStep) {
    if (m_noMeltWhenSnowCover) {
        if (m_relatedSnowpack == nullptr) {
            throw ConceptionIssue(_("No snowpack provided for the glacier melt limitation."));
        }
        if (m_relatedSnowpack->HasSnow()) {
            SetOutgoingRatesToZero();
        }
    }
    WaterContainer::ApplyConstraints(timeStep);
}

bool IceContainer::ContentAccessible() const {
    if (m_noMeltWhenSnowCover) {
        if (m_relatedSnowpack == nullptr) {
            throw ConceptionIssue(_("No snowpack provided for the glacier melt limitation."));
        }
        if (m_relatedSnowpack->HasSnow()) {
            return false;
        }
    }
    return WaterContainer::ContentAccessible();
}