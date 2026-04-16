#include "Hydrobricks.h"

#include <cstring>
#include <iostream>

#include "ModelHydro.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"
#include "TimeSeries.h"
#include "Utils.h"

static const char* kUsage =
    "Usage: hydrobricks [OPTIONS]\n"
    "\n"
    "Options:\n"
    "  --model-file <path>       Path to the model settings file\n"
    "  --parameters-file <path>  Path to the model parameters file\n"
    "  --basin-file <path>       Path to the spatial structure file\n"
    "  --data-file <path>        Path to the time series data file\n"
    "  --output-path <path>      Directory to save outputs (no trailing separator)\n"
    "  --start-date <YYYY-MM-DD> Starting date of the modelling\n"
    "  --end-date <YYYY-MM-DD>   Ending date of the modelling\n"
    "  --version                 Show version number and quit\n"
    "  --help                    Show this help message and quit\n";

static const char* kBanner =
    "\n"
    "________________________________________________________\n"
    " _               _           _          _      _        \n"
    "| |__  _   _  __| |_ __ ___ | |__  _ __(_) ___| | _____\n"
    "| '_ \\| | | |/ _` | '__/ _ \\| '_ \\| '__| |/ __| |/ / __|\n"
    "| | | | |_| | (_| | | | (_) | |_) | |  | | (__|   <\\__ \\\n"
    "|_| |_|\\__, |\\__,_|_|  \\___/|_.__/|_|  |_|\\___|_|\\_\\___/\n"
    "       |___/\n"
    "________________________________________________________\n"
    "\n";

bool ParseArgs(int argc, char** argv, CliArgs& args) {
    std::cout << kBanner;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << kUsage;
            return false;
        }
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::cout << std::format("hydrobricks version {}.{}.{}, {}\n", HYDROBRICKS_MAJOR_VERSION,
                                     HYDROBRICKS_MINOR_VERSION, HYDROBRICKS_PATCH_VERSION, __DATE__);
            return false;
        }

        auto getNext = [&](const char* name) -> string {
            if (i + 1 >= argc) {
                LogError("Missing value for argument '{}'.", name);
                return "";
            }
            return argv[++i];
        };

        if (std::strcmp(argv[i], "--model-file") == 0) {
            args.modelFile = getNext("--model-file");
        } else if (std::strcmp(argv[i], "--parameters-file") == 0) {
            args.parametersFile = getNext("--parameters-file");
        } else if (std::strcmp(argv[i], "--basin-file") == 0) {
            args.basinFile = getNext("--basin-file");
        } else if (std::strcmp(argv[i], "--data-file") == 0) {
            args.dataFile = getNext("--data-file");
        } else if (std::strcmp(argv[i], "--output-path") == 0) {
            args.outputPath = getNext("--output-path");
        } else if (std::strcmp(argv[i], "--start-date") == 0) {
            args.startDate = getNext("--start-date");
        } else if (std::strcmp(argv[i], "--end-date") == 0) {
            args.endDate = getNext("--end-date");
        } else {
            LogError("Unknown argument '{}'.", argv[i]);
            std::cout << kUsage;
            return false;
        }

        if (args.modelFile.empty() && std::strcmp(argv[i - 1], "--model-file") == 0) return false;
        if (args.parametersFile.empty() && std::strcmp(argv[i - 1], "--parameters-file") == 0) return false;
        if (args.basinFile.empty() && std::strcmp(argv[i - 1], "--basin-file") == 0) return false;
        if (args.dataFile.empty() && std::strcmp(argv[i - 1], "--data-file") == 0) return false;
        if (args.outputPath.empty() && std::strcmp(argv[i - 1], "--output-path") == 0) return false;
        if (args.startDate.empty() && std::strcmp(argv[i - 1], "--start-date") == 0) return false;
        if (args.endDate.empty() && std::strcmp(argv[i - 1], "--end-date") == 0) return false;
    }

    // Validate required arguments
    bool ok = true;
    if (args.modelFile.empty()) {
        LogError("The argument '--model-file' is required.");
        ok = false;
    }
    if (args.parametersFile.empty()) {
        LogError("The argument '--parameters-file' is required.");
        ok = false;
    }
    if (args.basinFile.empty()) {
        LogError("The argument '--basin-file' is required.");
        ok = false;
    }
    if (args.dataFile.empty()) {
        LogError("The argument '--data-file' is required.");
        ok = false;
    }
    if (args.outputPath.empty()) {
        LogError("The argument '--output-path' is required.");
        ok = false;
    }
    if (args.startDate.empty()) {
        LogError("The argument '--start-date' is required.");
        ok = false;
    }
    if (args.endDate.empty()) {
        LogError("The argument '--end-date' is required.");
        ok = false;
    }

    if (!ok) {
        std::cout << kUsage;
    }

    return ok;
}

int RunModel(const CliArgs& args) {
    auto startTime = std::chrono::steady_clock::now();

    // Load basin settings
    SettingsBasin basinSettings;
    if (!basinSettings.Parse(args.basinFile)) {
        LogError("Failed to load basin file '{}'.", args.basinFile);
        return 1;
    }

    // Initialize sub-basin
    SubBasin subBasin;
    if (!subBasin.Initialize(basinSettings)) {
        LogError("Failed to initialize sub-basin.");
        return 1;
    }

    // Load time series
    std::vector<TimeSeries*> vecTimeSeries;
    if (!TimeSeries::Parse(args.dataFile, vecTimeSeries)) {
        LogError("Failed to load data file '{}'.", args.dataFile);
        return 1;
    }

    // Build model settings
    // Note: loading SettingsModel from a YAML file requires model-type-specific
    // construction and is currently only supported via the Python interface.
    LogError("Direct CLI model execution is not yet implemented. Use the Python interface to run models.");
    for (auto* ts : vecTimeSeries) {
        delete ts;
    }
    return 1;
}

int main(int argc, char** argv) {
    try {
        InitHydrobricks();

        CliArgs args;
        if (!ParseArgs(argc, argv, args)) {
            return 0;
        }

        int result = RunModel(args);

        CloseLog();
        return result;

    } catch (const std::exception& e) {
        LogError("Fatal exception: {}", e.what());
        return 1;
    } catch (...) {
        LogError("Unknown fatal exception.");
        return 1;
    }
}
