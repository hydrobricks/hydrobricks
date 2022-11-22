#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "IceContainer.h"
#include "Includes.h"
#include "LandCover.h"
#include "Snowpack.h"

class Glacier : public LandCover {
  public:
    Glacier();

    void Reset() override;

    void SaveAsInitialState() override;

    /**
     * @copydoc Brick::AssignParameters()
     */
    void AssignParameters(const BrickSettings& brickSettings) override;

    void AttachFluxIn(Flux* flux) override;

    bool IsOk() override;

    WaterContainer* GetIceContainer();

    bool IsGlacier() override {
        return true;
    }

    void Finalize() override;

    void UpdateContentFromInputs() override;

    void ApplyConstraints(double timeStep) override;

    vecDoublePt GetStateVariableChanges() override;

    double* GetValuePointer(const string& name) override;

    void SurfaceComponentAdded(SurfaceComponent* brick) override;

  protected:
    IceContainer* m_ice;

  private:
};

#endif  // HYDROBRICKS_GLACIER_H
