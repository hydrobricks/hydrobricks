#ifndef HYDROBRICKS_SURFACE_CONTAINER_H
#define HYDROBRICKS_SURFACE_CONTAINER_H

#include "Brick.h"
#include "Includes.h"
#include "Snowpack.h"
#include "SplitterSnowRain.h"
#include "Surface.h"

class SurfaceContainer : public Brick {
  public:
    SurfaceContainer(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:
    std::vector<Brick*> m_components;
    std::vector<Surface*> m_surfaces;
    Snowpack* m_snowpack;
    SplitterSnowRain* m_snowRainSplitter;

  private:
};

#endif  // HYDROBRICKS_SURFACE_CONTAINER_H
