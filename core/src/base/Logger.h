#ifndef HYDROBRICKS_LOGGER_H
#define HYDROBRICKS_LOGGER_H

#include "Includes.h"
#include "SettingsModel.h"
#include "Subbasin.h"

/**
 * Records the model values over time: the subbasin (aggregated) values per subbasin, and the hydro unit
 * (distributed) values per unit. The subbasins are indexed in processing order (upstream first, the outlet
 * last); the hydro units are indexed globally, in the order of the subbasins.
 */
class Logger {
  public:
    explicit Logger();

    virtual ~Logger() = default;

    /**
     * Initialize the logger with the size of the time vector and the subbasins.
     *
     * @param timeSize size of the time vector.
     * @param subbasins the subbasins in processing order (upstream first, the outlet last).
     * @param modelSettings settings of the model.
     */
    void InitContainers(int timeSize, const std::vector<Subbasin*>& subbasins, SettingsModel& modelSettings);

    /**
     * Reset the logger.
     */
    void Reset();

    /**
     * Set a subbasin value pointer in the logger array.
     *
     * @param iSubbasin index of the subbasin (processing order).
     * @param iLabel index of the subbasin label.
     * @param valPt pointer to the value.
     */
    void SetSubbasinValuePointer(int iSubbasin, int iLabel, double* valPt);

