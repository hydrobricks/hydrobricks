#ifndef HYDROBRICKS_GLACIER_H
#define HYDROBRICKS_GLACIER_H

#include "IceContainer.h"
#include "Includes.h"
#include "LandCover.h"
#include "Snowpack.h"

class Glacier : public LandCover {
  public:
    Glacier();

    /**
     * @copydoc Brick::Reset()
     */
    void Reset() override;

    /**
     * @copydoc Brick::SaveAsInitialState()
     */
    void SaveAsInitialState() override;

    /**
     * @copydoc Brick::SetParameters()
     */
    void SetParameters(const BrickSettings& brickSettings) override;

    /**
     * @copydoc Brick::AttachFluxIn()
     */
    void AttachFluxIn(Flux* flux) override;

    /**
     * @copydoc Brick::IsOk()
     */
    bool IsOk() override;

    /**
     * Get the ice container of the glacier.
     *
     * @return The ice container of the glacier.
     */
    WaterContainer* GetIceContainer();

    /**
     * @copydoc Brick::IsGlacier()
     */
    bool IsGlacier() override {
        return true;
    }

    /**
     * @copydoc Brick::Finalize()
     */
    void Finalize() override;

    /**
     * @copydoc Brick::SetInitialState()
     */
    void SetInitialState(double value, const string& type) override;

    /**
     * @copydoc Brick::GetContent()
     */
    double GetContent(const string& type) override;

    /**
     * @copydoc Brick::UpdateContent()
     */
    void UpdateContent(double value, const string& type) override;

    /**
     * @copydoc Brick::UpdateContentFromInputs()
     */
    void UpdateContentFromInputs() override;

    /**
     * @copydoc Brick::ApplyConstraints()
     */
    void ApplyConstraints(double timeStep) override;

    /**
     * @copydoc Brick::GetDynamicContentChanges()
     */
    vecDoublePt GetDynamicContentChanges() override;

    /**
     * @copydoc Brick::GetValuePointer()
     */
    double* GetValuePointer(const string& name) override;

    /**
     * @copydoc LandCover::SurfaceComponentAdded()
     */
    void SurfaceComponentAdded(SurfaceComponent* brick) override;

  protected:
    IceContainer* _ice;
};

#endif  // HYDROBRICKS_GLACIER_H
