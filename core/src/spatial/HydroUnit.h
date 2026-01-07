#ifndef HYDROBRICKS_HYDRO_UNIT_H
#define HYDROBRICKS_HYDRO_UNIT_H

#include "Brick.h"
#include "Forcing.h"
#include "HydroUnitLateralConnection.h"
#include "HydroUnitProperty.h"
#include "Includes.h"
#include "LandCover.h"
#include "Splitter.h"

struct HydroUnitSettings;

class HydroUnit : public wxObject {
  public:
    enum Types {
        Distributed,
        SemiDistributed,
        Lumped,
        Undefined
    };

    explicit HydroUnit(double area = UNDEFINED, Types type = Undefined);

    ~HydroUnit() override;

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
    void AddProperty(HydroUnitProperty* property);

    /**
     * Get a numeric property of the hydro unit.
     *
     * @param name The name of the property to get.
     * @param unit The unit of the property to get.
     * @return The value of the property.
     */
    double GetPropertyDouble(const string& name, const string& unit = "");

    /**
     * Get a property of the hydro unit as a string.
     *
     * @param name The name of the property to get.
     * @return The value of the property as a string.
     */
    string GetPropertyString(const string& name);

    /**
     * Add a brick to the hydro unit.
     *
     * @param brick The brick to add.
     */
    void AddBrick(Brick* brick);

    /**
     * Add a splitter to the hydro unit.
     *
     * @param splitter The splitter to add.
     */
    void AddSplitter(Splitter* splitter);

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
    void AddForcing(Forcing* forcing);

    /**
     * Get a forcing of a specific type.
     *
     * @param type The type of forcing to get.
     * @return The forcing of the specified type.
     */
    Forcing* GetForcing(VariableType type);

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
     * Get the number of bricks in the hydro unit.
     *
     * @return The number of bricks.
     */
    int GetBricksCount();

    /**
     * Get the number of splitters in the hydro unit.
     *
     * @return The number of splitters.
     */
    int GetSplittersCount();

    /**
     * Get a brick by its index.
     *
     * @param index The index of the brick to get.
     * @return The brick at the specified index.
     */
    Brick* GetBrick(int index);

    /**
     * Check if the hydro unit has a brick with a specific name.
     *
     * @param name The name of the brick to check for.
     * @return True if the hydro unit has the brick, false otherwise.
     */
    [[nodiscard]] bool HasBrick(const string& name);

    /**
     * Get a brick by its name.
     *
     * @param name The name of the brick to get.
     * @return The brick with the specified name.
     */
    Brick* GetBrick(const string& name);

    /**
     * Get a vector of all snowpack bricks in the hydro unit.
     *
     * @return A vector of pointers to the snowpack bricks.
     */
    vector<Brick*> GetSnowpacks() const {
        vector<Brick*> snowBricks;
        for (auto& brick : _bricks) {
            if (brick->IsSnowpack()) {
                snowBricks.push_back(brick);
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
    LandCover* GetLandCover(const string& name);

    /**
     * Get a splitter by its index.
     *
     * @param index The index of the splitter to get.
     * @return The splitter at the specified index.
     */
    Splitter* GetSplitter(int index);

    /**
     * Check if the hydro unit has a splitter with a specific name.
     *
     * @param name The name of the splitter to check for.
     * @return True if the hydro unit has the splitter, false otherwise.
     */
    [[nodiscard]] bool HasSplitter(const string& name);

    /**
     * Get a splitter by its name.
     *
     * @param name The name of the splitter to get.
     * @return The splitter with the specified name.
     */
    Splitter* GetSplitter(const string& name);

    /**
     * Check if everything is ok with the hydro unit.
     *
     * @return True if everything is ok, false otherwise.
     */
    [[nodiscard]] bool IsOk();

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
    Types GetType() {
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
    double GetArea() const {
        return _area;
    }

    /**
     * Get the ID of the hydro unit.
     *
     * @return The ID of the hydro unit.
     */
    int GetId() const {
        return _id;
    }

    /**
     * Get the lateral connections of the hydro unit.
     *
     * @return A vector of lateral connections associated with the hydro unit.
     */
    vector<HydroUnitLateralConnection*> GetLateralConnections() const {
        return _lateralConnections;
    }

  protected:
    Types _type;
    int _id;
    double _area;  // [m²]
    vector<HydroUnitProperty*> _properties;
    vector<HydroUnitLateralConnection*> _lateralConnections;
    vector<Brick*> _bricks;
    vector<LandCover*> _landCoverBricks;
    vector<Splitter*> _splitters;
    vector<Forcing*> _forcing;
};

#endif
