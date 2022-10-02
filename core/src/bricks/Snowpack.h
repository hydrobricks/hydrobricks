#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Includes.h"
#include "SurfaceComponent.h"

class Snowpack : public SurfaceComponent {
  public:
    Snowpack();

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings& brickSettings) override;

    void AttachFluxIn(Flux* flux) override;

    WaterContainer* GetSnowContainer();

    bool IsSnowpack() override {
        return true;
    }

    void Finalize() override;

    void UpdateContentFromInputs() override;

    void ApplyConstraints(double timeStep, bool inSolver = true) override;

    vecDoublePt GetStateVariableChanges() override;

    double* GetValuePointer(const std::string& name) override;

    bool HasSnow();

  protected:
    WaterContainer* m_snow;

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
