#ifndef HYDROBRICKS_BRICK_H
#define HYDROBRICKS_BRICK_H

#include <memory>

#include "Flux.h"
#include "Includes.h"
#include "Process.h"
#include "SettingsModel.h"
#include "WaterContainer.h"

/**
 * Enumeration of brick categories.
 */
enum class BrickCategory {
    Snowpack,          ///< Snowpack brick
    Glacier,           ///< Glacier brick
    GenericLandCover,  ///< Generic land cover (covers all user-defined land cover types)
    Unknown            ///< Unknown or unspecified brick type
};

class Brick {
  public:
    explicit Brick();

    virtual ~Brick() = default;

    /**
     * Factory method to create a brick.
     *
     * @param brickSettings settings of the brick.
     * @return the created brick.
     */
    static std::unique_ptr<Brick> Factory(const BrickSettings& brickSettings);

    /**
     * Factory method to create a brick.
     *
     * @param type type of the brick.
     * @return the created brick.
     */
    static std::unique_ptr<Brick> Factory(BrickType type);

    /**
     * Check if the brick has a parameter with the provided name.
     *
     * @param brickSettings settings of the brick containing the parameters.
     * @param name name of the parameter to check.
     * @return true if the brick has a parameter with the provided name.
     */
    static bool HasParameter(const BrickSettings& brickSettings, std::string_view name);

    /**
     * Get the pointer to the parameter value. If the brick's hydro unit carries a
     * per-unit (spatial) override for this parameter, the override pointer is returned
     * instead of the shared settings value.
     *
     * @param brickSettings settings of the brick containing the parameters.
     * @param name name of the parameter.
     * @return pointer to the parameter value.
     */
    const float* GetParameterValuePointer(const BrickSettings& brickSettings, std::string_view name);

    /**
     * Assign the parameters to the brick element.
     *
     * @param brickSettings settings of the brick containing the parameters.
     */
    virtual void SetParameters(const BrickSettings& brickSettings);

    /**
     * Attach incoming flux (non-owning; caller retains ownership).
     *
     * @param flux incoming flux (non-owning reference, owned by process)
     */
    virtual void AttachFluxIn(Flux* flux);

    /**
     * Attach incoming flux and take ownership of it.
     * Used for forcing fluxes that are not owned by any process.
     *
     * @param flux incoming flux (ownership transferred)
     */
    virtual void AttachFluxIn(std::unique_ptr<Flux> flux);

    /**
     * Add a process to the brick.
     *
     * @param process process to add (ownership transferred).
     */
    void AddProcess(std::unique_ptr<Process> process) {
        assert(process);
        _processes.push_back(std::move(process));
    }

    /**
     * Reset the brick to its initial state.
     */
    virtual void Reset();

    /**
     * Save the current state of the brick as the initial state.
     */
    virtual void SaveAsInitialState();

    /**
     * Check that everything is correctly defined.
     *
     * @return true if everything is correctly defined.
     */
    [[nodiscard]] virtual bool IsValid(bool checkProcesses = true) const;

    /**
     * Validate that everything is correctly defined.
     * Throws an exception if validation fails.
     *
     * @throws ModelConfigError if validation fails.
     */
    virtual void Validate() const;

    /**
     * Define if the brick needs to be handled by the solver.
     *
     * @return true if the brick needs to be handled by the solver.
     */
    [[nodiscard]] bool NeedsSolver() const {
        return _needsSolver;
    }

    /**
     * Set whether the brick must be handled by the ODE solver. Set to false for
     * bricks whose processes apply an exact explicit update each time step.
     *
     * @param needsSolver true if the brick needs the solver.
     */
    void SetNeedsSolver(bool needsSolver) {
        _needsSolver = needsSolver;
    }

    /**
     * Get the category of the brick.
     *
     * @return the category of the brick.
     */
    [[nodiscard]] BrickCategory GetCategory() const {
        return _category;
    }

    /**
     * Check if the brick can have an area fraction.
     *
     * @return true if the brick can have an area fraction.
     */
    [[nodiscard]] virtual bool CanHaveAreaFraction() const {
        return false;
    }

    /**
     * Check if the brick is a land cover.
     *
     * @return true if the brick is a land cover.
     */
    [[nodiscard]] virtual bool IsLandCover() const {
        return false;
    }

