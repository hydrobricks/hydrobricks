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
    double GetPropertyDouble(const string& name, const string& unit = "") const;

    /**
     * Get a float property of the hydro unit.
     *
     * @param name The name of the property to get.
     * @param unit The unit of the property to get.
     * @return The value of the property.
     */
    float GetPropertyFloat(const string& name, const string& unit = "") const;

    /**
     * Get a property of the hydro unit as a string.
     *
     * @param name The name of the property to get.
     * @return The value of the property as a string.
     */
    string GetPropertyString(const string& name) const;

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
    [[nodiscard]] bool HasForcing(VariableType type);

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
    Forcing* GetForcing(VariableType type) const;

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
    int GetBrickCount() const;

    /**
     * Get the number of splitters in the hydro unit.
     *
     * @return The number of splitters.
     */
    int GetSplitterCount() const;

    /**
     * Get a brick by its index.
     *
     * @param index The index of the brick to get.
     * @return The brick at the specified index.
     */
    Brick* GetBrick(size_t index) const;

    /**
     * Check if the hydro unit has a brick with a specific name.
     *
     * @param name The name of the brick to check for.
     * @return True if the hydro unit has the brick, false otherwise.
     */
    [[nodiscard]] bool HasBrick(const string& name) const;

    /**
     * Get a brick by its name.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name.
     */
    Brick* GetBrick(const string& name) const;

    /**
     * Try to get a brick by its name without throwing.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name, or nullptr if not found.
     */
    Brick* TryGetBrick(const string& name) const;

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
    LandCover* GetLandCover(const string& name) const;

    /**
     * Try to get a land cover brick by its name without throwing.
     *
     * @param name The name of the land cover to get.
     * @return The land cover with the specified name, or nullptr if not found.
     */
    LandCover* TryGetLandCover(const string& name) const;

    /**
     * Get a splitter by its index.
     *
     * @param index The index of the splitter to get.
     * @return The splitter at the specified index.
     */
    Splitter* GetSplitter(size_t index) const;

    /**
     * Check if the hydro unit has a splitter with a specific name.
     *
     * @param name The name of the splitter to check for.
     * @return True if the hydro unit has the splitter, false otherwise.
     */
    [[nodiscard]] bool HasSplitter(const string& name) const;

    /**
     * Get a splitter by its name.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name.
     */
    Splitter* GetSplitter(const string& name) const;

    /**
     * Try to get a splitter by its name without throwing.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name, or nullptr if not found.
     */
    Splitter* TryGetSplitter(const string& name) const;

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
     */
    bool ChangeLandCoverAreaFraction(const string& name, double fraction);

    /**
     * Fix the land cover fractions to ensure that they sum to 1.
     *
     * @return True if the fractions were fixed successfully, false otherwise.
     */
    bool FixLandCoverFractionsTotal();

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
     * Get the lateral connections of the hydro unit.
     *
     * @return A vector of lateral connections associated with the hydro unit.
     */
    [[nodiscard]] std::vector<HydroUnitLateralConnection*> GetLateralConnections() const;

  protected:
    Types _type;
    int _id;
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
};

#endif
