#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <wx/log.h>

#include "Behaviour.h"
#include "BehaviourLandCoverChange.h"
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

    m.def("init", &InitHydrobricksForPython, "Initializes hydrobricks.");
    m.def("init_log", &InitLog, "Initializes log.", "path"_a);
    m.def("close_log", &CloseLog, "Closes the log.");
    m.def("set_max_log_level", &SetMaxLogLevel, "Set the log level to max (max verbosity).");
    m.def("set_debug_log_level", &SetDebugLogLevel, "Set the log level to debug.");
    m.def("set_message_log_level", &SetMessageLogLevel, "Set the log level to message (standard).");

    py::class_<SettingsModel>(m, "SettingsModel")
        .def(py::init<>())
        .def("log_all", &SettingsModel::SetLogAll, "Logging all components.", "log_all"_a = true)
        .def("add_logging_to", &SettingsModel::AddLoggingToItem, "Add logging to the item.", "name"_a)
        .def("set_solver", &SettingsModel::SetSolver, "Set the solver.", "name"_a)
        .def("set_timer", &SettingsModel::SetTimer, "Set the modelling time properties.", "start_date"_a, "end_date"_a,
             "time_step"_a, "time_step_unit"_a)
        .def("add_land_cover_brick", &SettingsModel::AddLandCoverBrick, "Add a land cover brick.", "name"_a, "kind"_a)
        .def("add_hydro_unit_brick", &SettingsModel::AddHydroUnitBrick, "Add a hydro unit brick.", "name"_a,
             "kind"_a = "storage")
        .def("add_sub_basin_brick", &SettingsModel::AddSubBasinBrick, "Add a sub basin brick.", "name"_a,
             "kind"_a = "storage")
        .def("select_hydro_unit_brick", &SettingsModel::SelectHydroUnitBrickByName, "Select a hydro unit brick.",
             "name"_a)
        .def("add_brick_process", &SettingsModel::AddBrickProcess, "Add a process to a brick.", "name"_a, "kind"_a,
             "target"_a = "", "log"_a = false)
        .def("add_process_parameter", &SettingsModel::AddProcessParameter, "Add a process parameter.", "name"_a,
             "value"_a, "kind"_a = "constant")
        .def("add_process_forcing", &SettingsModel::AddProcessForcing, "Add a process forcing.", "name"_a)
        .def("add_brick_parameter", &SettingsModel::AddBrickParameter, "Add a brick parameter.", "name"_a, "value"_a,
             "kind"_a = "constant")
        .def("set_parameter_value", &SettingsModel::SetParameterValue, "Setting one of the model parameter.",
             "component"_a, "name"_a, "value"_a)
        .def("generate_precipitation_splitters", &SettingsModel::GeneratePrecipitationSplitters,
             "Generate the precipitation splitters.", "with_snow"_a = true)
        .def("generate_snowpacks", &SettingsModel::GenerateSnowpacks, "Generate the snowpack.", "snow_melt_process"_a)
        .def("set_process_outputs_as_instantaneous", &SettingsModel::SetProcessOutputsAsInstantaneous,
             "Set the process outputs as instantaneous.")
        .def("set_process_outputs_as_static", &SettingsModel::SetProcessOutputsAsStatic,
             "Set the process outputs as static.");

    py::class_<SettingsBasin>(m, "SettingsBasin")
        .def(py::init<>())
        .def("add_hydro_unit", &SettingsBasin::AddHydroUnit, "Add a hydro unit to the spatial structure.", "id"_a,
             "area"_a)
        .def("add_land_cover", &SettingsBasin::AddLandCover, "Add a land cover element.", "name"_a, "kind"_a,
             "fraction"_a)
        .def("add_hydro_unit_property_str", &SettingsBasin::AddHydroUnitPropertyString, "Set a hydro unit property.",
             "name"_a, "value"_a)
        .def("add_hydro_unit_property_double", &SettingsBasin::AddHydroUnitPropertyDouble, "Set a hydro unit property.",
             "name"_a, "value"_a, "unit"_a)
        .def("clear", &SettingsBasin::Clear, "Clear the basin settings.");

    py::class_<SubBasin>(m, "SubBasin")
        .def(py::init<>())
        .def("init", &SubBasin::Initialize, "Initialize the basin.", "spatial_structure"_a);

    py::class_<Parameter>(m, "Parameter")
        .def(py::init<const string&, float>())
        .def_property("name", &Parameter::GetName, &Parameter::SetName)
        .def_property("value", &Parameter::GetValue, &Parameter::SetValue)
        .def("get_name", &Parameter::GetName, "Get the parameter name.")
        .def("set_name", &Parameter::SetName, "Set the parameter name.")
        .def("get_value", &Parameter::GetValue, "Get the parameter value.")
        .def("set_value", &Parameter::SetValue, "Set the parameter value.")
        .def("__repr__", [](const Parameter& a) { return "<_hydrobricks.Parameter named '" + a.GetName() + "'>"; });

    py::class_<ParameterVariableYearly, Parameter>(m, "ParameterVariableYearly")
        .def(py::init<const string&>())
        .def("set_values", &ParameterVariableYearly::SetValues, "Set the parameter values.", "year_start"_a,
             "year_end"_a, "values"_a);

    py::class_<TimeSeries>(m, "TimeSeries")
        .def_static("create", &TimeSeries::Create, "data_name"_a, "time"_a, "ids"_a, "data"_a);

    py::class_<ModelHydro>(m, "ModelHydro")
        .def(py::init<>())
        .def("init_with_basin", &ModelHydro::InitializeWithBasin, "Initialize the model and create the sub basin.",
             "model_settings"_a, "basin_settings"_a)
        .def("add_behaviour", &ModelHydro::AddBehaviour, "Adding a behaviour to the model.", "behaviour"_a)
        .def("get_behaviours_nb", &ModelHydro::GetBehavioursNb, "Get the number of behaviours.")
        .def("get_behaviour_items_nb", &ModelHydro::GetBehaviourItemsNb, "Get the number of behaviour items.")
        .def("add_time_series", &ModelHydro::AddTimeSeries, "Adding a time series to the model.", "time_series"_a)
        .def("create_time_series", &ModelHydro::CreateTimeSeries, "Create a time series and add it to the model.",
             "data_name"_a, "time"_a, "ids"_a, "data"_a)
        .def("clear_time_series", &ModelHydro::ClearTimeSeries,
             "Clear time series. Use only if the time series were created with ModelHydro::ClearTimeSeries.")
        .def("attach_time_series_to_hydro_units", &ModelHydro::AttachTimeSeriesToHydroUnits, "Attach the time series.")
        .def("update_parameters", &ModelHydro::UpdateParameters, "Update the parameters with the provided values.",
             "model_settings"_a)
        .def("forcing_loaded", &ModelHydro::ForcingLoaded, "Check if the forcing data were loaded.")
        .def("is_ok", &ModelHydro::IsOk, "Check if the model is correctly set up.")
        .def("run", &ModelHydro::Run, "Run the model.")
        .def("reset", &ModelHydro::Reset, "Reset the model before another run.")
        .def("save_as_initial_state", &ModelHydro::SaveAsInitialState, "Save the model state as initial conditions.")
        .def("get_outlet_discharge", &ModelHydro::GetOutletDischarge, "Get the outlet discharge.")
        .def("get_total_outlet_discharge", &ModelHydro::GetTotalOutletDischarge, "Get the outlet discharge total.")
        .def("get_total_et", &ModelHydro::GetTotalET, "Get the total amount of water lost by evapotranspiration.")
        .def("get_total_water_storage_changes", &ModelHydro::GetTotalWaterStorageChanges,
             "Get the total change in water storage.")
        .def("get_total_snow_storage_changes", &ModelHydro::GetTotalSnowStorageChanges,
             "Get the total change in snow storage.")
        .def("dump_outputs", &ModelHydro::DumpOutputs, "Dump the model outputs to file.", "path"_a);

    py::class_<Behaviour>(m, "Behaviour").def(py::init<>());

    py::class_<BehaviourLandCoverChange, Behaviour>(m, "BehaviourLandCoverChange")
        .def(py::init<>())
        .def("add_change", &BehaviourLandCoverChange::AddChange, "date"_a, "hydro_unit_id"_a, "land_cover"_a, "area"_a)
        .def("get_changes_nb", &BehaviourLandCoverChange::GetChangesNb)
        .def("get_land_covers_nb", &BehaviourLandCoverChange::GetLandCoversNb);

    py::class_<wxLogNull>(m, "LogNull").def(py::init<>());
}
