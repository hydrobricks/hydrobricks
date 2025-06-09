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
    /**
     * Initializes the application.
     *
     * @return true if the initialization was successful.
     */
    bool OnInit() override;

    /**
     * Runs the application.
     *
     * @return 0 if the run was successful.
     */
    int OnRun() override;

    /**
     * Exits the application.
     *
     * @return 0 if the exit was successful.
     */
    int OnExit() override;

    /**
     * Cleans up the application.
     */
    void CleanUp() override;

    /**
     * Initializes the command line parser.
     *
     * @param parser the command line parser.
     */
    void OnInitCmdLine(wxCmdLineParser& parser) override;

    /**
     * Parses the command line arguments.
     *
     * @param parser the command line parser.
     * @return true if the parsing was successful.
     */
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

    /**
     * Handles exceptions in the main loop.
     *
     * @return true if the exception was handled.
     */
    bool OnExceptionInMainLoop() override;

    /**
     * Handles fatal exceptions.
     */
    void OnFatalException() override;

    /**
     * Handles unhandled exceptions.
     */
    void OnUnhandledException() override;

    /**
     * Handles security messages.
     *
     * @param event the security message event.
     */
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
