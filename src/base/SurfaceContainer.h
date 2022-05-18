#ifndef HYDROBRICKS_SURFACE_CONTAINER_H
#define HYDROBRICKS_SURFACE_CONTAINER_H

#include "Includes.h"
#include "Brick.h"
#include "Snowpack.h"
#include "SplitterSnowRain.h"
#include "Surface.h"

class SurfaceContainer : public wxObject {
  public:
    explicit SurfaceContainer(HydroUnit* hydroUnit);

    ~SurfaceContainer() override;

  protected:
    HydroUnit* m_hydroUnit;
    std::vector<Brick*> m_components;
    std::vector<Surface*> m_surfaces;
    std::vector<Snowpack*> m_snowpacks;
    SplitterSnowRain* m_snowRainSplitter;

  private:
};

#endif  // HYDROBRICKS_SURFACE_CONTAINER_H
