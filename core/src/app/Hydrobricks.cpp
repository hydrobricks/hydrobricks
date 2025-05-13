#include "Hydrobricks.h"

#include <wx/fileconf.h>
#include <wx/filefn.h>
#include <wx/log.h>

#if _DEBUG
#include <wx/debug.h>
#endif

#include "CmdLineDesc.h"
#include "ModelHydro.h"
#include "SettingsBasin.h"
#include "SettingsModel.h"
#include "SubBasin.h"
#include "TimeSeries.h"
#include "Utils.h"

wxIMPLEMENT_APP_CONSOLE(Hydrobricks);

bool Hydrobricks::OnInit() {
#if _DEBUG
#ifdef __WXMSW__
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#endif

    // Set application name
    wxString appName = "hydrobricks";
    wxApp::SetAppName(appName);

    // Init
    InitHydrobricks();

    // Call default action
    if (!wxApp::OnInit()) {
        CleanUp();
        return false;
    }

    return true;
}

void Hydrobricks::OnInitCmdLine(wxCmdLineParser& parser) {
    wxAppConsole::OnInitCmdLine(parser);

    parser.SetDesc(cmdLineDesc);
    parser.SetLogo(cmdLineLogo);

    // Must refuse '/' as parameter starter or cannot use "/path" style paths
    parser.SetSwitchChars(wxT("-"));
}

bool Hydrobricks::OnCmdLineParsed(wxCmdLineParser& parser) {
    // Add a new line
    wxPrintf("\n");

    // Check if the user asked for command-line help
    if (parser.Found("help")) {
        parser.Usage();
        return false;
    }

    // Check if the user asked for the version
    if (parser.Found("version")) {
        wxString version = wxString::Format("%d.%d.%d", HYDROBRICKS_MAJOR_VERSION, HYDROBRICKS_MINOR_VERSION,
                                            HYDROBRICKS_PATCH_VERSION);
        wxPrintf("hydrobricks version %s, %s", version, static_cast<const wxChar*>(wxString::FromAscii(__DATE__)));
        return false;
    }

    /*
     * Hydrobricks inputs
     */
    wxString input = wxEmptyString;

    if (parser.Found("model-file", &input)) {
        m_modelFile = input;
    } else {
        wxLogError("The argument 'model-file' is missing.");
        return false;
    }

    if (parser.Found("parameters-file", &input)) {
        m_parametersFile = input;
    } else {
        wxLogError("The argument 'parameters-file' is missing.");
        return false;
    }

    if (parser.Found("basin-file", &input)) {
        m_basinFile = input;
    } else {
        wxLogError("The argument 'basin-file' is missing.");
        return false;
    }

    if (parser.Found("data-file", &input)) {
        m_dataFile = input;
    } else {
        wxLogError("The argument 'data-file' is missing.");
        return false;
    }

    if (parser.Found("output-path", &input)) {
        m_outputPath = input;
    } else {
        wxLogError("The argument 'output-path' is missing.");
        return false;
    }

    if (parser.Found("start-date", &input)) {
        m_startDate = input;
    } else {
        wxLogError("The argument 'start-date' is missing.");
        return false;
    }

    if (parser.Found("end-date", &input)) {
        m_endDate = input;
    } else {
        wxLogError("The argument 'end-date' is missing.");
        return false;
    }

    return wxAppConsole::OnCmdLineParsed(parser);
}

int Hydrobricks::OnRun() {
    try {
        // Create the output path if needed
        if (!CheckOutputDirectory(m_outputPath)) {
            return 1;
        }

        // Initialize log
        InitLog(m_outputPath);

        // Model settings
        SettingsModel modelSettings;
        if (!modelSettings.ParseStructure(m_modelFile)) {
            return 1;
        }

        // Modelling period
        modelSettings.SetTimer(m_startDate, m_endDate, 1, "day");

        // Parameters
        if (!modelSettings.ParseParameters(m_parametersFile)) {
            return 1;
        }

        // Basin settings
        SettingsBasin basinSettings;
        if (!basinSettings.Parse(m_basinFile)) {
            return 1;
        }

        // Data
        vector<TimeSeries*> vecTimeSeries;
        if (!TimeSeries::Parse(m_dataFile, vecTimeSeries)) {
            return 1;
        }

        // Watch
        wxStopWatch sw;

        // Create the basin
        SubBasin subBasin;
        if (!subBasin.Initialize(basinSettings)) {
            return 1;
        }

        // Create the model
        ModelHydro model(&subBasin);
        if (!model.Initialize(modelSettings, basinSettings)) {
            return 1;
        }

        // Add data
        for (auto timeSeries : vecTimeSeries) {
            if (!model.AddTimeSeries(timeSeries)) {
                return 1;
            }
        }
        if (!model.AttachTimeSeriesToHydroUnits()) {
            return 1;
        }

        // Check
        if (!model.IsOk()) {
            return 1;
        }

        // Do the work
        if (!model.Run()) {
            return 1;
        }

        // Save outputs
        if (!model.DumpOutputs(m_outputPath)) {
            return 1;
        }

        // Processing time and sources number
        DisplayProcessingTime(sw);
        wxLogMessage(_("Calculation over."));

        return 0;

    } catch (std::bad_alloc& ba) {
        wxString msg(ba.what(), wxConvUTF8);
        wxLogError(_("Bad allocation caught: %s"), msg);
        CleanUp();
        return 1;
    } catch (std::exception& e) {
        wxString fullMessage(e.what());
        wxLogError(fullMessage);
        wxLogError(_("Exception caught."));
        CleanUp();
        return 1;
    }

    return wxApp::OnRun();
}

int Hydrobricks::OnExit() {
    CleanUp();

#ifdef _DEBUG
#ifdef __WXMSW__
    _CrtDumpMemoryLeaks();
#endif
#endif

    return 0;
}

void Hydrobricks::CleanUp() {
    wxFileConfig::Get()->Flush();
    delete wxFileConfig::Set((wxFileConfig*)nullptr);

    wxApp::CleanUp();
}

bool Hydrobricks::OnExceptionInMainLoop() {
    wxLogError(_("An exception occurred."));
    CleanUp();
    return false;
}

void Hydrobricks::OnFatalException() {
    wxLogError(_("A fatal exception occurred."));
    CleanUp();
}

void Hydrobricks::OnUnhandledException() {
    wxLogError(_("An unhandled exception occurred."));
    CleanUp();
}