    /**
     * Check if the brick is null (e.g., if the area fraction is null).
     *
     * @return true if the brick is null.
     */
    [[nodiscard]] virtual bool IsNull() const {
        return false;
    }

    /**
     * Check if the brick has any processes.
     *
     * @return true if the brick has at least one process.
     */
    [[nodiscard]] bool HasProcesses() const {
        return !_processes.empty();
    }

    /**
     * Check if the brick has a hydro unit associated with it.
     *
     * @return true if the brick has a hydro unit.
     */
    [[nodiscard]] bool HasHydroUnit() const {
        return _hydroUnit != nullptr;
    }

    /**
     * Finalize the water transfer.
     */
    virtual void Finalize();

    /**
     * Set the initial state of the water container.
     *
     * @param value initial state value.
     * @param type type of the content.
     */
    virtual void SetInitialState(double value, ContentType type);

    /**
     * Get the content of the water container.
     *
     * @param type the type of content to get.
     * @return the content of the water container.
     */
    [[nodiscard]] virtual double GetContent(ContentType type) const;

    /**
     * Update the content of the water container.
     *
     * @param value new content value.
     * @param type type of the content
     */
    virtual void UpdateContent(double value, ContentType type);

    /**
     * Update the content of the water container from the inputs.
     */
    virtual void UpdateContentFromInputs();

    /**
     * Apply the constraints to the water container.
     *
     * @param timeStep time step for the simulation.
     */
    virtual void ApplyConstraints(double timeStep);

    /**
     * Get the water container.
     *
     * @return pointer to the water container.
     */
    [[nodiscard]] WaterContainer* GetWaterContainer() const;

    /**
     * Get the number of processes in the brick.
     *
     * @return number of processes.
     */
    [[nodiscard]] size_t GetProcessCount() const noexcept {
        return _processes.size();
    }

    /**
     * Get a process by its index.
     *
     * @param index index of the process.
     * @return pointer to the process.
     */
    [[nodiscard]] Process* GetProcess(size_t index) const;

    /**
     * Get the name of the brick.
     *
     * @return name of the brick.
     */
    [[nodiscard]] const string& GetName() const noexcept {
        return _name;
    }

    /**
     * Set the name of the brick.
     *
     * @param name new name of the brick.
     */
    void SetName(std::string_view name) {
        _name = string(name);
    }

    /**
     * Get the hydro unit associated with the brick.
     *
     * @return pointer to the hydro unit.
     */
    HydroUnit* GetHydroUnit() const {
        assert(_hydroUnit);
        return _hydroUnit;
    }

    /**
     * Set the hydro unit associated with the brick.
     *
     * @param hydroUnit pointer to the hydro unit.
     */
    void SetHydroUnit(HydroUnit* hydroUnit) {
        assert(hydroUnit);
        _hydroUnit = hydroUnit;
    }

    /**
     * Get pointers to the state variables.
     *
     * @return vector of pointers to the state variables.
     */
    virtual vecDoublePt GetDynamicContentChanges();

    /**
     * Get the changes in state variables from processes.
     *
     * @return vector of pointers to the changes in state variables.
     */
    vecDoublePt GetStateVariableChangesFromProcesses();

    /**
     * Get the number of process connections in the brick.
     *
     * @return number of process connections.
     */
    int GetProcessConnectionCount() const;

    /**
     * Get the pointer to the content of the brick's base water container.
     *
     * @param name name of the value (e.g., "water" or "water_content").
     * @return pointer to the water container content, or nullptr if the name is not recognized.
     */
    double* GetBaseValuePointer(std::string_view name);

    /**
     * Get the pointer to a state value specific to this brick type. The base implementation
     * returns nullptr; land cover bricks override it to expose their own container (e.g. the
     * snow content for a snowpack or the ice content for a glacier).
     *
     * @param name name of the value (e.g., "snow", "ice").
     * @return pointer to the value, or nullptr if the name is not recognized.
     */
    virtual double* GetValuePointer(std::string_view name);

  protected:
    string _name;
    bool _needsSolver;
    BrickCategory _category;
    std::unique_ptr<WaterContainer> _water;            // owning
    std::vector<std::unique_ptr<Process>> _processes;  // owning
    HydroUnit* _hydroUnit;                             // non-owning back-reference
};

#endif  // HYDROBRICKS_BRICK_H
