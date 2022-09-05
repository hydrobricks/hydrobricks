#ifndef HYDROBRICKS_FILE_NETCDF_H
#define HYDROBRICKS_FILE_NETCDF_H

#include <netcdf.h>

#include "Includes.h"

class FileNetcdf : public wxObject {
  public:
    explicit FileNetcdf();

    ~FileNetcdf() override;

    void CheckNcStatus(int status);

    bool OpenReadOnly(const wxString &path);

    bool Create(const wxString &path);

    void Close();

    int GetVarsNb();

    int GetVarId(const wxString &varName);

    wxString GetVarName(int varId);

    vecInt GetVarDimIds(int varId, int dimNb);

    int DefDim(const wxString &dimName, int length);

    int GetDimId(const wxString &dimName);

    int GetDimLen(const wxString &dimName);

    int DefVarInt(const wxString &varName, vecInt dimIds, int dimsNb = 1, bool compress = false);

    int DefVarFloat(const wxString &varName, vecInt dimIds, int dimsNb = 1, bool compress = false);

    int DefVarDouble(const wxString &varName, vecInt dimIds, int dimsNb = 1, bool compress = false);

    vecInt GetVarInt1D(const wxString &varName, int size);

    vecFloat GetVarFloat1D(const wxString &varName, int size);

    vecDouble GetVarDouble1D(const wxString &varName, int size);

    axxd GetVarDouble2D(int varId, int rows, int cols);

    void PutVar(int varId, const vecInt &values);

    void PutVar(int varId, const vecFloat &values);

    void PutVar(int varId, const vecDouble &values);

    void PutVar(int varId, const axd &values);

    void PutVar(int varId, const vecAxd &values);

    void PutVar(int varId, const vecAxxd &values);

    vecStr GetAttString1D(const wxString &attName, const wxString &varName = wxEmptyString);

    void PutAttString(const wxString &attName, const vecStr &values, int varId = NC_GLOBAL);

    wxString GetAttText(const wxString &attName, const wxString &varName = wxEmptyString);

    void PutAttText(const wxString &attName, const wxString &value, int varId = NC_GLOBAL);

  protected:
    int m_ncId;

  private:
};

#endif  // HYDROBRICKS_FILE_NETCDF_H
