#ifndef HYDROBRICKS_LOGGER_H
#define HYDROBRICKS_LOGGER_H

#include "Includes.h"
#include "SettingsModel.h"
#include "SubBasin.h"

class Logger : public wxObject {
  public:
    explicit Logger();

    ~Logger() override = default;

    /**
     * Initialize the logger with the size of the time vector and the sub-basin object.
     *
     * @param timeSize size of the time vector.
     * @param subBasin pointer to the sub-basin object.
     * @param modelSettings settings of the model.
     */
    void InitContainers(int timeSize, SubBasin* subBasin, SettingsModel& modelSettings);

    /**
     * Reset the logger.
     */
    void Reset();

    /**
     * Set a sub-basin value pointer in the logger array.
     *
     * @param iLabel index of the sub-basin label.
     * @param valPt pointer to the value.
     */
    void SetSubBasinValuePointer(int iLabel, double* valPt);

    /**
     * Set a hydro unit value pointer in the logger array.
     *
     * @param iUnit index of the hydro unit.
     * @param iLabel index of the hydro unit label.
     * @param valPt pointer to the value.
     */
    void SetHydroUnitValuePointer(int iUnit, int iLabel, double* valPt);

    /**
     * Set a hydro unit fraction pointer in the logger array.
     *
     * @param iUnit index of the hydro unit.
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
     * Get the outlet discharge series.
     *
     * @return outlet discharge series.
     */
    axd GetOutletDischarge();

    /**
     * Get the indices of the sub-basin elements for a given item.
     *
     * @param item item to search for.
     * @return vector of indices.
     */
    vecInt GetIndicesForSubBasinElements(const string& item);

    /**
     * Get the indices of the hydro unit elements for a given item.
     *
     * @param item item to search for.
     * @return vector of indices.
     */
    vecInt GetIndicesForHydroUnitElements(const string& item);

    /**
     * Get the sum of a sub-basin values for a given item.
     *
     * @param item item to search for.
     * @return total value.
     */
    double GetTotalSubBasin(const string& item);

    /**
     * Get the sum of hydro unit values for a given item.
     *
     * @param item item to search for.
     * @param needsAreaWeighting if true, area weighting is applied.
     * @return total value.
     */
    double GetTotalHydroUnits(const string& item, bool needsAreaWeighting = false);

    /**
     * Get the total outlet discharge over time.
     *
     * @return total outlet discharge.
     */
    double GetTotalOutletDischarge();

    /**
     * Get the total ET over time.
     *
     * @return total ET.
     */
    double GetTotalET();

    /**
     * Get the initial storage state of a sub-basin for a given tag.
     *
     * @param tag tag to search for.
     * @return initial storage state.
     */
    double GetSubBasinInitialStorageState(const string& tag);

    /**
     * Get the final storage state of a sub-basin for a given tag.
     *
     * @param tag tag to search for.
     * @return final storage state.
     */
    double GetSubBasinFinalStorageState(const string& tag);

    /**
     * Get the initial storage state of a hydro unit for a given tag.
     *
     * @param tag tag to search for.
     * @return initial storage state.
     */
    double GetHydroUnitsInitialStorageState(const string& tag);

    /**
     * Get the final storage state of a hydro unit for a given tag.
     *
     * @param tag tag to search for.
     * @return final storage state.
     */
    double GetHydroUnitsFinalStorageState(const string& tag);

    /**
     * Get the total water storage changes over time.
     *
     * @return total water storage changes.
     */
    double GetTotalWaterStorageChanges();

    /**
     * Get the total snow storage changes over time.
     *
     * @return total snow storage changes.
     */
    double GetTotalSnowStorageChanges();

    /**
     * Get the total glacier storage changes over time.
     *
     * @return total glacier storage changes.
     */
    double GetTotalGlacierStorageChanges();

    /**
     * Get all the sub-basin values.
     *
     * @return vector of sub-basin values.
     */
    const vecAxd& GetSubBasinValues() {
        return m_subBasinValues;
    }

    /**
     * Get all the hydro unit values.
     *
     * @return vector of hydro unit values.
     */
    const vecAxxd& GetHydroUnitValues() {
        return m_hydroUnitValues;
    }

    /**
     * Activate the recording of fractions.
     */
    void RecordFractions() {
        m_recordFractions = true;
    }

  protected:
    int m_cursor;
    axd m_time;
    bool m_recordFractions;
    vecStr m_subBasinLabels;
    axd m_subBasinInitialValues;
    vecAxd m_subBasinValues;
    vecDoublePt m_subBasinValuesPt;
    vecInt m_hydroUnitIds;
    axd m_hydroUnitAreas;
    vecStr m_hydroUnitLabels;
    vecAxd m_hydroUnitInitialValues;
    vecAxxd m_hydroUnitValues;
    vector<vecDoublePt> m_hydroUnitValuesPt;
    vecStr m_hydroUnitFractionLabels;
    vecAxxd m_hydroUnitFractions;
    vector<vecDoublePt> m_hydroUnitFractionsPt;
};

#endif  // HYDROBRICKS_LOGGER_H
