#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <wx/log.h>

#include "Includes.h"
#include "ModelHydro.h"
#include "Parameter.h"
#include "ParameterVariable.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"
#include "Utils.h"

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(_hydrobricks, m) {
    m.doc() = "hydrobricks Python interface";

    m.def("init", &InitHydrobricksForPython, "Initializes hydrobricks");
    m.def("init_log", &InitLog, "Initializes log", "path"_a);

    py::class_<SettingsModel>(m, "ModelStructure")
        .def(py::init<>())
        .def("generate_socont_structure", &SettingsModel::GenerateStructureSocont, "Generate the GSM-SOCONT structure.",
             "surface_types"_a, "surface_names"_a, "soil_storage_nb"_a = 1, "surface_runoff"_a = "socont-runoff")
        .def("set_timer", &SettingsModel::SetTimer, "Set the modelling time properties", "start_date"_a, "end_date"_a,
             "time_step"_a, "time_step_unit"_a)
        .def("set_parameter", &SettingsModel::SetParameter, "Setting one of the model parameter", "component"_a,
             "name"_a, "value"_a);

    py::class_<SettingsBasin>(m, "SpatialStructure").def(py::init<>());

    py::class_<SubBasin>(m, "SubBasin")
        .def(py::init<>())
        .def("init", &SubBasin::Initialize, "Initialize the basin", "spatial_structure"_a)
        .def("assign_fractions", &SubBasin::AssignFractions, "Assign the surface fractions", "spatial_structure"_a);

    py::class_<Parameter>(m, "Parameter")
        .def(py::init<const std::string&, float>())
        .def_property("name", &Parameter::GetName, &Parameter::SetName)
        .def_property("value", &Parameter::GetValue, &Parameter::SetValue)
        .def("get_name", &Parameter::GetName, "Get the parameter name")
        .def("set_name", &Parameter::SetName, "Set the parameter name")
        .def("get_value", &Parameter::GetValue, "Get the parameter value")
        .def("set_value", &Parameter::SetValue, "Set the parameter value")
        .def("__repr__", [](const Parameter& a) { return "<_hydrobricks.Parameter named '" + a.GetName() + "'>"; });

    py::class_<ParameterVariableYearly, Parameter>(m, "ParameterVariableYearly")
        .def(py::init<const std::string&>())
        .def("set_values", &ParameterVariableYearly::SetValues, "Set the parameter values", "year_start"_a,
             "year_end"_a, "values"_a);

    py::class_<TimeSeries>(m, "TimeSeries")
        .def_static("create", &TimeSeries::Create, "data_name"_a, "time"_a, "ids"_a, "data"_a);

    py::class_<ModelHydro>(m, "ModelHydro")
        .def(py::init<>())
        .def("set_sub_basin", &ModelHydro::SetSubBasin, "Set the basin", "sub_basin"_a)
        .def("add_time_series", &ModelHydro::AddTimeSeries, "Adding a time series to the model", "time_series"_a)
        .def("attach_time_series_to_hydro_units", &ModelHydro::AttachTimeSeriesToHydroUnits, "Attach the time series.")
        .def("is_ok", &ModelHydro::IsOk, "Check if the model is correctly set up.")
        .def("run", &ModelHydro::Run, "Run the model.")
        .def("dump_outputs", &ModelHydro::DumpOutputs, "Dump the model outputs to file.", "path"_a);

    py::class_<wxLogNull>(m, "LogNull").def(py::init<>());
}
