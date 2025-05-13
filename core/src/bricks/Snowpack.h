#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Includes.h"
#include "SnowContainer.h"
#include "SurfaceComponent.h"

class Snowpack : public SurfaceComponent {
  public:
    Snowpack();

    void Reset() override;

    void SaveAsInitialState() override;

    /**
     * @copydoc Brick::SetParameters()
     */
    void SetParameters(const BrickSettings& brickSettings) override;

    void AttachFluxIn(Flux* flux) override;

    bool IsOk() override;

    WaterContainer* GetSnowContainer();

    bool IsSnowpack() override {
        return true;
    }

    void Finalize() override;

    void SetInitialState(double value, const string& type) override;

    double GetContent(const string& type) override;

    void UpdateContent(double value, const string& type) override;

    void UpdateContentFromInputs() override;

    void ApplyConstraints(double timeStep) override;

    vecDoublePt GetDynamicContentChanges() override;

    double* GetValuePointer(const string& name) override;

    bool HasSnow();

  protected:
    SnowContainer* m_snow;

  private:
};

#endif  // HYDROBRICKS_SNOWPACK_H
