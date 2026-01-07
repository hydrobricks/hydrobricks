#ifndef HYDROBRICKS_RESULT_WRITER_H
#define HYDROBRICKS_RESULT_WRITER_H

#include "Includes.h"

/**
 * @class ResultWriter
 * @brief Handles writing simulation results to various output formats.
 *
 * This class is responsible for writing simulation data to files (NetCDF, CSV, etc.).
 * It decouples the data recording/logging from the output writing concerns,
 * allowing for flexible output strategies (snapshot-based or streaming).
 */
class ResultWriter : public wxObject {
  public:
    ResultWriter() = default;
    ~ResultWriter() override = default;

    /**
     * Write results to a NetCDF file.
     *
     * @param path Directory path where the output file will be created.
     * @param time Time series vector.
     * @param hydroUnitIds Vector of hydro unit IDs.
     * @param hydroUnitAreas Vector of hydro unit areas.
     * @param subBasinLabels Labels for sub-basin aggregated values.
     * @param subBasinValues Vector of sub-basin value arrays.
     * @param hydroUnitLabels Labels for distributed hydro unit values.
     * @param hydroUnitValues Vector of 2D hydro unit value arrays.
     * @param hydroUnitFractionLabels Labels for land cover fractions (optional).
     * @param hydroUnitFractions Vector of 2D fraction arrays (optional).
     * @return true if successful, false otherwise.
     */
    bool WriteNetCDF(const string& path, const axd& time, const vecInt& hydroUnitIds, const axd& hydroUnitAreas,
                     const vecStr& subBasinLabels, const vecAxd& subBasinValues, const vecStr& hydroUnitLabels,
                     const vecAxxd& hydroUnitValues, const vecStr& hydroUnitFractionLabels = vecStr(),
                     const vecAxxd& hydroUnitFractions = vecAxxd());

    /**
     * Write results to a CSV file.
     *
     * @param path Directory path where the output file will be created.
     * @param time Time series vector.
     * @param labels Column labels.
     * @param values Vector of value arrays.
     * @return true if successful, false otherwise.
     */
    bool WriteCSV(const string& path, const axd& time, const vecStr& labels, const vecAxd& values);

    /**
     * Append a single time step to a streaming NetCDF file.
     * This allows for progressive writing without storing all data in memory.
     *
     * @param filePath Full path to the NetCDF file.
     * @param timeStep Current time step index.
     * @param time Current time value.
     * @param subBasinValues Current sub-basin values.
     * @param hydroUnitValues Current hydro unit values.
     * @param hydroUnitFractions Current fraction values (optional).
     * @return true if successful, false otherwise.
     */
    bool AppendToNetCDF(const string& filePath, int timeStep, double time, const axd& subBasinValues,
                        const axxd& hydroUnitValues, const axxd& hydroUnitFractions = axxd());

    /**
     * Initialize a streaming NetCDF file for progressive writing.
     *
     * @param path Directory path where the output file will be created.
     * @param timeSize Total number of time steps.
     * @param hydroUnitIds Vector of hydro unit IDs.
     * @param hydroUnitAreas Vector of hydro unit areas.
     * @param subBasinLabels Labels for sub-basin values.
     * @param hydroUnitLabels Labels for hydro unit values.
     * @param hydroUnitFractionLabels Labels for fractions (optional).
     * @return Full path to the created file, or empty string on failure.
     */
    string InitializeStreamingNetCDF(const string& path, int timeSize, const vecInt& hydroUnitIds,
                                     const axd& hydroUnitAreas, const vecStr& subBasinLabels,
                                     const vecStr& hydroUnitLabels, const vecStr& hydroUnitFractionLabels = vecStr());

  protected:
    /**
     * Create the NetCDF file and define its structure.
     *
     * @param filePath Full path to the file.
     * @param timeSize Size of time dimension.
     * @param numHydroUnits Number of hydro units.
     * @param numSubBasinItems Number of aggregated items.
     * @param numHydroUnitItems Number of distributed items.
     * @param numFractions Number of land cover fractions.
     * @param hydroUnitIds Vector of hydro unit IDs.
     * @param hydroUnitAreas Vector of hydro unit areas.
     * @param subBasinLabels Labels for sub-basin values.
     * @param hydroUnitLabels Labels for hydro unit values.
     * @param hydroUnitFractionLabels Labels for fractions.
     * @return true if successful, false otherwise.
     */
    bool CreateNetCDFStructure(const string& filePath, int timeSize, int numHydroUnits, int numSubBasinItems,
                               int numHydroUnitItems, int numFractions, const vecInt& hydroUnitIds,
                               const axd& hydroUnitAreas, const vecStr& subBasinLabels, const vecStr& hydroUnitLabels,
                               const vecStr& hydroUnitFractionLabels);
};

#endif  // HYDROBRICKS_RESULT_WRITER_H

