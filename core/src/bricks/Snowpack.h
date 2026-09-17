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
    void UpdateContentFromInputs(double timeStepInDays = 1.0) override;

    /**
     * @copydoc Brick::ResetInputBooking()
     */
    void ResetInputBooking() override;

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
     * Get the age of the snow surface since the last snowfall. The age (in days) is reset
     * on snowfall and when the snowpack is empty, and incremented by the time step length
     * each step the snow persists. Used by age-dependent albedo parameterizations (e.g.
     * PrevahSnowAlbedo).
     *
     * @return the snow surface age [d].
     */
    [[nodiscard]] double GetSnowAge() const;

  protected:
    // Snowfall rate [mm/d] from which the surface counts as fresh snow (age reset).
    static constexpr double kFreshSnowfallRate = 0.01;
    // Snow content [mm] below which the snowpack counts as empty (age reset).
    static constexpr double kEmptySnowContent = 0.01;

    std::unique_ptr<SnowContainer> _snow;  // owning
    double _snowAge = 0;                   // age of the snow surface [d] since the last snowfall
    double _initialSnowAge = 0;            // snow surface age saved with the initial state [d]
    double _snowfallInput = 0;             // snow inflow of the current time step [mm] (for the age reset)
};

#endif  // HYDROBRICKS_SNOWPACK_H
