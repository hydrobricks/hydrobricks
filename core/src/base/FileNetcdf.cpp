#include "FileNetcdf.h"

FileNetcdf::FileNetcdf()
    : m_ncId(0) {}

FileNetcdf::~FileNetcdf() {
    CheckNcStatus(nc_close(m_ncId));
}

void FileNetcdf::CheckNcStatus(int status) {
    if (status != NC_NOERR) {
        throw InvalidArgument(nc_strerror(status));
    }
}

bool FileNetcdf::OpenReadOnly(const wxString &path) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    CheckNcStatus(nc_open(path, NC_NOWRITE, &m_ncId));

    return true;
}

bool FileNetcdf::Create(const wxString &path) {
    if (!wxFileName(path).DirExists()) {
        wxLogError(_("The directory %s could not be found."), wxFileName(path).GetPath());
        return false;
    }

    CheckNcStatus(nc_create(path, NC_NETCDF4 | NC_CLOBBER, &m_ncId));

    return true;
}

int FileNetcdf::GetVarsNb() {
    int varsNb, gattNb;
    CheckNcStatus(nc_inq(m_ncId, nullptr, &varsNb, &gattNb, nullptr));

    return varsNb;
}

wxString FileNetcdf::GetVarName(int varId) {
    char varNameChar[NC_MAX_NAME + 1];
    CheckNcStatus(nc_inq_varname(m_ncId, varId, varNameChar));

    return wxString(varNameChar);
}

vecInt FileNetcdf::GetVarDimIds(int varId, int dimNb) {
    vecInt dimIds(dimNb);
    CheckNcStatus(nc_inq_vardimid(m_ncId, varId, &dimIds[0]));

    return dimIds;
}

int FileNetcdf::DefDim(const wxString &dimName, int length) {
    int dimId;
    CheckNcStatus(nc_def_dim(m_ncId, dimName.mb_str(), length, &dimId));

    return dimId;
}

int FileNetcdf::GetDimId(const wxString &dimName) {
    int dimId;
    CheckNcStatus(nc_inq_dimid(m_ncId, dimName.mb_str(), &dimId));

    return dimId;
}

int FileNetcdf::GetDimLen(const wxString &dimName) {
    int dimId;
    size_t dimLen;
    CheckNcStatus(nc_inq_dimid(m_ncId, dimName.mb_str(), &dimId));
    CheckNcStatus(nc_inq_dimlen(m_ncId, dimId, &dimLen));

    return dimLen;
}

int FileNetcdf::DefVarInt(const wxString &varName, vecInt dimIds, int dimsNb, bool compress) {
    int varId;
    CheckNcStatus(nc_def_var(m_ncId, varName, NC_INT, dimsNb, &dimIds[0], &varId));

    if (compress) {
        CheckNcStatus(nc_def_var_deflate(m_ncId, varId, NC_SHUFFLE, true, 7));
    }

    return varId;
}

int FileNetcdf::DefVarFloat(const wxString &varName, vecInt dimIds, int dimsNb, bool compress) {
    int varId;
    CheckNcStatus(nc_def_var(m_ncId, varName, NC_FLOAT, dimsNb, &dimIds[0], &varId));

    if (compress) {
        CheckNcStatus(nc_def_var_deflate(m_ncId, varId, NC_SHUFFLE, true, 7));
    }

    return varId;
}

int FileNetcdf::DefVarDouble(const wxString &varName, vecInt dimIds, int dimsNb, bool compress) {
    int varId;
    CheckNcStatus(nc_def_var(m_ncId, varName, NC_DOUBLE, dimsNb, &dimIds[0], &varId));

    if (compress) {
        CheckNcStatus(nc_def_var_deflate(m_ncId, varId, NC_SHUFFLE, true, 7));
    }

    return varId;
}
vecInt FileNetcdf::GetVarInt1D(const wxString &varName, int size) {
    int varId;
    vecInt items(size);

    CheckNcStatus(nc_inq_varid(m_ncId, varName.mb_str(), &varId));
    CheckNcStatus(nc_get_var_int(m_ncId, varId, &items[0]));

    return items;
}

