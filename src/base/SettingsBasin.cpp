#include <netcdf.h>

#include "Parameter.h"
#include "SettingsBasin.h"


SettingsBasin::SettingsBasin()
{
}

SettingsBasin::~SettingsBasin() {
}

void SettingsBasin::AddHydroUnit(int id, double area) {
    HydroUnitSettings unit;
    unit.id = id;
    unit.area = area;
    m_hydroUnits.push_back(unit);
    m_selectedHydroUnit = &m_hydroUnits[m_hydroUnits.size() - 1];
}

void SettingsBasin::AddHydroUnitSurfaceElement(const wxString& name, double fraction) {
    wxASSERT(m_selectedHydroUnit);
    SurfaceElementSettings element;
    element.name = name;
    element.fraction = fraction;
    m_selectedHydroUnit->surfaceElements.push_back(element);
}

void SettingsBasin::SelectUnit(int index) {
    wxASSERT(m_hydroUnits.size() > index);
    m_selectedHydroUnit = &m_hydroUnits[index];
}

bool SettingsBasin::Parse(const wxString &path) {
    if (!wxFile::Exists(path)) {
        wxLogError(_("The file %s could not be found."), path);
        return false;
    }

    try {
        int ncId, varId, dimId;

        CheckNcStatus(nc_open(path, NC_NOWRITE, &ncId));

        // Get the surface names
        size_t surfacesNb;
        CheckNcStatus(nc_inq_attlen(ncId, NC_GLOBAL, "surface_names", &surfacesNb));

        char **stringAtt = (char **) malloc(surfacesNb * sizeof(char *));
        memset(stringAtt, 0, surfacesNb * sizeof(char *));
        CheckNcStatus(nc_get_att_string(ncId, NC_GLOBAL, "surface_names", stringAtt));

        vecStr surfaces;
        for (int iSurface = 0; iSurface < surfacesNb; ++iSurface) {
            wxString attValue = wxString(stringAtt[iSurface], wxConvUTF8);
            surfaces.push_back(attValue);
        }
        nc_free_string(surfacesNb, stringAtt);

        // Get number of units
        size_t unitsNb;
        CheckNcStatus(nc_inq_dimid(ncId, "units", &dimId));
        CheckNcStatus(nc_inq_dimlen(ncId, dimId, &unitsNb));

        // Get ids
        CheckNcStatus(nc_inq_varid(ncId, "id", &varId));
        vecInt ids(unitsNb);
        CheckNcStatus(nc_get_var_int(ncId, varId, &ids[0]));

        // Get areas
        CheckNcStatus(nc_inq_varid(ncId, "area", &varId));
        vecFloat areas(unitsNb);
        CheckNcStatus(nc_get_var_float(ncId, varId, &areas[0]));

        // Get elevations
        CheckNcStatus(nc_inq_varid(ncId, "elevation", &varId));
        vecFloat elevations(unitsNb);
        CheckNcStatus(nc_get_var_float(ncId, varId, &elevations[0]));

        // Store hydro units
        for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
            HydroUnitSettings unit;
            unit.id = ids[iUnit];
            unit.area = areas[iUnit];
            unit.elevation = elevations[iUnit];
            m_hydroUnits.push_back(unit);
        }

        // Get surface data
        for (const auto& surface: surfaces) {
            CheckNcStatus(nc_inq_varid(ncId, surface.mb_str(), &varId));
            vecFloat fractions(unitsNb);
            CheckNcStatus(nc_get_var_float(ncId, varId, &fractions[0]));

            // Get the surface type
            size_t typeLength;
            CheckNcStatus(nc_inq_attlen(ncId, varId, "type", &typeLength));
            auto *text = new char[typeLength + 1];
            CheckNcStatus(nc_get_att_text(ncId, varId, "type", text));
            text[typeLength] = '\0';
            wxString type = wxString(text);
            wxDELETEA(text);

            for (int iUnit = 0; iUnit < unitsNb; ++iUnit) {
                SurfaceElementSettings element;
                element.name = surface;
                element.type = type;
                element.fraction = fractions[iUnit];
                m_hydroUnits[iUnit].surfaceElements.push_back(element);
            }
        }

        CheckNcStatus(nc_close(ncId));

    } catch(std::exception& e) {
        wxLogError(e.what());
        return false;
    }

    return true;
}
