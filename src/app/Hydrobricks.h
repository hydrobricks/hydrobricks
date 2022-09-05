#ifndef HYDROBRICKS_APP_H
#define HYDROBRICKS_APP_H

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/socket.h>


class Hydrobricks : public wxAppConsole
{
public:
    bool OnInit() override;

    int OnRun() override;

    int OnExit() override;

    void CleanUp() override;

    void OnInitCmdLine(wxCmdLineParser &parser) override;

    bool OnCmdLineParsed(wxCmdLineParser &parser) override;

    bool OnExceptionInMainLoop() override;

    void OnFatalException() override;

    void OnUnhandledException() override;

    void OnDisplaySecurityMessage(wxThreadEvent &event);

protected:
    wxString m_modelFile;
    wxString m_parametersFile;
    wxString m_basinFile;
    wxString m_dataFile;
    wxString m_outputPath;
    wxString m_startDate;
    wxString m_endDate;

    wxString GetUserDirPath();

    void InitializeLog();

    bool CheckOutputDirectory();

    void DisplayProcessingTime(const wxStopWatch &sw);

private:
};

DECLARE_APP(Hydrobricks);

#endif // HYDROBRICKS_APP_H
