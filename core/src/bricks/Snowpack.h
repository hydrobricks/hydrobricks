#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include "Includes.h"
#include "SnowContainer.h"
#include "SurfaceComponent.h"

class Snowpack : public SurfaceComponent {
  public:
    Snowpack();

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
     * Get the snow container.
     *
     * @return A pointer to the snow container.
     */
    WaterContainer* GetSnowContainer();

    /**
     * @copydoc Brick::IsSnowpack()
     */
    bool IsSnowpack() override {
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
     * Check if the snowpack has snow.
     *
     * @return True if the snowpack has snow, false otherwise.
     */
    bool HasSnow();

  protected:
    SnowContainer* m_snow;
};

#endif  // HYDROBRICKS_SNOWPACK_H
