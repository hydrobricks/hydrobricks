#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Includes.h"
#include "SurfaceComponent.h"

class Snowpack : public SurfaceComponent {
  public:
    Snowpack(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

    void AttachFluxIn(Flux* flux) override;

    WaterContainer* GetSnowContainer();

    bool IsSnowpack() override {
        return true;
    }

    void Finalize() override;

    void UpdateContentFromInputs() override;

    void ApplyConstraints(double timeStep) override;

    vecDoublePt GetStateVariableChanges() override;

    double* GetValuePointer(const wxString& name) override;

    bool HasSnow();

  protected:
    WaterContainer* m_snow;

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
