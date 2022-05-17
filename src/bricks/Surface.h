#ifndef HYDROBRICKS_SURFACE_H
#define HYDROBRICKS_SURFACE_H

#include "Brick.h"
#include "Includes.h"
#include "Snowpack.h"
#include "SplitterSnowRain.h"

class Surface : public Brick {
  public:
    Surface(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

  protected:
    std::vector<Brick*> m_components;
    Snowpack* m_snowpack;
    SplitterSnowRain* m_snowRainSplitter;

  private:
};

#endif  // HYDROBRICKS_SURFACE_H
