#ifndef HYDROBRICKS_HYDRO_UNIT_H
#define HYDROBRICKS_HYDRO_UNIT_H

#include <memory>
#include <unordered_map>
#include <vector>

#include "Brick.h"
#include "Forcing.h"
#include "HydroUnitLateralConnection.h"
#include "HydroUnitProperty.h"
#include "Includes.h"
#include "LandCover.h"
#include "Splitter.h"

struct HydroUnitSettings;

class HydroUnit {
  public:
    enum Types {
        Distributed,
        SemiDistributed,
        Lumped,
        Undefined
    };

    explicit HydroUnit(double area = UNDEFINED, Types type = Undefined);

    virtual ~HydroUnit();

    /**
     * Reset the hydro unit to its initial state.
     */
    void Reset();

    /**
     * Save the current state of the hydro unit as the initial state.
     */
    void SaveAsInitialState();

    /**
     * Set the properties of the hydro unit.
     *
     * @param unitSettings The settings to set.
     */
    void SetProperties(HydroUnitSettings& unitSettings);

    /**
     * Add a property to the hydro unit.
     *
     * @param property The property to add.
     */
    void AddProperty(std::unique_ptr<HydroUnitProperty> property);

    /**
     * Get a numeric property of the hydro unit.
     *
     * @param name The name of the property to get.
     * @param unit The unit of the property to get.
     * @return The value of the property.
     */
    [[nodiscard]] double GetPropertyDouble(std::string_view name, std::string_view unit = "") const;

    /**
     * Get a float property of the hydro unit.
     *
     * @param name The name of the property to get.
     * @param unit The unit of the property to get.
     * @return The value of the property.
     */
    [[nodiscard]] float GetPropertyFloat(std::string_view name, std::string_view unit = "") const;

    /**
     * Get a property of the hydro unit as a string.
     *
     * @param name The name of the property to get.
     * @return The value of the property as a string.
     */
    [[nodiscard]] string GetPropertyString(std::string_view name) const;

    /**
     * Check whether the hydro unit has a numeric or string property with a given name.
     *
     * @param name The name of the property to look for.
     * @return true if a property with that name exists.
     */
    [[nodiscard]] bool HasProperty(std::string_view name) const;

    /**
     * Set a per-unit override value for a model parameter, keyed by
     * "<brickName>:<paramName>". Used for spatial (per-unit) parameters: the brick or
     * process reads this value instead of the shared settings value.
     *
     * @param key The override key ("<brickName>:<paramName>").
     * @param value The per-unit parameter value.
     */
    void SetParameterOverride(const string& key, float value);

    /**
     * Check whether a per-unit parameter override exists for a given key.
     *
     * @param key The override key ("<brickName>:<paramName>").
     * @return true if an override exists.
     */
    [[nodiscard]] bool HasParameterOverride(const string& key) const;

    /**
     * Get a stable pointer to a per-unit parameter override value (for the brick/process
     * to read live). The key must exist (HasParameterOverride).
     *
     * @param key The override key ("<brickName>:<paramName>").
     * @return Pointer to the override value.
     */
    [[nodiscard]] const float* GetParameterOverridePointer(const string& key) const;

    /**
     * Add a brick to the hydro unit.
     *
     * @param brick The brick to add.
     */
    void AddBrick(std::unique_ptr<Brick> brick);

    /**
     * Add a splitter to the hydro unit.
     *
     * @param splitter The splitter to add.
     */
    void AddSplitter(std::unique_ptr<Splitter> splitter);

    /**
     * Check if the hydro unit has a forcing of a specific type.
     *
     * @param type The type of forcing to check for.
     * @return True if the hydro unit has the forcing, false otherwise.
     */
    [[nodiscard]] bool HasForcing(VariableType type) const;

    /**
     * Attach a forcing to the hydro unit.
     *
     * @param forcing The forcing to attach.
     */
    void AddForcing(std::unique_ptr<Forcing> forcing);

    /**
     * Get a forcing of a specific type.
     *
     * @param type The type of forcing to get.
     * @return The forcing of the specified type.
     */
    [[nodiscard]] Forcing* GetForcing(VariableType type) const;

    /**
     * Reset all dynamic forcing overrides set during the current timestep.
     */
    void ResetForcingUpdates();

