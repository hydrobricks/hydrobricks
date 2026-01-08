#include "ResultWriter.h"

#include <wx/filename.h>

#include <fstream>

#include "FileNetcdf.h"

bool ResultWriter::WriteNetCDF(const string& path, const axd& time, const vecInt& hydroUnitIds,
                               const axd& hydroUnitAreas, const vecStr& subBasinLabels, const vecAxd& subBasinValues,
                               const vecStr& hydroUnitLabels, const vecAxxd& hydroUnitValues,
                               const vecStr& hydroUnitFractionLabels, const vecAxxd& hydroUnitFractions) {
    if (!wxDirExists(path)) {
        wxLogError(_("The directory %s could not be found."), path);
        return false;
    }

    wxLogMessage(_("Writing output file."));

    try {
        string filePath = path;
        filePath.append(wxString(wxFileName::GetPathSeparator()).c_str());
        filePath.append("/results.nc");

        FileNetcdf file;

        if (!file.Create(filePath)) {
            return false;
        }

        // Create dimensions
        int dimIdTime = file.DefDim("time", (int)time.size());
        int dimIdUnit = file.DefDim("hydro_units", (int)hydroUnitIds.size());
        int dimIdItemsAgg = file.DefDim("aggregated_values", (int)subBasinLabels.size());
        int dimIdItemsDist = file.DefDim("distributed_values", (int)hydroUnitLabels.size());
        int dimIdFractions = 0;
        bool recordFractions = !hydroUnitFractionLabels.empty() && !hydroUnitFractions.empty();
        if (recordFractions) {
            dimIdFractions = file.DefDim("land_covers", (int)hydroUnitFractionLabels.size());
        }

        // Create variables and put data
        int varId = file.DefVarDouble("time", {dimIdTime});
        file.PutVar(varId, time);
        file.PutAttText("long_name", "time", varId);
        file.PutAttText("units", "days since 1858-11-17 00:00:00.0", varId);

        varId = file.DefVarInt("hydro_units_ids", {dimIdUnit});
        file.PutVar(varId, hydroUnitIds);
        file.PutAttText("long_name", "hydrological units ids", varId);

        varId = file.DefVarDouble("hydro_units_areas", {dimIdUnit});
        file.PutVar(varId, hydroUnitAreas);
        file.PutAttText("long_name", "hydrological units areas", varId);

        varId = file.DefVarDouble("sub_basin_values", {dimIdItemsAgg, dimIdTime}, 2, true);
        file.PutVar(varId, subBasinValues);
        file.PutAttText("long_name", "aggregated values over the sub basin", varId);
        file.PutAttText("units", "mm", varId);

        varId = file.DefVarDouble("hydro_units_values", {dimIdItemsDist, dimIdUnit, dimIdTime}, 3, true);
        file.PutVar(varId, hydroUnitValues);
        file.PutAttText("long_name", "values for each hydrological units", varId);
        file.PutAttText("units", "mm", varId);

        if (recordFractions) {
            varId = file.DefVarDouble("land_cover_fractions", {dimIdFractions, dimIdUnit, dimIdTime}, 3, true);
            file.PutVar(varId, hydroUnitFractions);
            file.PutAttText("long_name", "land cover fractions for each hydrological units", varId);
            file.PutAttText("units", "percent", varId);
        }

        // Global attributes
        file.PutAttString("labels_aggregated", subBasinLabels);
        file.PutAttString("labels_distributed", hydroUnitLabels);
        if (recordFractions) {
            file.PutAttString("labels_land_covers", hydroUnitFractionLabels);
        }

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    wxLogMessage(_("Output file written."));

    return true;
}

bool ResultWriter::WriteCSV(const string& path, const axd& time, const vecStr& labels, const vecAxd& values) {
    if (!wxDirExists(path)) {
        wxLogError(_("The directory %s could not be found."), path);
        return false;
    }

    wxLogMessage(_("Writing CSV output file."));

    try {
        string filePath = path;
        filePath.append(wxString(wxFileName::GetPathSeparator()).c_str());
        filePath.append("/results.csv");

        std::ofstream outFile(filePath);
        if (!outFile.is_open()) {
            wxLogError(_("Could not open file %s for writing."), filePath);
            return false;
        }

        // Write header
        outFile << "time";
        for (const auto& label : labels) {
            outFile << "," << label;
        }
        outFile << "\n";

        // Write data
        for (int t = 0; t < time.size(); ++t) {
            outFile << time[t];
            for (int i = 0; i < values.size(); ++i) {
                outFile << "," << values[i][t];
            }
            outFile << "\n";
        }

        outFile.close();

    } catch (std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    wxLogMessage(_("CSV output file written."));

    return true;
}

bool ResultWriter::AppendToNetCDF(const string& filePath, int timeStep, double time, const axd& subBasinValues,
                                  const axxd& hydroUnitValues, const axxd& hydroUnitFractions) {
    // This would require implementing streaming write capability in FileNetcdf
    // For now, this is a placeholder for future implementation
    wxLogWarning(_("Streaming NetCDF writing is not yet implemented."));
    return false;
}

string ResultWriter::InitializeStreamingNetCDF(const string& path, int timeSize, const vecInt& hydroUnitIds,
                                               const axd& hydroUnitAreas, const vecStr& subBasinLabels,
                                               const vecStr& hydroUnitLabels, const vecStr& hydroUnitFractionLabels) {
    // This would initialize a NetCDF file structure for streaming writes
    // For now, this is a placeholder for future implementation
    wxLogWarning(_("Streaming NetCDF initialization is not yet implemented."));
    return "";
}

bool ResultWriter::CreateNetCDFStructure(const string& filePath, int timeSize, int numHydroUnits, int numSubBasinItems,
                                         int numHydroUnitItems, int numFractions, const vecInt& hydroUnitIds,
                                         const axd& hydroUnitAreas, const vecStr& subBasinLabels,
                                         const vecStr& hydroUnitLabels, const vecStr& hydroUnitFractionLabels) {
    // Helper method for creating NetCDF structure
    // For now, this is a placeholder for future implementation
    return false;
}
