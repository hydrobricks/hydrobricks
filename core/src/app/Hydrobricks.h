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

#include "Includes.h"

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
    string m_modelFile;
    string m_parametersFile;
    string m_basinFile;
    string m_dataFile;
    string m_outputPath;
    string m_startDate;
    string m_endDate;

  private:
};

DECLARE_APP(Hydrobricks);

#endif  // HYDROBRICKS_APP_H
