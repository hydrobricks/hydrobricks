#include "IceContainer.h"

#include "Brick.h"

IceContainer::IceContainer(Brick* brick)
    : WaterContainer(brick),
      m_noMeltWhenSnowCover(false),
      m_relatedSnowpack(nullptr) {}

void IceContainer::ApplyConstraints(double timeStep, bool inSolver) {
    if (m_noMeltWhenSnowCover) {
        if (m_relatedSnowpack == nullptr) {
            throw ConceptionIssue(_("No snowpack provided for the glacier melt limitation."));
        }
        if (m_relatedSnowpack->HasSnow()) {
            SetOutgoingRatesToZero();
        }
    }
    WaterContainer::ApplyConstraints(timeStep, inSolver);
}