vecFloat FileNetcdf::GetVarFloat1D(const wxString &varName, int size) {
    int varId;
    vecFloat items(size);

    CheckNcStatus(nc_inq_varid(m_ncId, varName.mb_str(), &varId));
    CheckNcStatus(nc_get_var_float(m_ncId, varId, &items[0]));

    return items;
}

vecDouble FileNetcdf::GetVarDouble1D(const wxString &varName, int size) {
    int varId;
    vecDouble items(size);

    CheckNcStatus(nc_inq_varid(m_ncId, varName.mb_str(), &varId));
    CheckNcStatus(nc_get_var_double(m_ncId, varId, &items[0]));

    return items;
}

axxd FileNetcdf::GetVarDouble2D(int varId, int rows, int cols) {
    axxd values = axxd::Zero((long long)rows, (long long)cols);
    CheckNcStatus(nc_get_var_double(m_ncId, varId, values.data()));

    return values;
}

void FileNetcdf::PutVar(int varId, const vecInt &values) {
    CheckNcStatus(nc_put_var_int(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const vecFloat &values) {
    CheckNcStatus(nc_put_var_float(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const vecDouble &values) {
    CheckNcStatus(nc_put_var_double(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const axd &values) {
    CheckNcStatus(nc_put_var_double(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const vecAxd &values) {
    for (size_t i = 0; i < values.size(); ++i) {
        size_t start[] = {i, 0};
        size_t count[] = {1, (size_t)values[i].size()};
        CheckNcStatus(nc_put_vara_double(m_ncId, varId, start, count, &values[i](0)));
    }
}

void FileNetcdf::PutVar(int varId, const vecAxxd &values) {
    for (size_t i = 0; i < values.size(); ++i) {
        size_t start[] = {i, 0, 0};
        size_t count[] = {1, (size_t)values[i].cols(), (size_t)values[i].rows()};
        CheckNcStatus(nc_put_vara_double(m_ncId, varId, start, count, &values[i](0, 0)));
    }
}

vecStr FileNetcdf::GetAttString1D(const wxString &attName, const wxString &varName) {
    int varId = NC_GLOBAL;
    if (!varName.IsEmpty()) {
        CheckNcStatus(nc_inq_varid(m_ncId, varName.mb_str(), &varId));
    }

    size_t itemsNb;
    CheckNcStatus(nc_inq_attlen(m_ncId, varId, attName.mb_str(), &itemsNb));

    char **stringAtt = (char **)malloc(itemsNb * sizeof(char *));
    memset(stringAtt, 0, itemsNb * sizeof(char *));
    CheckNcStatus(nc_get_att_string(m_ncId, varId, attName.mb_str(), stringAtt));

    vecStr items;
    for (int i = 0; i < itemsNb; ++i) {
        wxString attValue = wxString(stringAtt[i], wxConvUTF8);
        items.push_back(attValue);
    }
    nc_free_string(itemsNb, stringAtt);

    return items;
}

wxString FileNetcdf::GetAttText(const wxString &attName, const wxString &varName) {
    int varId = NC_GLOBAL;
    if (!varName.IsEmpty()) {
        CheckNcStatus(nc_inq_varid(m_ncId, varName.mb_str(), &varId));
    }

    size_t attLen;
    CheckNcStatus(nc_inq_attlen(m_ncId, varId, attName.mb_str(), &attLen));

    auto *text = new char[attLen + 1];
    CheckNcStatus(nc_get_att_text(m_ncId, varId, attName.mb_str(), text));
    text[attLen] = '\0';
    wxString value = wxString(text);
    wxDELETEA(text);

    return value;
}

void FileNetcdf::PutAttText(const wxString &attName, const wxString &value, int varId) {
    wxCharBuffer buffer = value.ToUTF8();
    CheckNcStatus(nc_put_att_text(m_ncId, varId, attName.mb_str(), strlen(buffer.data()), buffer.data()));
}

void FileNetcdf::PutAttString(const wxString &attName, const vecStr &values, int varId) {
    std::vector<const char *> valuesChar;
    for (const auto &label : values) {
        const char *str = (const char *)label.mb_str(wxConvUTF8);
        valuesChar.push_back(str);
    }
    CheckNcStatus(nc_put_att_string(m_ncId, varId, attName.mb_str(), values.size(), &valuesChar[0]));
}