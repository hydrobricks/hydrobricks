#include "Hydrobricks.h"

#include <gdal_priv.h>
#include <wx/fileconf.h>
#include <wx/filefn.h>
#include <wx/stdpaths.h>
#include <wx/log.h>
#include <wx/ffile.h>

#if _DEBUG
    #include <wx/debug.h>
#endif

#include "CmdLineDesc.h"
#include "SettingsModel.h"
#include "TimeSeries.h"
#include "SettingsBasin.h"
#include "SubBasin.h"
#include "ModelHydro.h"


wxIMPLEMENT_APP_CONSOLE(Hydrobricks);

bool Hydrobricks::OnInit()
{
#if _DEBUG
    #ifdef __WXMSW__
        _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    #endif
#endif

    // Set application name
    wxString appName = "hydrobricks";
    wxApp::SetAppName(appName);
    wxFileName userDir = wxFileName::DirName(GetUserDirPath());
    userDir.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    // Set config file
	wxFileName filePath = wxFileConfig::GetLocalFile("hydrobricks", wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_SUBDIR);
	filePath.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
	wxFileConfig *pConfig = new wxFileConfig("hydrobricks", wxEmptyString, filePath.GetFullPath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_SUBDIR);
	wxFileConfig::Set(pConfig);

    // Initialize GDAL
    GDALAllRegister();

    // Call default behaviour
    if (!wxApp::OnInit()) {
        CleanUp();
        return false;
    }

    return true;
}

wxString Hydrobricks::GetUserDirPath()
{
    wxStandardPathsBase &stdPth = wxStandardPaths::Get();
    stdPth.UseAppInfo(0);
    wxString userDir = stdPth.GetUserDataDir();
    userDir.Append(wxFileName::GetPathSeparator());
    userDir.Append("hydrobricks");
    userDir.Append(wxFileName::GetPathSeparator());

    return userDir;
}

void Hydrobricks::OnInitCmdLine(wxCmdLineParser &parser)
{
    wxAppConsole::OnInitCmdLine(parser);

    parser.SetDesc(cmdLineDesc);
    parser.SetLogo(cmdLineLogo);

    // Must refuse '/' as parameter starter or cannot use "/path" style paths
    parser.SetSwitchChars(wxT("-"));
}

bool Hydrobricks::OnCmdLineParsed(wxCmdLineParser &parser)
{
    // Add a new line
    wxPrintf("\n");

    // Check if the user asked for command-line help
    if (parser.Found("help")) {
        parser.Usage();
        return false;
    }

    // Check if the user asked for the version
    if (parser.Found("version")) {
        wxString version = wxString::Format("%d.%d.%d", HYDROBRICKS_MAJOR_VERSION, HYDROBRICKS_MINOR_VERSION, HYDROBRICKS_PATCH_VERSION);
        wxPrintf("hydrobricks version %s, %s", version, (const wxChar *) wxString::FromAscii(__DATE__));
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

int Hydrobricks::OnRun()
{
    try {
        // Create the output path if needed
        if (!CheckOutputDirectory()) {
            return 1;
        }

        // Initialize log
        InitializeLog();

        // Model settings
        SettingsModel modelSettings;
        if (!modelSettings.ParseStructure(m_modelFile)) {
            return 1;
        }

        // Modelling period
        modelSettings.SetTimer(m_startDate, m_endDate, 1, "Day");

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
        std::vector<TimeSeries*> vecTimeSeries;
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
        if (!model.Initialize(modelSettings)) {
            return 1;
        }

        // Add data
        for (auto timeSeries: vecTimeSeries) {
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

    } catch (std::bad_alloc &ba) {
        wxString msg(ba.what(), wxConvUTF8);
        wxLogError(_("Bad allocation caught: %s"), msg);
        CleanUp();
        return 1;
    } catch (std::exception &e) {
        wxString fullMessage(e.what());
        wxLogError(fullMessage);
        wxLogError(_("Exception caught."));
        CleanUp();
        return 1;
    }

    return wxApp::OnRun();
}

int Hydrobricks::OnExit()
{
    CleanUp();

    #ifdef _DEBUG
        #ifdef __WXMSW__
            _CrtDumpMemoryLeaks();
        #endif
    #endif

    return 0;
}

void Hydrobricks::CleanUp()
{
	wxFileConfig::Get()->Flush();
    delete wxFileConfig::Set((wxFileConfig *) nullptr);

    wxApp::CleanUp();
}

bool Hydrobricks::OnExceptionInMainLoop()
{
    wxLogError(_("An exception occurred."));
    CleanUp();
    return false;
}

void Hydrobricks::OnFatalException()
{
    wxLogError(_("A fatal exception occurred."));
    CleanUp();
}

void Hydrobricks::OnUnhandledException()
{
    wxLogError(_("An unhandled exception occurred."));
    CleanUp();
}

void Hydrobricks::InitializeLog() {
    wxString fullPath = m_outputPath;
    wxString logFileName = "hydrobricks.log";
    fullPath.Append(wxFileName::GetPathSeparator());
    fullPath.Append(logFileName);
    wxFFile *logFile = new wxFFile(fullPath, "w");
    auto *pLogFile = new wxLogStderr(logFile->fp());
    new wxLogChain(pLogFile);
    wxString version = wxString::Format("%d.%d.%d", HYDROBRICKS_MAJOR_VERSION,
                                        HYDROBRICKS_MINOR_VERSION,
                                        HYDROBRICKS_PATCH_VERSION);
    wxLogMessage("hydrobricks version %s, %s", version, (const wxChar *) wxString::FromAscii(__DATE__));
}

bool Hydrobricks::CheckOutputDirectory() {
    if (!wxFileName::DirExists(m_outputPath)) {
        wxFileName dir = wxFileName(m_outputPath);
        if (!dir.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
            wxLogError(_("The path %s could not be created."), m_outputPath);
            return false;
        }
    }

    return true;
}

void Hydrobricks::DisplayProcessingTime(const wxStopWatch &sw)
{
    auto dispTime = float(sw.Time());
    wxString watchUnit = "ms";
    if (dispTime > 3600000) {
        dispTime = dispTime / 3600000.0f;
        watchUnit = "h";
    } else if (dispTime > 60000) {
        dispTime = dispTime / 60000.0f;
        watchUnit = "min";
    } else if (dispTime > 1000) {
        dispTime = dispTime / 1000.0f;
        watchUnit = "s";
    }
    wxLogMessage(_("Processing time: %.2f %s."), dispTime, watchUnit);
}