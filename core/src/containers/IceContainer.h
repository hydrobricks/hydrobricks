#ifndef HYDROBRICKS_ICE_CONTAINER_H
#define HYDROBRICKS_ICE_CONTAINER_H

#include "Includes.h"
#include "Snowpack.h"
#include "WaterContainer.h"

class Brick;

class IceContainer : public WaterContainer {
  public:
    IceContainer(Brick* brick);

    /**
     * @copydoc WaterContainer::ApplyConstraints()
     */
    void ApplyConstraints(double timeStep) override;

    /**
     * Set the option to not melt when snow cover is present.
     *
     * @param value Pointer to a float value. If the value is greater than 0, melting is disabled when snow cover is present.
     */
    void SetNoMeltWhenSnowCover(const float* value) {
        wxASSERT(value);
        m_noMeltWhenSnowCover = *value > 0;
    }

    /**
     * Set the related snowpack.
     *
     * @param snowpack Pointer to a Snowpack object. This is used to check if there is snow cover present.
     */
    void SetRelatedSnowpack(Snowpack* snowpack) {
        wxASSERT(snowpack);
        m_relatedSnowpack = snowpack;
    }

    /**
     * @copydoc WaterContainer::ContentAccessible()
     */
    bool ContentAccessible() const override;

  private:
    bool m_noMeltWhenSnowCover;
    Snowpack* m_relatedSnowpack;
};

#endif  // HYDROBRICKS_ICE_CONTAINER_H