    /**
     * Set a hydro unit value pointer in the logger array.
     *
     * @param iUnit index of the hydro unit (global, over all the subbasins).
     * @param iLabel index of the hydro unit label.
     * @param valPt pointer to the value.
     */
    void SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt);

    /**
     * Set a hydro unit fraction pointer in the logger array.
     *
     * @param iUnit index of the hydro unit (global, over all the subbasins).
     * @param iLabel index of the hydro unit label.
     * @param valPt pointer to the value.
     */
    void SetHydroUnitFractionPointer(int iUnit, int iLabel, double* valPt);

    /**
     * Set the date in the logger.
     *
     * @param date date to set.
     */
    void SetDate(double date);

    /**
     * Save the current values as initial values.
     */
    void SaveInitialValues();

    /**
     * Record the current values in the logger.
     */
    void Record();

    /**
     * Increment the cursor to the next time step.
     */
    void Increment();

    /**
     * Dump the outputs to a file.
     *
     * @param path path to the output file.
     * @return true if the dump was successful, false otherwise.
     */
    bool DumpOutputs(const string& path);

    /**
     * Get the outlet discharge series at the catchment outlet (the terminal subbasin), in mm over the
     * drained area.
     *
     * @return outlet discharge series.
     */
    [[nodiscard]] axd GetOutletDischarge() const;

    /**
     * Get the discharge series at the outlet of a subbasin, in mm over its drained area.
     *
     * @param subbasinId ID of the subbasin.
     * @return discharge series.
     */
    [[nodiscard]] axd GetSubbasinDischarge(int subbasinId) const;

    /**
     * Get the indices of the subbasin elements for a given item.
     *
     * @param item item to search for.
     * @return vector of indices.
     */
    [[nodiscard]] vecInt GetIndicesForSubbasinElements(const string& item) const;

    /**
     * Get the indices of the hydro unit elements for a given item.
     *
     * @param item item to search for.
     * @return vector of indices.
     */
    [[nodiscard]] vecInt GetIndicesForHydroUnitElements(const string& item) const;

    /**
     * Get the sum of a subbasin values for a given item, over time and over the subbasins (each weighted by
     * its local area, so the total is in mm over the catchment area).
     *
     * @param item item to search for.
     * @return total value.
     */
    [[nodiscard]] double GetTotalSubbasin(const string& item) const;

    /**
     * Get the sum of hydro unit values for a given item.
     *
     * @param item item to search for.
     * @param needsAreaWeighting if true, area weighting is applied.
     * @return total value.
     */
    [[nodiscard]] double GetTotalHydroUnits(const string& item, bool needsAreaWeighting = false) const;

    /**
     * Get the total outlet discharge over time (at the catchment outlet).
     *
     * @return total outlet discharge.
     */
    [[nodiscard]] double GetTotalOutletDischarge() const;

    /**
     * Get the total ET over time.
     *
     * @return total ET.
     */
    [[nodiscard]] double GetTotalET() const;

    /**
     * Get the initial storage state of the subbasins for a given tag (area-weighted over the subbasins).
     *
     * @param tag tag to search for.
     * @return initial storage state.
     */
    [[nodiscard]] double GetSubbasinInitialStorageState(const string& tag) const;

    /**
     * Get the final storage state of the subbasins for a given tag (area-weighted over the subbasins).
     *
     * @param tag tag to search for.
     * @return final storage state.
     */
    [[nodiscard]] double GetSubbasinFinalStorageState(const string& tag) const;

    /**
     * Find the land-cover fraction series that weights a logged hydro-unit component
     * to a basin (area) average. A component is matched to its land cover by name: the
     * land cover itself (``<cover>:``) or one of its surface components
     * (``<cover>_snowpack:``, ``<cover>_canopy:``). Returns the index into
     * ``_hydroUnitFractions`` or -1 if the component is not tied to a land cover (then
     * a fraction of one applies, i.e. it spans the whole unit).
     *
     * @param componentName the logged component label (e.g. "forest_canopy:water_content").
     * @return the fraction index, or -1 if none.
     */
    [[nodiscard]] int GetFractionIndexForComponent(const string& componentName) const;

    /**
     * Get the initial storage state of a hydro unit for a given tag.
     *
     * @param tag tag to search for.
     * @return initial storage state.
     */
    [[nodiscard]] double GetHydroUnitsInitialStorageState(const string& tag) const;

    /**
     * Get the final storage state of a hydro unit for a given tag.
     *
     * @param tag tag to search for.
     * @return final storage state.
     */
    [[nodiscard]] double GetHydroUnitsFinalStorageState(const string& tag) const;

    /**
     * Get the total water storage changes.
     *
     * @return the total water storage changes.
     */
    [[nodiscard]] double GetTotalWaterStorageChanges() const;

    /**
     * Get the total snow storage changes.
     *
     * @return the total snow storage changes.
     */
    [[nodiscard]] double GetTotalSnowStorageChanges() const;

    /**
     * Get the total glacier storage changes.
     *
     * @return the total glacier storage changes.
     */
    [[nodiscard]] double GetTotalGlacierStorageChanges() const;

    /**
     * Get all the subbasin values at the catchment outlet (the terminal subbasin).
     *
     * @return vector of subbasin values (one series per label).
     */
    [[nodiscard]] vecAxd GetSubbasinValues() const;

    /**
     * Get all the subbasin values of every subbasin.
     *
     * @return vector of subbasin values: one matrix per label, of shape (time steps x subbasins), the
     * subbasins in processing order.
     */
    const vecAxxd& GetSubbasinValuesPerSubbasin() const {
        return _subbasinValues;
    }

    /**
     * Get all the hydro unit values.
     *
     * @return vector of hydro unit values.
     */
    const vecAxxd& GetHydroUnitValues() const {
        return _hydroUnitValues;
    }

    /**
     * Get the time series.
     *
     * @return time series vector.
     */
    const axd& GetTime() const {
        return _time;
    }

    /**
     * Get the subbasin IDs (processing order, the outlet last).
     *
     * @return vector of subbasin IDs.
     */
    const vecInt& GetSubbasinIds() const {
        return _subbasinIds;
    }

    /**
     * Get the downstream subbasin IDs (0 for the catchment outlet).
     *
     * @return vector of downstream subbasin IDs.
     */
    const vecInt& GetSubbasinDownstreamIds() const {
        return _subbasinDownstreamIds;
    }

    /**
     * Get the structure variant used by each subbasin's catchment-level components (processing order).
     *
     * @return vector of structure IDs.
     */
    const vecInt& GetSubbasinStructureIds() const {
        return _subbasinStructureIds;
    }

    /**
     * Get the local areas of the subbasins (their own hydro units) [m2].
     *
     * @return vector of local areas.
     */
    const axd& GetSubbasinLocalAreas() const {
        return _subbasinLocalAreas;
    }

    /**
     * Get the areas drained at the subbasin outlets (own plus upstream) [m2].
     *
     * @return vector of drained areas.
     */
    const axd& GetSubbasinDrainedAreas() const {
        return _subbasinDrainedAreas;
    }

    /**
     * Get the number of subbasins.
     *
     * @return number of subbasins.
     */
    [[nodiscard]] int GetSubbasinCount() const {
        return static_cast<int>(_subbasinIds.size());
    }

    /**
     * Get the hydro unit IDs.
     *
     * @return vector of hydro unit IDs.
     */
    const vecInt& GetHydroUnitIds() const {
        return _hydroUnitIds;
    }

    /**
     * Get the hydro unit areas.
     *
     * @return vector of hydro unit areas.
     */
    const axd& GetHydroUnitAreas() const {
        return _hydroUnitAreas;
    }

    /**
     * Get the subbasin labels.
     *
     * @return vector of subbasin labels.
     */
    const vecStr& GetSubbasinLabels() const {
        return _subbasinLabels;
    }

    /**
     * Get the hydro unit labels.
     *
     * @return vector of hydro unit labels.
     */
    const vecStr& GetHydroUnitLabels() const {
        return _hydroUnitLabels;
    }

    /**
     * Get the hydro unit fraction labels.
     *
     * @return vector of fraction labels.
     */
    const vecStr& GetHydroUnitFractionLabels() const {
        return _hydroUnitFractionLabels;
    }

    /**
     * Get the hydro unit fractions.
     *
     * @return vector of fraction arrays.
     */
    const vecAxxd& GetHydroUnitFractions() const {
        return _hydroUnitFractions;
    }

    /**
     * Activate the recording of fractions.
     */
    void RecordFractions() {
        _recordFractions = true;
    }

    /**
     * Tag a subbasin log label index as an evapotranspiration (to-atmosphere) flux. A label is tagged once
     * (the same structure is built in every subbasin).
     *
     * @param iLabel index of the subbasin label.
     */
    void AddSubbasinEtIndex(int iLabel);

    /**
     * Tag a hydro unit log label index as an evapotranspiration (to-atmosphere) flux.
     *
     * @param iLabel index of the hydro unit label.
     */
    void AddHydroUnitEtIndex(int iLabel) {
        _hydroUnitEtIndices.push_back(iLabel);
    }

  protected:
    int _cursor;
    axd _time;
    bool _recordFractions;
    vecInt _subbasinIds;  // processing order, the outlet last
    vecInt _subbasinDownstreamIds;
    vecInt _subbasinStructureIds;
    axd _subbasinLocalAreas;
    axd _subbasinDrainedAreas;
    axd _subbasinWeights;  // local area / catchment area, to sum subbasin values over the catchment
    int _outletIndex;      // index of the terminal subbasin
    vecStr _subbasinLabels;
    vecAxd _subbasinInitialValues;          // [label](subbasins)
    vecAxxd _subbasinValues;                // [label](time x subbasins)
    vector<vecDoublePt> _subbasinValuesPt;  // [label][subbasin]
    vecInt _hydroUnitIds;
    vecInt _hydroUnitStructureIds;
    axd _hydroUnitAreas;
    vecStr _hydroUnitLabels;
    vecAxd _hydroUnitInitialValues;
    vecAxxd _hydroUnitValues;
    vector<vecDoublePt> _hydroUnitValuesPt;
    vecStr _hydroUnitFractionLabels;
    vecAxxd _hydroUnitFractions;
    vector<vecDoublePt> _hydroUnitFractionsPt;
    vecInt _subbasinEtIndices;   // indices into _subbasinValues that are ET (to-atmosphere) fluxes
    vecInt _hydroUnitEtIndices;  // indices into _hydroUnitValues that are ET (to-atmosphere) fluxes

  private:
    /**
     * Sum a subbasin label over time and over the subbasins, each weighted by its local area.
     *
     * @param iLabel index of the subbasin label.
     * @return the catchment total [mm].
     */
    [[nodiscard]] double WeightedTotal(int iLabel) const;

    /**
     * Get the index of a subbasin in the logger from its ID.
     *
     * @param subbasinId ID of the subbasin.
     * @return index of the subbasin (processing order).
     */
    [[nodiscard]] int GetSubbasinIndex(int subbasinId) const;

    /**
     * Get the index of the 'outlet' label among the subbasin labels.
     *
     * @return index of the outlet label.
     */
    [[nodiscard]] int GetOutletLabelIndex() const;
};

#endif  // HYDROBRICKS_LOGGER_H
