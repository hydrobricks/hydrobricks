#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "SurfaceComponent.h"
#include "Includes.h"

class Glacier : public SurfaceComponent {
  public:
    Glacier(HydroUnit *hydroUnit);

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings &brickSettings) override;

    void AttachFluxIn(Flux* flux);

    WaterContainer* GetIceContainer();

    bool IsGlacier() override {
        return true;
    }

    void Finalize() override;

    void UpdateContentFromInputs() override;

    void ApplyConstraints(double timeStep) override;

    vecDoublePt GetStateVariableChanges() override;

    double* GetValuePointer(const wxString& name) override;

  protected:
    WaterContainer* m_ice;
    bool m_infiniteStorage;

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
