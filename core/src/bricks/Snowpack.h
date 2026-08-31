#ifndef HYDROBRICKS_SNOWPACK_H
#define HYDROBRICKS_SNOWPACK_H

#include <memory>

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
     * @copydoc Brick::IsValid()
     */
    [[nodiscard]] bool IsValid(bool checkProcesses = true) const override;

    /**
     * Get the snow container.
     *
     * @return A pointer to the snow container.
     */
    [[nodiscard]] WaterContainer* GetSnowContainer() const;

    /**
     * @copydoc Brick::Finalize()
     */
    void Finalize() override;

    /**
     * @copydoc Brick::SetInitialState()
     */
    void SetInitialState(double value, ContentType type) override;

    /**
     * @copydoc Brick::GetContent()
     */
    [[nodiscard]] double GetContent(ContentType type) const override;

    /**
     * @copydoc Brick::UpdateContent()
     */
    void UpdateContent(double value, ContentType type) override;

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
    double* GetValuePointer(std::string_view name) override;

    /**
     * Check if the snowpack has snow.
     *
     * @return True if the snowpack has snow, false otherwise.
     */
    [[nodiscard]] bool HasSnow() const;

    /**
     * Get the snow surface albedo, following PREVAH's snow-age relation
     * (sxp_core.f08): albedo = 0.4 + 0.45 * exp(-0.15 * snow_age), i.e. ~0.85 for
     * fresh snow decaying toward 0.4 for old snow. The age (in time steps) is reset on
     * snowfall and incremented each step the snow persists. Meaningful only when the
     * snowpack holds snow (callers weight it by the snow-covered fraction).
     *
     * @return the snow albedo [0.4, 0.85].
     */
    [[nodiscard]] double GetSnowAlbedo() const;

  protected:
    std::unique_ptr<SnowContainer> _snow;  // owning
    double _snowAge = 0;                   // age of the snow surface [time steps] since the last snowfall
    double _snowfallInput = 0;             // snow inflow of the current time step [mm] (for the age reset)
};

#endif  // HYDROBRICKS_SNOWPACK_H
