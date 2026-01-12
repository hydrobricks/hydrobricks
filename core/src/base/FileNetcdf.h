#ifndef HYDROBRICKS_FILE_NETCDF_H
#define HYDROBRICKS_FILE_NETCDF_H

#include <netcdf.h>

#include "Includes.h"

class FileNetcdf : public wxObject {
  public:
    explicit FileNetcdf();

    ~FileNetcdf() override;

    /**
     * Open a NetCDF file as read only.
     *
     * @param path Path of the existing file.
     * @return True if successful, false otherwise
     */
    bool OpenReadOnly(const string& path);

    /**
     * Create a NetCDF file at the given path.
     *
     * @param path Path of the file to be created.
     * @return True if successful, false otherwise.
     */
    bool Create(const string& path);

    /**
     * Close the NetCDF file. Not mandatory as the file is closed in the destructor if still opened.
     */
    void Close();

    /**
     * Get the number of variables in the file.
     *
     * @return the number of variables.
     */
    int GetVariableCount() const;

    /**
     * Get the variable id corresponding to the provided name.
     *
     * @param varName The name of the variable of interest.
     * @return The id of the requested variable.
     */
    int GetVarId(const string& varName) const;

    /**
     * Get the variable name corresponding to the provided id.
     *
     * @param varId The id of the variable of interest.
     * @return The name of the requested variable.
     */
    string GetVarName(int varId) const;

    /**
     * Get the dimension ids of the provided variable.
     *
     * @param varId The id of the variable of interest.
     * @param dimCount The number of dimensions of the variable.
     * @return A vector of the dimension ids.
     */
    vecInt GetVarDimIds(int varId, int dimCount) const;

    /**
     * Define a new dimension.
     *
     * @param dimName Name of the new dimension.
     * @param length Length of the new dimension.
     * @return The new dimension id.
     */
    int DefDim(const string& dimName, int length);

    /**
     * Get a dimension ID.
     *
     * @param dimName The dimension name of interest.
     * @return The id of the requested dimension.
     */
    int GetDimId(const string& dimName) const;

    /**
     * Get a dimension length.
     *
     * @param dimName The name of the dimension.
     * @return The length of the dimension.
     */
    int GetDimLen(const string& dimName) const;

    /**
     * Define a new integer variable.
     *
     * @param varName Name of the new variable.
     * @param dimIds The corresponding dimension ids.
     * @param dimCount The number of corresponding dimensions.
     * @param compress Option to compress the variable values (default: false).
     * @return The new variable id.
     */
    int DefVarInt(const string& varName, vecInt dimIds, int dimCount = 1, bool compress = false);

    /**
     * Define a new float variable.
     *
     * @param varName Name of the new variable.
     * @param dimIds The corresponding dimension ids.
     * @param dimCount The number of corresponding dimensions.
     * @param compress Option to compress the variable values (default: false).
     * @return The new variable id.
     */
    int DefVarFloat(const string& varName, vecInt dimIds, int dimCount = 1, bool compress = false);

    /**
     * Define a new double variable.
     *
     * @param varName Name of the new variable.
     * @param dimIds The corresponding dimension ids.
     * @param dimCount The number of corresponding dimensions.
     * @param compress Option to compress the variable values (default: false).
     * @return The new variable id.
     */
    int DefVarDouble(const string& varName, vecInt dimIds, int dimCount = 1, bool compress = false);

    /**
     * Get the values of a 1D integer variable. The whole vector retrieved at once.
     *
     * @param varName The name of the variable of interest.
     * @param size The size of the data vector.
     * @return A vector containing the data.
     */
    vecInt GetVarInt1D(const string& varName, int size) const;

    /**
     * Get the values of a 1D float variable. The whole vector retrieved at once.
     *
     * @param varName The name of the variable of interest.
     * @param size The size of the data vector.
     * @return A vector containing the data.
     */
    vecFloat GetVarFloat1D(const string& varName, int size) const;

    /**
     * Get the values of a 1D double variable. The whole vector retrieved at once.
     *
     * @param varName The name of the variable of interest.
     * @param size The size of the data vector.
     * @return A vector containing the data.
     */
    vecDouble GetVarDouble1D(const string& varName, int size) const;

    /**
     * Get values of a 2D double variable. The whole array retrieved at once.
     *
     * @param varId The id of the variable of interest.
     * @param rows The number of rows of the data array.
     * @param cols The number of columns of the data array.
     * @return An array containing the data.
     */
    axxd GetVarDouble2D(int varId, int rows, int cols) const;

    /**
     * Set the variable values from a vector of integers.
     *
     * @param varId The id of the variable of interest.
     * @param values The data to store.
     */
    void PutVar(int varId, const vecInt& values);

    /**
     * Set the variable values from a vector of floats.
     *
     * @param varId The id of the variable of interest.
     * @param values The data to store.
     */
    void PutVar(int varId, const vecFloat& values);

    /**
     * Set the variable values from a vector of doubles.
     *
     * @param varId The id of the variable of interest.
     * @param values The data to store.
     */
    void PutVar(int varId, const vecDouble& values);

    /**
     * Set the variable values from a 1D array of doubles.
     *
     * @param varId The id of the variable of interest.
     * @param values The data to store.
     */
    void PutVar(int varId, const axd& values);

    /**
     * Set the variable values from a vector of 1D arrays of doubles.
     *
     * @param varId The id of the variable of interest.
     * @param values The data to store.
     */
    void PutVar(int varId, const vecAxd& values);

    /**
     * Set the variable values from a vector of 2D arrays of doubles.
     *
     * @param varId The id of the variable of interest.
     * @param values The data to store.
     */
    void PutVar(int varId, const vecAxxd& values);

    /**
     * Check if a variable exists.
     *
     * @param varName The variable name.
     * @return True if the attribute exists, false otherwise.
     */
    bool HasVar(const string& varName) const;

    /**
     * Check if an attribute exists.
     *
     * @param attName The attribute name.
     * @param varName The variable name. If empty, search in the global attributes.
     * @return True if the attribute exists, false otherwise.
     */
    bool HasAtt(const string& attName, const string& varName = "") const;

    /**
     * Get a string vector stored as an attribute.
     *
     * @param attName The attribute name.
     * @param varName The variable name. If empty, search in the global attributes.
     * @return A string vector containing the data.
     */
    vecStr GetAttString1D(const string& attName, const string& varName = "") const;

    /**
     * Store a string vector as an attribute.
     *
     * @param attName The attribute name.
     * @param values A string vector containing the data.
     * @param varId The variable id. If empty, search in the global attributes.
     */
    void PutAttString(const string& attName, const vecStr& values, int varId = NC_GLOBAL);

    /**
     * Get a text attribute.
     *
     * @param attName The name of the attribute.
     * @param varName The name of the variable (empty for global attribute).
     * @return The value of the attribute.
     */
    string GetAttText(const string& attName, const string& varName = "") const;

    /**
     * Store a string as an attribute.
     *
     * @param attName The attribute name.
     * @param value The string to store.
     * @param varId The variable id. If empty, search in the global attributes.
     */
    void PutAttText(const string& attName, const string& value, int varId = NC_GLOBAL);

  protected:
    int _ncId;

  private:
    /**
     * Check the NetCDF status and throw an exception if an error occurred.
     *
     * @param status The NetCDF status to check.
     */
    void CheckNcStatus(int status) const;
};

#endif  // HYDROBRICKS_FILE_NETCDF_H