    /**
     * Add a lateral connection to the hydro unit.
     *
     * @param receiver The hydro unit that receives the lateral connection.
     * @param fraction The fraction of the flow that is transferred.
     * @param type The type of the lateral connection (optional). It is unused in the current implementation, but can be
     * used for future extensions (for example, to differentiate between snow and groundwater).
     */
    void AddLateralConnection(HydroUnit* receiver, double fraction, const string& type = "");

    /**
     * Reserve space for a number of bricks in the hydro unit.
     *
     * @param count The number of bricks to reserve space for.
     */
    void ReserveBricks(size_t count);

    /**
     * Reserve space for a number of land cover bricks in the hydro unit.
     *
     * @param count The number of land cover bricks to reserve space for.
     */
    void ReserveLandCoverBricks(size_t count);

    /**
     * Reserve space for a number of splitters in the hydro unit.
     *
     * @param count The number of splitters to reserve space for.
     */
    void ReserveSplitters(size_t count);

    /**
     * Reserve space for a number of lateral connections in the hydro unit.
     *
     * @param count The number of lateral connections to reserve space for.
     */
    void ReserveLateralConnections(size_t count);

    /**
     * Reserve space for a number of forcings in the hydro unit.
     *
     * @param count The number of forcings to reserve space for.
     */
    void ReserveForcings(size_t count);

    /**
     * Get the number of bricks in the hydro unit.
     *
     * @return The number of bricks.
     */
    [[nodiscard]] int GetBrickCount() const;

    /**
     * Get the number of splitters in the hydro unit.
     *
     * @return The number of splitters.
     */
    [[nodiscard]] int GetSplitterCount() const;

    /**
     * Get a brick by its index.
     *
     * @param index The index of the brick to get.
     * @return The brick at the specified index.
     */
    [[nodiscard]] Brick* GetBrick(size_t index) const;

    /**
     * Check if the hydro unit has a brick with a specific name.
     *
     * @param name The name of the brick to check for.
     * @return True if the hydro unit has the brick, false otherwise.
     */
    [[nodiscard]] bool HasBrick(std::string_view name) const;

    /**
     * Get a brick by its name.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name.
     */
    [[nodiscard]] Brick* GetBrick(std::string_view name) const;

    /**
     * Try to get a brick by its name without throwing.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name, or nullptr if not found.
     */
    [[nodiscard]] Brick* TryGetBrick(std::string_view name) const;

    /**
     * Get a vector of all snowpack bricks in the hydro unit.
     *
     * @return A vector of pointers to the snowpack bricks.
     */
    [[nodiscard]] std::vector<Brick*> GetSnowpacks() const {
        std::vector<Brick*> snowBricks;
        snowBricks.reserve(_bricks.size());
        for (const auto& brick : _bricks) {
            if (brick->GetCategory() == BrickCategory::Snowpack) {
                snowBricks.push_back(brick.get());
            }
        }
        return snowBricks;
    }

    /**
     * Get a land cover by its name.
     *
     * @param name The name of the land cover to get.
     * @return The land cover with the specified name.
     */
    [[nodiscard]] LandCover* GetLandCover(std::string_view name) const;

    /**
     * Try to get a land cover brick by its name without throwing.
     *
     * @param name The name of the land cover to get.
     * @return The land cover with the specified name, or nullptr if not found.
     */
    [[nodiscard]] LandCover* TryGetLandCover(std::string_view name) const;

    /**
     * Get a splitter by its index.
     *
     * @param index The index of the splitter to get.
     * @return The splitter at the specified index.
     */
    [[nodiscard]] Splitter* GetSplitter(size_t index) const;

    /**
     * Check if the hydro unit has a splitter with a specific name.
     *
     * @param name The name of the splitter to check for.
     * @return True if the hydro unit has the splitter, false otherwise.
     */
    [[nodiscard]] bool HasSplitter(std::string_view name) const;

    /**
     * Get a splitter by its name.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name.
     */
    [[nodiscard]] Splitter* GetSplitter(std::string_view name) const;

    /**
     * Try to get a splitter by its name without throwing.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name, or nullptr if not found.
     */
    [[nodiscard]] Splitter* TryGetSplitter(std::string_view name) const;

    /**
     * Check if the hydro unit is properly configured.
     *
     * @return True if everything is correctly defined, false otherwise.
     */
    [[nodiscard]] bool IsValid(bool checkProcesses = true) const;

    /**
     * Validate that the hydro unit is properly configured.
     * Throws an exception if validation fails.
     *
     * @throws ModelConfigError if validation fails.
     */
    void Validate() const;

