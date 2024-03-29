#ifndef HYDROBRICKS_ICE_CONTAINER_H
#define HYDROBRICKS_ICE_CONTAINER_H

#include "Includes.h"
#include "Snowpack.h"
#include "WaterContainer.h"

class Brick;

class IceContainer : public WaterContainer {
  public:
    IceContainer(Brick* brick);

    void ApplyConstraints(double timeStep) override;

    void SetNoMeltWhenSnowCover(const float* value) {
        wxASSERT(value);
        m_noMeltWhenSnowCover = *value > 0;
    }

    void SetRelatedSnowpack(Snowpack* snowpack) {
        wxASSERT(snowpack);
        m_relatedSnowpack = snowpack;
    }

    bool ContentAccessible() const override;

  protected:
  private:
    bool m_noMeltWhenSnowCover;
    Snowpack* m_relatedSnowpack;
};

#endif  // HYDROBRICKS_ICE_CONTAINER_H
