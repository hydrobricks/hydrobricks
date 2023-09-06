#include "FileNetcdf.h"

FileNetcdf::FileNetcdf()
    : m_ncId(-1) {}

FileNetcdf::~FileNetcdf() {
    if (m_ncId >= 0) {
        CheckNcStatus(nc_close(m_ncId));
    }
}

void FileNetcdf::CheckNcStatus(int status) {
    if (status != NC_NOERR) {
        throw InvalidArgument(nc_strerror(status));
    }
}

bool FileNetcdf::OpenReadOnly(const string& path) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    CheckNcStatus(nc_open(path.c_str(), NC_NOWRITE, &m_ncId));

    return true;
}

bool FileNetcdf::Create(const string& path) {
    if (!wxFileName(path).DirExists()) {
        wxLogError(_("The directory %s could not be found."), wxFileName(path).GetPath());
        return false;
    }

    CheckNcStatus(nc_create(path.c_str(), NC_NETCDF4 | NC_CLOBBER, &m_ncId));

    return true;
}

void FileNetcdf::Close() {
    CheckNcStatus(nc_close(m_ncId));
    m_ncId = -1;
}

int FileNetcdf::GetVarsNb() {
    int varsNb, gattNb;
    CheckNcStatus(nc_inq(m_ncId, nullptr, &varsNb, &gattNb, nullptr));

    return varsNb;
}

int FileNetcdf::GetVarId(const string& varName) {
    int varId;
    CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));

    return varId;
}

string FileNetcdf::GetVarName(int varId) {
    char varNameChar[NC_MAX_NAME + 1];
    CheckNcStatus(nc_inq_varname(m_ncId, varId, varNameChar));

    return {varNameChar};
}

vecInt FileNetcdf::GetVarDimIds(int varId, int dimNb) {
    vecInt dimIds(dimNb);
    CheckNcStatus(nc_inq_vardimid(m_ncId, varId, &dimIds[0]));

    return dimIds;
}

int FileNetcdf::DefDim(const string& dimName, int length) {
    int dimId;
    CheckNcStatus(nc_def_dim(m_ncId, dimName.c_str(), length, &dimId));

    return dimId;
}

int FileNetcdf::GetDimId(const string& dimName) {
    int dimId;
    CheckNcStatus(nc_inq_dimid(m_ncId, dimName.c_str(), &dimId));

    return dimId;
}

int FileNetcdf::GetDimLen(const string& dimName) {
    int dimId;
    size_t dimLen;
    CheckNcStatus(nc_inq_dimid(m_ncId, dimName.c_str(), &dimId));
    CheckNcStatus(nc_inq_dimlen(m_ncId, dimId, &dimLen));

    return dimLen;
}

int FileNetcdf::DefVarInt(const string& varName, vecInt dimIds, int dimsNb, bool compress) {
    int varId;
    CheckNcStatus(nc_def_var(m_ncId, varName.c_str(), NC_INT, dimsNb, &dimIds[0], &varId));

    if (compress) {
        CheckNcStatus(nc_def_var_deflate(m_ncId, varId, NC_SHUFFLE, true, 7));
    }

    return varId;
}

int FileNetcdf::DefVarFloat(const string& varName, vecInt dimIds, int dimsNb, bool compress) {
    int varId;
    CheckNcStatus(nc_def_var(m_ncId, varName.c_str(), NC_FLOAT, dimsNb, &dimIds[0], &varId));

    if (compress) {
        CheckNcStatus(nc_def_var_deflate(m_ncId, varId, NC_SHUFFLE, true, 7));
    }

    return varId;
}

int FileNetcdf::DefVarDouble(const string& varName, vecInt dimIds, int dimsNb, bool compress) {
    int varId;
    CheckNcStatus(nc_def_var(m_ncId, varName.c_str(), NC_DOUBLE, dimsNb, &dimIds[0], &varId));

    if (compress) {
        CheckNcStatus(nc_def_var_deflate(m_ncId, varId, NC_SHUFFLE, true, 7));
    }

    return varId;
}

vecInt FileNetcdf::GetVarInt1D(const string& varName, int size) {
    int varId;
    vecInt items(size);

    CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));
    CheckNcStatus(nc_get_var_int(m_ncId, varId, &items[0]));

    return items;
}