    /**
     * Change the area fraction of a land cover in the hydro unit.
     * Ensure that the sum of all land cover fractions is equal to 1.
     *
     * @param name The name of the land cover to change.
     * @param fraction The new area fraction of the land cover.
     * @return True on success; false if the fraction is outside [0, 1] or the land cover was not found.
     */
    bool ChangeLandCoverAreaFraction(std::string_view name, double fraction);

    /**
     * Fix the land cover fractions to ensure that they sum to 1.
     *
     * @return True if the fractions were fixed successfully, false otherwise.
     */
    bool FixLandCoverFractionsTotal();

    /**
     * Restore the area fractions of all land covers to their initial extents without
     * touching the stored contents (used after the spin-up phase).
     */
    void RestoreInitialAreaFractions();

    /**
     * Get the generic land cover that absorbs area changes ('open', or its
     * 'ground'/'generic'/'generic_land_cover' aliases).
     *
     * @return Pointer to the generic land cover, or nullptr if none is defined.
     */
    [[nodiscard]] LandCover* GetGenericLandCover() const;

    /**
     * Get the type of the hydro unit.
     *
     * @return The type of the hydro unit.
     */
    Types GetType() const {
        return _type;
    }

    /**
     * Set the ID of the hydro unit.
     *
     * @param id The ID to set.
     */
    void SetId(int id) {
        _id = id;
    }

    /**
     * Get the area of the hydro unit.
     *
     * @return The area of the hydro unit [m²]
     */
    [[nodiscard]] double GetArea() const {
        return _area;
    }

    /**
     * Get the ID of the hydro unit.
     *
     * @return The ID of the hydro unit.
     */
    [[nodiscard]] int GetId() const {
        return _id;
    }

    /**
     * Set the model-structure ID used by this hydro unit. Units sharing the same
     * subsurface use the same structure; an exclusive land cover (e.g. a lake) can
     * place a unit on a different structure variant. Defaults to 1.
     *
     * @param structureId The model-structure ID to use when building this unit.
     */
    void SetStructureId(int structureId) {
        _structureId = structureId;
    }

    /**
     * Get the model-structure ID used by this hydro unit.
     *
     * @return The model-structure ID (defaults to 1).
     */
    [[nodiscard]] int GetStructureId() const {
        return _structureId;
    }

    /**
     * Get the lateral connections of the hydro unit.
     *
     * @return A vector of lateral connections associated with the hydro unit.
     */
    [[nodiscard]] std::vector<HydroUnitLateralConnection*> GetLateralConnections() const;

  private:
    /**
     * Conserve stored water/snow/ice when a land cover's area changes by transferring
     * the content sitting on the converted land between the changed cover and the
     * generic cover. The land that changes hands carries the donor cover's water
     * column; the donor keeps its depth while the receiver's depth is diluted or
     * concentrated so the total volume is preserved.
     *
     * @param changed The land cover whose fraction changes.
     * @param oldFraction The changed cover's area fraction before the change.
     * @param newFraction The changed cover's area fraction after the change.
     * @param generic The generic cover that absorbs the area difference.
     * @param genericOldFraction The generic cover's area fraction before the change.
     * @param genericNewFraction The generic cover's area fraction after the change.
     */
    void TransferLandCoverContent(LandCover* changed, double oldFraction, double newFraction, LandCover* generic,
                                  double genericOldFraction, double genericNewFraction);

  protected:
    Types _type;
    int _id;
    int _structureId = 1;                                                          // model-structure variant used
    double _area;                                                                  // [m²]
    std::vector<std::unique_ptr<HydroUnitProperty>> _properties;                   // owning
    std::vector<std::unique_ptr<HydroUnitLateralConnection>> _lateralConnections;  // owning
    std::vector<std::unique_ptr<Brick>> _bricks;                                   // owning
    std::unordered_map<string, Brick*> _brickMap;                                  // non-owning view into _bricks
    std::vector<LandCover*> _landCoverBricks;                                      // non-owning view into _bricks
    std::unordered_map<string, LandCover*> _landCoverMap;    // non-owning view into _landCoverBricks
    std::vector<std::unique_ptr<Splitter>> _splitters;       // owning
    std::unordered_map<string, Splitter*> _splitterMap;      // non-owning view into _splitters
    std::vector<std::unique_ptr<Forcing>> _forcing;          // owning
    std::unordered_map<VariableType, Forcing*> _forcingMap;  // non-owning view into _forcing
    std::unordered_map<string, float> _paramOverrides;       // per-unit spatial parameter values
};

#endif
