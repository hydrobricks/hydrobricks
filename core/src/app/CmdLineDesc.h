#include <wx/cmdline.h>

static const wxCmdLineEntryDesc cmdLineDesc[] = {
    {wxCMD_LINE_SWITCH, "v", "version", "Show version number and quit"},
    {wxCMD_LINE_OPTION, NULL, "model-file", "Path to the model settings file"},
    {wxCMD_LINE_OPTION, NULL, "parameters-file", "Path to the model parameters file"},
    {wxCMD_LINE_OPTION, NULL, "basin-file", "Path to the spatial structure file"},
    {wxCMD_LINE_OPTION, NULL, "data-file", "Path to the spatial structure file"},
    {wxCMD_LINE_OPTION, NULL, "output-path", "Path to save the output from hydrobricks (no ending backslash)"},
    {wxCMD_LINE_OPTION, NULL, "start-date", "Starting date of the modelling (YYYY-MM-DD)"},
    {wxCMD_LINE_OPTION, NULL, "end-date", "Ending date of the modelling (YYYY-MM-DD)"},
    {wxCMD_LINE_NONE}};

static const wxString cmdLineLogo = wxT(
    "\n"
    "________________________________________________________\n"
    " _               _           _          _      _        \n"
    "| |__  _   _  __| |_ __ ___ | |__  _ __(_) ___| | _____ \n"
    "| '_ \\| | | |/ _` | '__/ _ \\| '_ \\| '__| |/ __| |/ / __|\n"
    "| | | | |_| | (_| | | | (_) | |_) | |  | | (__|   <\\__ \\ \n"
    "|_| |_|\\__, |\\__,_|_|  \\___/|_.__/|_|  |_|\\___|_|\\_\\___/\n"
    "       |___/                                            \n"
    "________________________________________________________\n"
    "\n");

static const wxString cmdLineLogo2 = wxT(
    "\n"
    "________________________________________________________\n"
    " _               _           _          _      _        \n"
    "| |             | |         | |        (_)    | |       \n"
    "| |__  _   _  __| |_ __ ___ | |__  _ __ _  ___| | _____ \n"
    "| '_ \\| | | |/ _` | '__/ _ \\| '_ \\| '__| |/ __| |/ / __|\n"
    "| | | | |_| | (_| | | | (_) | |_) | |  | | (__|   <\\__ \\ \n"
    "|_| |_|\\__, |\\__,_|_|  \\___/|_.__/|_|  |_|\\___|_|\\_\\___/\n"
    "        __/ |                                           \n"
    "       |___/                                            \n"
    "________________________________________________________\n"
    "\n");
