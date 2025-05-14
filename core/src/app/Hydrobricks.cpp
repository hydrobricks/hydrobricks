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
