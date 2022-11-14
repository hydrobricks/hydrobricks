#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "Includes.h"
#include "LandCover.h"
#include "Snowpack.h"

class Glacier : public LandCover {
  public:
    Glacier();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings& brickSettings) override;

    void AttachFluxIn(Flux* flux);

    WaterContainer* GetIceContainer();

    bool IsGlacier() override {
        return true;
    }

    void Finalize() override;

    void UpdateContentFromInputs() override;

    void ApplyConstraints(double timeStep, bool inSolver = true) override;

    vecDoublePt GetStateVariableChanges() override;

    double* GetValuePointer(const std::string& name) override;

    void SurfaceComponentAdded(SurfaceComponent* brick) override;

  protected:
    WaterContainer* m_ice;
    bool m_noMeltWhenSnowCover;
    Snowpack* m_snowpack;

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
