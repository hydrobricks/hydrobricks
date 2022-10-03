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

class Hydrobricks : public wxAppConsole {
  public:
    bool OnInit() override;

    int OnRun() override;

    int OnExit() override;

    void CleanUp() override;

    void OnInitCmdLine(wxCmdLineParser& parser) override;

    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

    bool OnExceptionInMainLoop() override;

    void OnFatalException() override;

    void OnUnhandledException() override;

    void OnDisplaySecurityMessage(wxThreadEvent& event);

  protected:
    std::string m_modelFile;
    std::string m_parametersFile;
    std::string m_basinFile;
    std::string m_dataFile;
    std::string m_outputPath;
    std::string m_startDate;
    std::string m_endDate;

    wxString GetUserDirPath();

    void InitializeLog();

    bool CheckOutputDirectory();

    void DisplayProcessingTime(const wxStopWatch& sw);

  private:
};

DECLARE_APP(Hydrobricks);

#endif  // HYDROBRICKS_APP_H