vecFloat FileNetcdf::GetVarFloat1D(const string& varName, int size) {
    int varId;
    vecFloat items(size);

    CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));
    CheckNcStatus(nc_get_var_float(m_ncId, varId, &items[0]));

    return items;
}

vecDouble FileNetcdf::GetVarDouble1D(const string& varName, int size) {
    int varId;
    vecDouble items(size);

    CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));
    CheckNcStatus(nc_get_var_double(m_ncId, varId, &items[0]));

    return items;
}

axxd FileNetcdf::GetVarDouble2D(int varId, int rows, int cols) {
    axxd values = axxd::Zero((long long)rows, (long long)cols);
    CheckNcStatus(nc_get_var_double(m_ncId, varId, values.data()));

    return values;
}

void FileNetcdf::PutVar(int varId, const vecInt& values) {
    CheckNcStatus(nc_put_var_int(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const vecFloat& values) {
    CheckNcStatus(nc_put_var_float(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const vecDouble& values) {
    CheckNcStatus(nc_put_var_double(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const axd& values) {
    CheckNcStatus(nc_put_var_double(m_ncId, varId, &values[0]));
}

void FileNetcdf::PutVar(int varId, const vecAxd& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        size_t start[] = {i, 0};
        size_t count[] = {1, (size_t)values[i].size()};
        CheckNcStatus(nc_put_vara_double(m_ncId, varId, start, count, &values[i](0)));
    }
}

void FileNetcdf::PutVar(int varId, const vecAxxd& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        size_t start[] = {i, 0, 0};
        size_t count[] = {1, (size_t)values[i].cols(), (size_t)values[i].rows()};
        CheckNcStatus(nc_put_vara_double(m_ncId, varId, start, count, &values[i](0, 0)));
    }
}

bool FileNetcdf::HasVar(const string& varName) {
    int varId;

    return nc_inq_varid(m_ncId, varName.c_str(), &varId) != NC_ENOTVAR;
}

bool FileNetcdf::HasAtt(const string& attName, const string& varName) {
    int varId = NC_GLOBAL;
    if (!varName.empty()) {
        CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));
    }

    return nc_inq_att(m_ncId, varId, attName.c_str(), nullptr, nullptr) != NC_ENOTATT;
}

vecStr FileNetcdf::GetAttString1D(const string& attName, const string& varName) {
    int varId = NC_GLOBAL;
    if (!varName.empty()) {
        CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));
    }

    size_t itemsNb;
    CheckNcStatus(nc_inq_attlen(m_ncId, varId, attName.c_str(), &itemsNb));

    char** stringAtt = static_cast<char**>(malloc(itemsNb * sizeof(char*)));
    memset(stringAtt, 0, itemsNb * sizeof(char*));
    CheckNcStatus(nc_get_att_string(m_ncId, varId, attName.c_str(), stringAtt));

    vecStr items;
    for (int i = 0; i < itemsNb; ++i) {
        string attValue = string(stringAtt[i]);
        items.push_back(attValue);
    }
    nc_free_string(itemsNb, stringAtt);

    return items;
}

void FileNetcdf::PutAttString(const string& attName, const vecStr& values, int varId) {
    vector<const char*> valuesChar;
    for (const auto& label : values) {
        const char* str = static_cast<const char*>(label.c_str());
        valuesChar.push_back(str);
    }
    CheckNcStatus(nc_put_att_string(m_ncId, varId, attName.c_str(), values.size(), &valuesChar[0]));
}

string FileNetcdf::GetAttText(const string& attName, const string& varName) {
    int varId = NC_GLOBAL;
    if (!varName.empty()) {
        CheckNcStatus(nc_inq_varid(m_ncId, varName.c_str(), &varId));
    }

    size_t attLen;
    CheckNcStatus(nc_inq_attlen(m_ncId, varId, attName.c_str(), &attLen));

    auto* text = new char[attLen + 1];
    CheckNcStatus(nc_get_att_text(m_ncId, varId, attName.c_str(), text));
    text[attLen] = '\0';
    string value = string(text);
    wxDELETEA(text);

    return value;
}

void FileNetcdf::PutAttText(const string& attName, const string& value, int varId) {
    wxCharBuffer buffer = wxString(value).ToUTF8();
    CheckNcStatus(nc_put_att_text(m_ncId, varId, attName.c_str(), strlen(buffer.data()), buffer.data()));
}
