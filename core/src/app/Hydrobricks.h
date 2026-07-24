#ifndef HYDROBRICKS_APP_H
#define HYDROBRICKS_APP_H

#include "Includes.h"

struct CliArgs {
    string modelFile;
    string parametersFile;
    string basinFile;
    string dataFile;
    string outputPath;
    string startDate;
    string endDate;
};

/**
 * Parse command-line arguments.
 *
 * @param argc argument count.
 * @param argv argument values.
 * @param args output struct with parsed arguments.
 * @return true if parsing succeeded; false if --help or --version was printed or an error occurred.
 */
bool ParseArgs(int argc, char** argv, CliArgs& args);

/**
 * Run the hydrological model with the given arguments.
 *
 * @param args parsed CLI arguments.
 * @return 0 on success, non-zero on error.
 */
int RunModel(const CliArgs& args);

#endif  // HYDROBRICKS_APP_H
