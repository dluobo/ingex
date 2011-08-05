/***************************************************************************
 *   $Id: ingexgui.cpp,v 1.46 2011/08/05 11:05:35 john_f Exp $           *
 *                                                                         *
 *   Copyright (C) 2006-2011 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
 *   Author: Matthew Marks                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <wx/wx.h>
#include "wx/tooltip.h" //for SetDelay()
#include "wx/splitter.h"
#include "wx/filename.h"
#include "wx/cmdline.h"
#include "wx/notebook.h"
#include "help.h"
#include "jogshuttle.h"
#include "JogShuttle.h"
#include "eventlist.h"
#include "savedstate.h"
#include "colours.h"

#include "ingexgui.xpm"
#include "stop.xpm"
#include "record.xpm"
#include "play.xpm"
#include "play_backwards.xpm"
#include "paused.xpm"
#include "happy.xpm"
#include "concerned.xpm"
#include "exclamation.xpm"
#include "ffx2.xpm"
#include "ffx4.xpm"
#include "ffx8.xpm"
#include "ffx16.xpm"
#include "ffx32.xpm"
#include "ffx64.xpm"
#include "frx2.xpm"
#include "frx4.xpm"
#include "frx8.xpm"
#include "frx16.xpm"
#include "frx32.xpm"
#include "frx64.xpm"
//#include "question.xpm"
#include "blank.xpm"

#include "ticktree.h"
#include "timepos.h"
#include "dialogues.h"
#include "player.h"
#include "recordbutton.h"
#include "recordergroup.h"

//#include <wx/tooltip.h>

#include <wx/arrimpl.cpp>
#include <fstream>
WX_DEFINE_OBJARRAY(ChunkInfoArray);
WX_DEFINE_OBJARRAY(ArrayOfTrackList_var);
WX_DEFINE_OBJARRAY(ArrayOfStringSeq_var);

DEFINE_EVENT_TYPE(EVT_LOGGING_MESSAGE)

BEGIN_EVENT_TABLE( MyTextCtrl, wxTextCtrl )
    EVT_SET_FOCUS( MyTextCtrl::OnFocus )
    EVT_KILL_FOCUS( MyTextCtrl::OnFocus )
END_EVENT_TABLE()


BEGIN_EVENT_TABLE( IngexguiFrame, wxFrame )
    EVT_CLOSE( IngexguiFrame::OnClose )
    EVT_MENU( MENU_Help, IngexguiFrame::OnHelp )
    EVT_MENU( MENU_About, IngexguiFrame::OnAbout )
    EVT_MENU( BUTTON_MENU_Record, IngexguiFrame::OnRecord )
    EVT_MENU( MENU_MarkCue, IngexguiFrame::OnCue )
    EVT_MENU( BUTTON_MENU_Stop, IngexguiFrame::OnStop )
    EVT_MENU( MENU_PrevTrack, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_NextTrack, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_StepBackwards, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_StepForwards, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_Up, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_Down, IngexguiFrame::OnShortcut )
    EVT_MENU( BUTTON_MENU_PrevTake, IngexguiFrame::OnShortcut )
    EVT_MENU( BUTTON_MENU_NextTake, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_FirstTake, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_LastTake, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_JumpToTimecode, IngexguiFrame::OnJumpToTimecode )
    EVT_MENU( MENU_PlayBackwards, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_Pause, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_PlayForwards, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_PlayPause, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_Mute, IngexguiFrame::OnShortcut )
    EVT_MENU( MENU_ClearLog, IngexguiFrame::OnClearLog )
    EVT_MENU( MENU_PlayerDisable, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_OpenMOV, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_OpenMXF, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_OpenServer, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerAbsoluteTimecode, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerRelativeTimecode, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerNoOSD, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerOSDTop, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerOSDMiddle, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerOSDBottom, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerNoLevelMeters, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerTwoLevelMeters, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerFourLevelMeters, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerEightLevelMeters, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerSixteenLevelMeters, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerAudioFollowsVideo, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerLimitSplitToQuad, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerDisableScalingFiltering, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerAccelOutput, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerEnableSDIOSD, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerExtOutput, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerExtAccelOutput, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerExtUnaccelOutput, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( MENU_PlayerUnaccelOutput, IngexguiFrame::OnPlayerCommand )
    EVT_MENU( wxID_CLOSE, IngexguiFrame::OnQuit )
    EVT_MENU( MENU_SetProjectName, IngexguiFrame::OnSetProjectName )
    EVT_MENU( MENU_SetRolls, IngexguiFrame::OnSetRolls )
    EVT_MENU( MENU_SetCues, IngexguiFrame::OnSetCues )
    EVT_MENU( MENU_Chunking, IngexguiFrame::OnChunking )
    EVT_MENU( MENU_TestMode, IngexguiFrame::OnTestMode )
    EVT_BUTTON( BUTTON_RecorderListRefresh, IngexguiFrame::OnRecorderListRefresh )
    EVT_BUTTON( BUTTON_TapeId, IngexguiFrame::OnSetTapeIds )
    EVT_BUTTON( BUTTON_MENU_Record, IngexguiFrame::OnRecord )
    EVT_BUTTON( BUTTON_MENU_Stop, IngexguiFrame::OnStop )
    EVT_BUTTON( BUTTON_Cue, IngexguiFrame::OnCue )
    EVT_BUTTON( BUTTON_Chunk, IngexguiFrame::OnChunkButton )
    EVT_BUTTON( BUTTON_ClearDescription, IngexguiFrame::OnClearDescription )
    EVT_TEXT_ENTER( TEXTCTRL_Description, IngexguiFrame::OnDescriptionEnterKey )
    EVT_SET_FOCUS( IngexguiFrame::OnFocusGot )
    EVT_KILL_FOCUS( IngexguiFrame::OnFocusLost )
    EVT_LIST_ITEM_SELECTED( wxID_ANY, IngexguiFrame::OnEventSelection )
    EVT_LIST_ITEM_ACTIVATED( wxID_ANY, IngexguiFrame::OnEventActivated )
    EVT_LIST_BEGIN_LABEL_EDIT( wxID_ANY, IngexguiFrame::OnEventBeginEdit )
    EVT_LIST_END_LABEL_EDIT( wxID_ANY, IngexguiFrame::OnEventEndEdit )
    EVT_BUTTON( BUTTON_MENU_PrevTake, IngexguiFrame::OnShortcut )
    EVT_BUTTON( BUTTON_MENU_NextTake, IngexguiFrame::OnShortcut )
    EVT_BUTTON( BUTTON_JumpToTimecode, IngexguiFrame::OnJumpToTimecode )
    EVT_BUTTON( BUTTON_TakeSnapshot, IngexguiFrame::OnTakeSnapshot )
    EVT_BUTTON( BUTTON_DeleteCue, IngexguiFrame::OnDeleteCue )
    EVT_MENU( MENU_UseTapeIds, IngexguiFrame::OnUseTapeIds )
    EVT_COMMAND( wxID_ANY, EVT_PLAYER_MESSAGE, IngexguiFrame::OnPlayerEvent )
    EVT_COMMAND( wxID_ANY, EVT_TREE_MESSAGE, IngexguiFrame::OnTreeEvent )
    EVT_COMMAND( wxID_ANY, EVT_RECORDERGROUP_MESSAGE, IngexguiFrame::OnRecorderGroupEvent )
    EVT_COMMAND( wxID_ANY, EVT_JOGSHUTTLE_MESSAGE, IngexguiFrame::OnJogShuttleEvent )
    EVT_COMMAND( wxID_ANY, EVT_TIMEPOS_EVENT, IngexguiFrame::OnTimeposEvent )
    EVT_COMMAND( wxID_ANY, EVT_JUMP_TO_TIMECODE, IngexguiFrame::OnJumpToTimecodeEvent )
    EVT_COMMAND( wxID_ANY, EVT_LOGGING_MESSAGE, IngexguiFrame::OnLoggingEvent )
    EVT_MENU( MENU_PlayRecordings, IngexguiFrame::OnPlayerCommand )
#ifndef DISABLE_SHARED_MEM_SOURCE
    EVT_MENU( MENU_EtoE, IngexguiFrame::OnPlayerCommand )
#endif
    EVT_MENU( MENU_PlayFiles, IngexguiFrame::OnPlayerCommand )
    EVT_UPDATE_UI(wxID_ANY, IngexguiFrame::OnUpdateUI)
END_EVENT_TABLE()

IMPLEMENT_APP(IngexguiApp)

/// Overridden Initialisation function to allow XInitThreads to be called before gtk_init_check() (which has been known to cause a crash otherwise).
/// This method is not in the wx documentation but is mentioned in wx/app.h.
bool IngexguiApp::Initialize(int& argc, wxChar **argv)
{
    XInitThreads(); //This must be called before any other X lib functions in order for multithreaded X apps (i.e. the player) to work properly.  Otherwise XLockDisplay sometimes doesn't lock, resulting in the player hanging until its window gets an event such as focus or resize
    return wxApp::Initialize(argc, argv);
}

/// Application initialisation: creates and displays the main frame.
/// @return Always true.
bool IngexguiApp::OnInit()
{
    IngexguiFrame *frame = new IngexguiFrame(argc, argv);

    frame->Show(true);
    SetTopWindow(frame);
    return true;
} 

//Need to do this because accelerator/menu shortcuts don't always work
//Even so, sometimes (when control that takes cursor keys doesn't have focus?), no key events are generated for cursor keys
int IngexguiApp::FilterEvent(wxEvent& event)
{
//return -1;
    bool intercepted = false;
    if (wxEVT_CHAR == event.GetEventType()) {
        wxKeyEvent * keyEvent = dynamic_cast<wxKeyEvent *>(&event);
        intercepted = true;
        wxCommandEvent guiMenuEvent(wxEVT_COMMAND_MENU_SELECTED);
        switch (keyEvent->GetKeyCode()) {
            case WXK_UP : //controls get this before menu (at least in GTK)
                guiMenuEvent.SetId(IngexguiFrame::MENU_Up);
                break;
            case WXK_DOWN : //controls get this before menu (at least in GTK)
                guiMenuEvent.SetId(IngexguiFrame::MENU_Down);
                break;
//          case WXK_PAGEUP : //menus don't recognise this as a shortcut (at least in GTK)
//              guiMenuEvent.SetId(IngexguiFrame::MENU_PrevTake);
//              GetTopWindow()->AddPendingEvent(guiMenuEvent);
////                guiButtonEvent.SetId(IngexguiFrame::BUTTON_PrevTake);
////                GetTopWindow()->AddPendingEvent(guiButtonEvent);
//              break;
//          case WXK_PAGEDOWN : //menus don't recognise this as a shortcut (at least in GTK)
//              guiButtonEvent.SetId(IngexguiFrame::BUTTON_NextTake);
//              GetTopWindow()->AddPendingEvent(guiButtonEvent);
//              break;

//          case ' ' :
//              break;
            default:
                intercepted = false;
        }
        if (intercepted) {
            GetTopWindow()->AddPendingEvent(guiMenuEvent);
        }
    }


    if (intercepted) {
        return true; //if false is returned, the event is processed again
    }
    else {
        return -1; //process as normal
    }
}


/// Lays out all controls and menus, and creates persistent objects.
/// @param argc Command line argument count.
/// @param argv Command line argument vector.
IngexguiFrame::IngexguiFrame(int argc, wxChar** argv)
    : wxFrame((wxFrame *) 0, FRAME, wxT("ingexgui")/*, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS* - this doesn't prevent cursor keys being lost, as hoped */), mPlayer(0), mStatus(STOPPED), mTextFieldHasFocus(false), mToday(wxDateTime::Today()), mSnapshotIndex(1), mSnapshotPath(SNAPSHOT_PATH), mSetProjectDlg(0)
{
    wxUpdateUIEvent::SetUpdateInterval(-1); //disable control updates until all controls have been created; otherwise, things like the warning dialogues will allow events to be generated which will then cause a crash
    //logging
    wxLog::SetActiveTarget(new wxLogStream(new ofstream(wxDateTime::Now().Format(wxT("ingexguiLog-%y%m%d-%H%M%S")).mb_str(*wxConvCurrent))));
        Log(wxT("Controller started on ") + wxDateTime::Now().FormatISODate());

    //icon
    wxIcon icon(ingexgui_xpm);
    SetIcon(icon);

    //command line/recorder group - recorder group needs to see the command line to send parameters to the ORB
    // create char * [] version of argv, required by the ORB.  Do it here so that the changes the ORB makes are carried through
//      char * argv_[argc]; //gcc likes this but Win32 doesn't
    char ** argv_ = new char * [argc];
    for (int i = 0; i < argc; i++) {
        wxString wxArg = argv[i];
        char * arg = new char[wxArg.Len() + 1];
        strcpy(arg, wxArg.mb_str());
        argv_[i] = arg;
    }
    wxSize size = wxDefaultSize;
    size.SetHeight(100); //bodge to show at least four lines
    mRecorderGroup = new RecorderGroupCtrl(this, wxID_ANY, wxDefaultPosition, size, argc, argv_); //do this here to allow the ORB to remove its options, which would otherwise prevent parsing of the command line.
    wxCmdLineParser parser(argc, argv_);
    parser.AddSwitch(wxT("h"), wxT("help"), wxT("Display this help message"), wxCMD_LINE_OPTION_HELP);
    parser.AddSwitch(wxT("n"), wxEmptyString, wxT("Do not load recording list files"));
    parser.AddSwitch(wxT("p"), wxEmptyString, wxT("Disable player"));
    parser.AddOption(wxT("r"), wxEmptyString, wxString::Format(wxT("Root path for recordings playback dialogue (defaults to %s)"), RECORDING_SERVER_ROOT)); //FIXME: not Windows-compatible
    parser.AddOption(wxT("s"), wxEmptyString, wxString::Format(wxT("Path for snapshots (defaults to %s)"), SNAPSHOT_PATH)); //FIXME: not Windows-compatible
    parser.AddOption(wxEmptyString, wxT("ORB..."), wxT("ORB options, such as \"-ORBDefaultInitRef corbaloc:iiop:192.168.1.123:8888\" (note: SINGLE dash - not as shown at the start of this line)"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE); //a dummy option simply to augment the usage message
    if (parser.Parse()) {
        Log(wxT("Application closed due to incorrect arguments.")); //this can segfault if done after Destroy()
        Destroy();
        exit(1);
    }
    delete[] argv_;
    wxString recordingServerRoot = RECORDING_SERVER_ROOT;
    parser.Found(wxT("r"), &recordingServerRoot);
    parser.Found(wxT("s"), &mSnapshotPath);

    mTestModeDlg = new TestModeDlg(this, BUTTON_MENU_Record, BUTTON_MENU_Stop); //persistent so that it can remember its parameters between invocations; create early as it's called when populating the events list

    //menu
    wxMenuBar * menuBar = new wxMenuBar;

    //Misc menu
    wxMenu * menuMisc = new wxMenu;
    menuMisc->Append(MENU_SetProjectName, wxT("Set project name..."));
    menuMisc->Append(MENU_SetRolls, wxT("Set pre- and post-roll..."));
    menuMisc->Append(MENU_SetCues, wxT("Cue points..."));
    menuMisc->Append(MENU_Chunking, wxT("Chunking..."));
    wxMenuItem * clearLogItem = menuMisc->Append(MENU_ClearLog, wxT("Clear recording log"));
    clearLogItem->Enable(false);
    menuMisc->AppendCheckItem(MENU_UseTapeIds, wxT("Use tape IDs"));
    menuMisc->AppendCheckItem(MENU_AutoClear, wxT("Don't log multiple recordings"));
    menuMisc->Append(MENU_TestMode, wxT("Test mode..."));
    menuMisc->Append(wxID_CLOSE, wxT("Quit"));
    menuBar->Append(menuMisc, wxT("&Misc"));

    //Player menu
    if (!parser.Found(wxT("p"))) {
        wxMenu * menuPlayer = new wxMenu;
        menuPlayer->AppendCheckItem(MENU_PlayerDisable, wxT("Disable player"));

        wxMenu * menuPlayerOpen = new wxMenu;
        menuPlayerOpen->Append(MENU_OpenServer, wxT("From server..."));
        menuPlayerOpen->Append(MENU_OpenMOV, wxT("Open MOV file..."));
        menuPlayerOpen->Append(MENU_OpenMXF, wxT("Open MXF file(s)..."));
        menuPlayer->Append(MENU_PlayerOpen, wxT("Open"), menuPlayerOpen);

        wxMenu * menuPlayerType = new wxMenu;
        menuPlayerType->AppendRadioItem(MENU_PlayerExtAccelOutput, wxEmptyString);
        menuPlayerType->AppendRadioItem(MENU_PlayerAccelOutput, wxEmptyString);
        menuPlayerType->AppendRadioItem(MENU_PlayerExtOutput, wxEmptyString);
        menuPlayerType->AppendRadioItem(MENU_PlayerExtUnaccelOutput, wxEmptyString);
        menuPlayerType->AppendRadioItem(MENU_PlayerUnaccelOutput, wxEmptyString);
        menuPlayer->Append(MENU_PlayerType, wxT("Player type"), menuPlayerType);

        wxMenu * menuPlayerOSD = new wxMenu;
        menuPlayerOSD->AppendRadioItem(MENU_PlayerAbsoluteTimecode, wxT("&Absolute timecode"));
        menuPlayerOSD->AppendRadioItem(MENU_PlayerRelativeTimecode, wxT("&Relative timecode"));
        menuPlayerOSD->AppendRadioItem(MENU_PlayerNoOSD, wxT("&OSD Off"));
        menuPlayer->Append(wxID_ANY, wxT("Player On-Screen Display Type"), menuPlayerOSD);

        wxMenu * menuPlayerOSDPos = new wxMenu;
        menuPlayerOSDPos->AppendRadioItem(MENU_PlayerOSDTop, wxT("&Top"));
        menuPlayerOSDPos->AppendRadioItem(MENU_PlayerOSDMiddle, wxT("&Middle"));
        menuPlayerOSDPos->AppendRadioItem(MENU_PlayerOSDBottom, wxT("&Bottom"));
        menuPlayer->Append(wxID_ANY, wxT("Player On-Screen Display Position"), menuPlayerOSDPos);

        wxMenu * menuPlayerNumLevelMeters = new wxMenu;
        menuPlayerNumLevelMeters->AppendRadioItem(MENU_PlayerNoLevelMeters, wxT("&None"));
        menuPlayerNumLevelMeters->AppendRadioItem(MENU_PlayerTwoLevelMeters, wxT("&Two"));
        menuPlayerNumLevelMeters->AppendRadioItem(MENU_PlayerFourLevelMeters, wxT("&Four"));
        menuPlayerNumLevelMeters->AppendRadioItem(MENU_PlayerEightLevelMeters, wxT("&Eight"));
        menuPlayerNumLevelMeters->AppendRadioItem(MENU_PlayerSixteenLevelMeters, wxT("&Sixteen"));
        menuPlayer->Append(wxID_ANY, wxT("Maximum number of audio level meters"), menuPlayerNumLevelMeters);

        menuPlayer->AppendCheckItem(MENU_PlayerAudioFollowsVideo, wxT("Audio corresponds to video source displayed"));
        menuPlayer->AppendCheckItem(MENU_PlayerLimitSplitToQuad, wxT("Limit split view to quad split"));
        menuPlayer->AppendCheckItem(MENU_PlayerDisableScalingFiltering, wxT("Disable scaling filters (saves processing power)"));
        menuPlayer->AppendCheckItem(MENU_PlayerEnableSDIOSD, wxT("Enable external monitor On-screen Display"));
        menuBar->Append(menuPlayer, wxT("&Player"));
    }
    //Help menu
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(MENU_Help, wxT("&Help..."));
    menuHelp->Append(MENU_About, wxT("&About..."));
    menuBar->Append(menuHelp, wxT("&Help"));

    //Shortcuts menu
    mMenuShortcuts = new wxMenu;
    mMenuShortcuts->Append(MENU_Help, wxT("Help\tCTRL-H"));
    mMenuShortcuts->Append(BUTTON_MENU_Record, wxT("Record\tF1"));
    mMenuShortcuts->Append(MENU_MarkCue, wxT("Mark Cue\tF2")); //NB this must correspond to the "insert blank cue" key in CuePointsDlg
    mMenuShortcuts->Append(BUTTON_MENU_Stop, wxT("Stop\tShift+F5"));
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_PrevTrack, wxT("Select previous playback track\tF7"));
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_NextTrack, wxT("Select next playback track\tF8"));
    mMenuShortcuts->Append(MENU_Up); //label set dynamically
    mMenuShortcuts->Append(MENU_Down); //label set dynamically
    mMenuShortcuts->Append(BUTTON_MENU_PrevTake); //label set dynamically
    mMenuShortcuts->Append(BUTTON_MENU_NextTake); //label set dynamically
    mMenuShortcuts->Append(MENU_FirstTake, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    mMenuShortcuts->Append(MENU_LastTake, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_JumpToTimecode, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_PlayBackwards, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_Pause, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_PlayForwards, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_PlayPause, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_StepBackwards, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_StepForwards, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_OpenServer, wxT("Open recording from server\tCTRL-O"));

    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_PlayRecordings, wxT("Play Recordings\tF10"));
#ifndef DISABLE_SHARED_MEM_SOURCE
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_EtoE, wxT("Play E to E\tF11"));
#endif
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_PlayFiles, wxT("Play Opened Files\tF12"));
    if (!parser.Found(wxT("p"))) mMenuShortcuts->Append(MENU_Mute, wxEmptyString); //dynamically set to allow shortcut to be switched on and off
    menuHelp->Append(wxID_ANY, wxT("Shortcuts"), mMenuShortcuts); //add this after adding menu items to it

    SetMenuBar(menuBar);
    CreateStatusBar();

    //window
    wxBoxSizer * sizer1V = new wxBoxSizer(wxVERTICAL);
    SetSizer(sizer1V);

    //recorder selection row
    wxBoxSizer * sizer2aH = new wxBoxSizer(wxHORIZONTAL);
    sizer1V->Add(sizer2aH, 0, wxEXPAND);
    wxStaticBoxSizer * recorderSelectionBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Recorder Selection"));
    sizer2aH->Add(recorderSelectionBox, 1, wxALL, CONTROL_BORDER);

    recorderSelectionBox->Add(mRecorderGroup, 1, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
    recorderSelectionBox->Add(new wxButton(this, BUTTON_RecorderListRefresh, wxT("Refresh list")), 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
    sizer2aH->Add(new wxButton(this, BUTTON_TapeId, wxT("Set tape IDs")), 0, wxALL | wxALIGN_CENTRE_VERTICAL, CONTROL_BORDER);
    //indicator row
    wxBoxSizer * sizer2bH = new wxBoxSizer(wxHORIZONTAL);
    sizer1V->Add(sizer2bH, 0, wxEXPAND);
    sizer2bH->Add(new wxStaticBitmap(this, BITMAP_StatusCtrl, wxBitmap(blank)), 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);

    wxStaticBoxSizer* timecodeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Studio timecode"));
    timecodeBox->GetStaticBox()->SetId(TIMECODE_BOX);
    sizer2bH->Add(timecodeBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
    wxStaticText * timecodeDisplay = new wxStaticText(this, -1, UNKNOWN_TIMECODE); //need to set the contents to read the correct size for the position box and display
    wxFont* font = wxFont::New(TIME_FONT_SIZE, wxFONTFAMILY_MODERN); //this way works under GTK
    timecodeDisplay->SetFont(*font);
    timecodeDisplay->SetToolTip(wxT("Time-of-day timecode"));
    timecodeBox->Add(timecodeDisplay, 0, wxALL | wxALIGN_CENTRE, CONTROL_BORDER);
    sizer2bH->Add(new wxStaticBitmap(this, BITMAP_Alert, wxBitmap(blank)), 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE, CONTROL_BORDER);
    wxStaticBoxSizer * positionBox = new wxStaticBoxSizer(wxHORIZONTAL, this, wxT("Position"));
    sizer2bH->Add(positionBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    wxStaticText* positionDisplay = new wxStaticText(this, -1, NO_POSITION, wxDefaultPosition, timecodeDisplay->GetSize());
    positionDisplay->SetFont(*font);
    delete font;
    positionDisplay->SetToolTip(wxT("Current recording's position"));
    positionBox->Add(positionDisplay, 0, wxALIGN_CENTRE);

    if (!parser.Found(wxT("p"))) {
        mPlayer = new Player(this, wxID_ANY, true, recordingServerRoot);
        sizer2bH->Add(mPlayer, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    }
    //transport controls
    wxBoxSizer* sizer2cH = new wxBoxSizer(wxHORIZONTAL);
    sizer1V->Add(sizer2cH);
    RecordButton* recordButton = new RecordButton(this, BUTTON_MENU_Record, wxT("Record (999 sources)")); //to set a minimum size
    recordButton->SetMinSize(recordButton->GetSize());
    sizer2cH->Add(recordButton, 0, wxALL, CONTROL_BORDER);
    sizer2cH->Add(new wxButton(this, BUTTON_MENU_Stop, wxT("Stop")), 0, wxALL, CONTROL_BORDER);
    sizer2cH->Add(new wxButton(this, BUTTON_Cue, wxT("Mark Cue")), 0, wxALL, CONTROL_BORDER);
    wxButton* chunkButton = new wxButton(this, BUTTON_Chunk, wxT("Chunk ???:??")); //to set a minimum size
    chunkButton->SetMinSize(chunkButton->GetSize());
    sizer2cH->Add(chunkButton, 0, wxALL, CONTROL_BORDER);
    //splitter window containing everything else
    wxSplitterWindow * splitterWindow = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D);
    sizer1V->Add(splitterWindow, 1, wxEXPAND | wxALL);

    //notebook panel and notebook
    wxPanel* notebookPanel = new wxPanel(splitterWindow); //need a panel so we can have a sizer so we can have a control border...
    wxBoxSizer* notebookPanelSizer = new wxBoxSizer(wxVERTICAL);
    notebookPanel->SetSizer(notebookPanelSizer);
    wxNotebook * notebook = new wxNotebook(notebookPanel, NOTEBOOK);
    notebookPanelSizer->Add(notebook, 1, wxEXPAND + wxALL, CONTROL_BORDER);

    //notebook record page
    wxPanel * recordPage = new wxPanel(notebook); //need this as can't add a sizer directly to a notebook
    recordPage->SetNextHandler(this); //allow focus events from the description text box to propagate
    notebook->AddPage(recordPage, wxT("Record"));
    wxBoxSizer * recordPageSizer = new wxBoxSizer(wxVERTICAL);
    recordPage->SetSizer(recordPageSizer);
    wxStaticBoxSizer * recProjectNameBox = new wxStaticBoxSizer(wxHORIZONTAL, recordPage, wxT("Project"));
    recordPageSizer->Add(recProjectNameBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mRecProjectNameCtrl = new wxStaticText(recordPage, REC_PROJECT_NAME, wxEmptyString); //name added later
    recProjectNameBox->Add(mRecProjectNameCtrl, 1, wxEXPAND);
    wxStaticBoxSizer * descriptionBox = new wxStaticBoxSizer(wxHORIZONTAL, recordPage, wxT("Description"));
    recordPageSizer->Add(descriptionBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
    mDescriptionCtrl = new MyTextCtrl(recordPage, TEXTCTRL_Description, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    descriptionBox->Add(mDescriptionCtrl, 1, wxEXPAND);
    descriptionBox->Add(new wxButton(recordPage, BUTTON_ClearDescription, wxT("Clear")), 0, wxLEFT, CONTROL_BORDER);
    mTree = new TickTreeCtrl(recordPage, TREE, wxDefaultPosition, wxSize(100, 100), wxT("Recorders")); //the y value of the size is the significant one.  Makes it a bit taller than default.  Do not use until SetSavedState() has been called.
    mRecorderGroup->SetTree(mTree);
    recordPageSizer->Add(mTree, 1, wxEXPAND | wxALL, CONTROL_BORDER);

    //display a sensible amount (i.e total initial size of record tab) in the upper part of the splitter window 
    splitterWindow->SetMinimumPaneSize(notebookPanel->GetBestSize().y);

    //notebook playback page
    if (!parser.Found(wxT("p"))) {
        wxPanel * playbackPage = new wxPanel(notebook); //need this as can't add a sizer directly to a notebook
        notebook->AddPage(playbackPage, wxT("Playback"));
        mPlaybackPageSizer = new wxBoxSizer(wxVERTICAL);
        playbackPage->SetSizer(mPlaybackPageSizer);
        mPlayProjectNameBox = new wxStaticBoxSizer(wxHORIZONTAL, playbackPage); //updated when project name static text within is updated
        mPlaybackPageSizer->Add(mPlayProjectNameBox, 0, wxEXPAND | wxALL, CONTROL_BORDER);
        mPlayProjectNameBox->Add(new wxStaticText(playbackPage, PLAY_PROJECT_NAME, wxEmptyString), 1, wxEXPAND);
        wxStaticBoxSizer * playbackTracksBox = new wxStaticBoxSizer(wxHORIZONTAL, playbackPage, wxT("Tracks"));
        mPlaybackPageSizer->Add(playbackTracksBox, 1, wxEXPAND | wxALL, CONTROL_BORDER);
        playbackTracksBox->Add((wxWindow*) mPlayer->GetTrackSelector(playbackPage), 1, wxEXPAND);
    }
    //event panel
    wxPanel * eventPanel = new wxPanel(splitterWindow);
    splitterWindow->SplitHorizontally(notebookPanel, eventPanel); //default 50% split
    wxBoxSizer * eventPanelSizer = new wxBoxSizer(wxVERTICAL);
    eventPanel->SetSizer(eventPanelSizer);

    //take navigation buttons
    wxBoxSizer * sizer2dH = new wxBoxSizer(wxHORIZONTAL);
    eventPanelSizer->Add(sizer2dH, 0, wxEXPAND);
    sizer2dH->Add(new wxButton(eventPanel, BUTTON_MENU_PrevTake, wxT("Previous Take")), 0, wxALL | wxFIXED_MINSIZE, CONTROL_BORDER); //init with longest string to make button big enough
    sizer2dH->Add(new wxButton(eventPanel, BUTTON_MENU_NextTake, wxT("End of Take")), 0, wxALL | wxFIXED_MINSIZE, CONTROL_BORDER); //init with longest string to make button big enough
    if (!parser.Found(wxT("p"))) sizer2dH->Add(new wxButton(eventPanel, BUTTON_JumpToTimecode, wxT("Jump to Timecode")), 0, wxALL, CONTROL_BORDER);
    if (!parser.Found(wxT("p"))) sizer2dH->Add(new wxButton(eventPanel, BUTTON_TakeSnapshot, wxT("Take Snapshot")), 0, wxALL, CONTROL_BORDER);
    sizer2dH->AddStretchSpacer();
    sizer2dH->Add(new wxButton(eventPanel, BUTTON_DeleteCue, wxT("Delete Cue Point")), 0, wxALL, CONTROL_BORDER);

    //event list
    mEventList = new EventList(eventPanel, -1, wxDefaultPosition, wxSize(splitterWindow->GetClientSize().x, EVENT_LIST_HEIGHT), !parser.Found(wxT("n")));
    mEventList->Load(); //this should be done in the constructor but causes a segfault
    eventPanelSizer->Add(mEventList, 1, wxEXPAND + wxALL, CONTROL_BORDER);

    //Global window settings
    Fit();
    splitterWindow->SetMinimumPaneSize(1); //to allow it to be almost but not quite closed.  Do this after Fit() or the earlier value will not be reflected in the window layout.
    SetMinSize(GetSize()); //prevents scrollbar of event list disappearing oddly
    wxToolTip::SetDelay(TOOLTIP_DELAY);

    //Timepos
    mTimepos = new Timepos(this, timecodeDisplay, positionDisplay);
    mTimepos->SetDefaultEditRate(mEventList->GetEditRate());

    //jog/shuttle
    mJogShuttle = new ingex::JogShuttle();
    mJSListener = new JSListener(this);
    mJogShuttle->addListener(mJSListener);
    mJogShuttle->start();

    //saved state - do this after all controls have been created because it may generate a dialogue which will lead to any events previously issued being processed, which may affect controls which hitherto didn't exist (here's looking at you, mRecorderGroup)
    mSavedState = new SavedState(this, SAVED_STATE_FILENAME);
    mTree->SetSavedState(mSavedState);
    mRecorderGroup->SetSavedState(mSavedState);
    if (mPlayer) mPlayer->SetSavedState(mSavedState);

    //Various persistent dialogues
    mHelpDlg = new HelpDlg(this); //persistent to allow it to be modeless in order to remain visible while using the app
    mCuePointsDlg = new CuePointsDlg(this, mSavedState); //persistent so that a file doesn't have to be read each time a cue point is marked
    mChunkingDlg = new ChunkingDlg(this, mTimepos, mSavedState); //persistent as it controls chunking while recording, as well as showing to set up the parameters
    mJumpToTimecodeDlg = new JumpToTimecodeDlg(this); //persistent so that multiple timecodes can be entered without bringing it up afresh each time

    mRecorderGroup->StartGettingRecorders(); //safe to let it generate events now that everything has been created (events will be processed if any dialogue is visible, such as the saved preferences warnings above)
    SetStatus(STOPPED);
    wxUpdateUIEvent::SetUpdateInterval(50); //update controls frequently but not as often as possible!
    mMenuShortcuts->UpdateUI(); //otherwise doesn't apply shortcuts on dynamically-generated menu labels until the menu is displayed
}

/// Responds to an application close event by closing, preventing closure or seeking confirmation, depending on the circumstances.
/// @param event The close event.
void IngexguiFrame::OnClose(wxCloseEvent & event)
{
    if (event.CanVeto() && RECORDING == mStatus) {
        wxMessageDialog dlg(this, wxT("You cannot close the application while recording: stop the recording first"), wxT("Cannot close while recording"), wxICON_EXCLAMATION | wxOK);
        dlg.ShowModal();
        event.Veto();
    }
    else {
        wxMessageDialog dlg(this, wxT("Are you sure?  You are not disconnected"), wxT("Confirmation of Quit"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
        if (!event.CanVeto() || 0 == mTree->GetRecorderCount() || wxID_YES == dlg.ShowModal()) {
            mRecorderGroup->Shutdown(); //will issue an event which will cause the app to exit
        }
        else {
            event.Veto();
        }
    }
}

/// Responds to an About menu request by showing the appropriate dialogue.
/// @param event The command event.
void IngexguiFrame::OnAbout( wxCommandEvent& WXUNUSED( event ) )
{
    AboutDlg dlg(this);
    dlg.ShowModal();
}

/// Responds to a Help menu request by displaying the help dialogue modelessly.
/// @param event The command event.
void IngexguiFrame::OnHelp( wxCommandEvent& WXUNUSED( event ) )
{
    //modeless help dialog
    mHelpDlg->Show();
}

/// Responds to a Project Names menu request by showing the appropriate dialogue and updating state if user makes changes.
/// @param event The command event.
void IngexguiFrame::OnSetProjectName( wxCommandEvent& WXUNUSED( event ) )
{
    SetProjectName();
}

/// Shows the set project name dialogue and updates state if user makes changes.
void IngexguiFrame::SetProjectName()
{
    SetProjectDlg dlg(this);
    mSetProjectDlg = &dlg; //so we know where to send project names to
    if (mRecorderGroup->RequestProjectNames()) { //an event will arrive with project names
        if (wxID_OK == dlg.ShowModal()) { //dialogue will display holding message and idle, allowing the event with the list of project names to propagate and be sent to it
            mRecorderGroup->AddProjectNames(dlg.GetNewProjectNames());
            mRecorderGroup->SetCurrentProjectName(dlg.GetSelectedProject());
        }
    }
    else { //can't get project names
        dlg.SetProjectNames(); //display warning
    }
    mSetProjectDlg = 0; //to prevent any events being sent to it
}

/// Responds to a Test mode request by showing the appropriate dialogue, which uses events to send commands back to the frame.
/// @param event The command event.
void IngexguiFrame::OnTestMode( wxCommandEvent& WXUNUSED( event ) )
{
    mTestModeDlg->ShowModal();
}

/// Responds to a Set Rolls menu request by showing the appropriate dialogue and informing the recorder group if user makes changes.
/// @param event The command event.
void IngexguiFrame::OnSetRolls( wxCommandEvent& WXUNUSED( event ) )
{
    SetRollsDlg dlg(this, mRecorderGroup->GetPreroll(), mRecorderGroup->GetMaxPreroll(), mRecorderGroup->GetPostroll(), mRecorderGroup->GetMaxPostroll(), mSavedState);
    if (wxID_OK == dlg.ShowModal()) {
        mRecorderGroup->SetPreroll(dlg.GetPreroll());
        mRecorderGroup->SetPostroll(dlg.GetPostroll());
    }
}

/// Responds to a Set Cue Descriptions menu request by showing the appropriate dialogue.
/// @param event The command event.
void IngexguiFrame::OnSetCues( wxCommandEvent& WXUNUSED( event ) )
{
    mCuePointsDlg->ShowModal();
}

/// Responds to a Chunking menu request by showing the appropriate dialogue.
/// @param event The command event.
void IngexguiFrame::OnChunking( wxCommandEvent& WXUNUSED( event ) )
{
    mChunkingDlg->ShowModal();
}

/// Responds to a Refresh Recorder List request by initiating the process of obtaining a list of recorders.
/// @param event The command event.
void IngexguiFrame::OnRecorderListRefresh( wxCommandEvent& WXUNUSED( event ) )
{
    mRecorderGroup->StartGettingRecorders();
}

/// Responds to the Use Tape IDs menu checkbox being clicked by updating the state
void IngexguiFrame::OnUseTapeIds( wxCommandEvent& event )
{
    SetTapeIdsDlg::EnableTapeIds(mSavedState, event.IsChecked());
    mTree->UpdateTapeIds(); //will issue an event to update the record and tape ID buttons
}

/// Responds to the tape IDs button being pressed by showing the appropriate dialogue and updating state if user makes changes.
/// @param event The button event.
void IngexguiFrame::OnSetTapeIds( wxCommandEvent& WXUNUSED( event ) )
{
    wxArrayString names;
    std::vector<bool> enabled;
    mTree->GetPackageNames(names, enabled);
    SetTapeIdsDlg dlg(this, mSavedState, names, enabled);
    if (dlg.IsUpdated()) {
        mTree->UpdateTapeIds(); //will issue an event to update the record and tape ID buttons
    }
}

/// Responds to a Clear Log menu request.
/// @param event The command event.
void IngexguiFrame::OnClearLog( wxCommandEvent& WXUNUSED(event) )
{
    ClearLog();
}

/// Responds to a menu quit event by attempting to close the app.
/// @param event The command event.
void IngexguiFrame::OnQuit( wxCommandEvent& WXUNUSED( event ) )
{
    Close();
}

/// Responds to various shortcut menu events.
/// @param event The command event.
void IngexguiFrame::OnShortcut( wxCommandEvent& event )
{
    if (OperationAllowed(event.GetId())) {
        switch (event.GetId()) {
            case MENU_PrevTrack :
                if (mPlayer) mPlayer->SelectEarlierTrack();
                break;
            case MENU_NextTrack :
                if (mPlayer) mPlayer->SelectLaterTrack();
                break;
            case MENU_Up :
                //up one event
            case MENU_Down :
                //down one event
                if (mPlayer && FILES == mPlayer->GetMode()) { //File Mode - just jump to start or end of file
                    mPlayer->JumpToFrame(MENU_Down == event.GetId() ? -1 : 0);
                }
                else {
                    mEventList->SelectAdjacentEvent(MENU_Down == event.GetId());
                }
                break;
            case BUTTON_MENU_PrevTake :
                mEventList->SelectPrevTake(mPlayer ? mPlayer->WithinRecording() : false);
                break;
            case BUTTON_MENU_NextTake :
                mEventList->SelectNextTake();
                break;
            case MENU_FirstTake :
                mEventList->Select(0);
                break;
            case MENU_LastTake :
                mEventList->SelectLastTake();
                break;
            case MENU_PlayBackwards :
                if (mPlayer) mPlayer->Play(true, true);
                break;
            case MENU_Pause :
                if (mPlayer) mPlayer->Pause();
                break;
            case MENU_PlayForwards :
                if (mPlayer) mPlayer->Play(true, false);
                break;
            case MENU_PlayPause :
                if (PAUSED == mStatus) {
                    if (mPlayer) mPlayer->Play();
                }
                else {
                    if (mPlayer) mPlayer->Pause();
                }
                break;
            case MENU_StepBackwards :
                if (mPlayer) mPlayer->Step(false);
                break;
            case MENU_StepForwards :
                if (mPlayer) mPlayer->Step(true);
                break;
            case MENU_Mute :
                if (mPlayer) mPlayer->MuteAudio(!mPlayer->IsMuted());
                break;
        }
    }
}

/// Responds to record button being pressed.
/// @param event The button event.
void IngexguiFrame::OnRecord( wxCommandEvent& WXUNUSED( event ) )
{
    if (OperationAllowed(BUTTON_MENU_Record)) {
        if (GetMenuBar()->FindItem(MENU_AutoClear)->IsChecked()) {
            //clear everything away because we're starting again
            ClearLog(); //resets player
        }
        //issue command to record immediately, and log
        ProdAuto::MxfTimecode now;
        Log(wxT("Record button pressed @ ") + mTimepos->GetTimecode(&now));
        mRecorderGroup->Record(now);
        //now in the running up state
        SetStatus(RUNNING_UP);
    }
}

/// Responds to the an event from the player.
/// Doesn't check for player presence - do not call if player doesn't exist.
/// @param event The command event.
/// The following event IDs are recognised, with corresponding data values (all enumerations being in the prodauto namespace):
/// NEW_MODE: Reflect status in controls.
///     Event int: the mode.
/// STATE_CHANGE: Reflect the player's status.
///     Event int: PLAY, PLAY_BACKWARDS, PAUSE, STOP, CLOSE.
/// CUE_POINT: Select indicated point in the event list and pause player if at end/start of file depending on playback direction
///     Event int: Event list index for cue point.
/// FRAME_DISPLAYED: Update time counter.
///     Event extra long: Frame number.
///     Event client data: Valid position if non-zero, whereupon it points to an edit rate which must be deleted.
/// AT_START/WITHIN/AT_END: Update controls as some states etc will change.
/// CLOSE_REQ: Switch the player to external output only, or disable completely if external output is not being used.
/// KEYPRESS: Respond to the user pressing a shortcut key in the player window when in "divert Keypresses" mode.
///     Event int: X-windows key value.
///     Event client data: edit rate - must delete this.
void IngexguiFrame::OnPlayerEvent(wxCommandEvent& event) {
    switch ((PlayerEventType) event.GetId()) {
        case NEW_MODE:
            break;
        case STATE_CHANGE:
            switch ((PlayerState::PlayerState) event.GetInt()) {
                case PlayerState::PLAYING:
                    if (!IsRecording()) SetStatus(PLAYING); //player is in control of state (otherwise going E to E on record would change the mode)
                    break;
                case PlayerState::PLAYING_BACKWARDS:
                    SetStatus(PLAYING_BACKWARDS);
                    break;
                case PlayerState::PAUSED:
                    if (!IsRecording()) SetStatus(PAUSED); //player is in control of state (This check prevents a sitaution where when toggling rapidly between play and record we get a paused status but recorders are still recording. Record button is not enabled, because recorder selector is not enabled, because RecorderGroup::Stop() hasn't been called.  Position is still running and tree/status are indicating problems.  The only way out of this is to quit - stopping the recorders with another GUI leaves the recorder selector and record buttons disabled.)
                    break;
                case PlayerState::STOPPED:
                    if (!IsRecording()) SetStatus(STOPPED); //player is in control of state
                    break;
                case PlayerState::CLOSED:
                    //Issued on Enable(false) _and_ Reset()
                    break;
            }
            break;
        case CUE_POINT:
            //move to the right position in the list
            mEventList->Select(event.GetInt(), true); //As it is the player selecting this event, stop the player being told to jump to it
            break;
        case FRAME_DISPLAYED:
            if (event.GetClientData()) {
                //valid position
                mTimepos->SetPosition(event.GetExtraLong() + mEventList->GetCurrentChunkStartPosition(), (ProdAuto::MxfTimecode *) event.GetClientData());
                delete (ProdAuto::MxfTimecode *) event.GetClientData();
            }
            else {
                mTimepos->SetPositionUnknown(true); //display "no position"
            }
            break;
        case AT_START:
        case WITHIN:
        case AT_END:
            break;
        case LOAD_PREV_CHUNK: //player has not paused
            mEventList->SelectAdjacentEvent(false);
            break;
        case LOAD_NEXT_CHUNK: //player has not paused
            mEventList->SelectAdjacentEvent(true);
            break;
        case LOAD_FIRST_CHUNK: //player has not paused
            mEventList->SelectPrevTake(true);
            break;
        case CLOSE_REQ:
            if (prodauto::DUAL_DVS_AUTO_OUTPUT == mPlayer->GetOutputType() || prodauto::DUAL_DVS_X11_OUTPUT == mPlayer->GetOutputType()) { //external output is active
                //don't close the player, just switch to external only
                mPlayer->ChangeOutputType(prodauto::DVS_OUTPUT);
            }
            else {
                //close the player completely
                mPlayer->Enable(false);
            }
            break;
        case KEYPRESS: {
            int id = wxID_ANY;
            switch (event.GetInt()) {
                case 48: //0...
                case 49:
                case 50:
                case 51:
                case 52:
                case 53:
                case 54:
                case 55:
                case 56:
                case 57: //...9
                    if (mCuePointsDlg->IsModal()) mCuePointsDlg->Shortcut(event.GetInt() - 48);//digit to select a cue type
                    break;
                case 65307: //Esc
                    if (mCuePointsDlg->IsModal()) mCuePointsDlg->Shortcut(-1);//abandon selecting cue type
                    break;
                case 65470: //F1
                    id = BUTTON_MENU_Record;
                    break;
                case 65471: //F2
                    id = MENU_MarkCue;
                    break;
                case 65474: //F5 (NB shift is detected below)
                    id = BUTTON_MENU_Stop;
                    break;
                case 65362: //Up
                    //Prev Event
                    id = MENU_Up;
                    break;
                case 65364: //Down
                    //Next Event
                    id = MENU_Down;
                    break;
                case 65365: //PgUp
                    id = BUTTON_MENU_PrevTake;
                    break;
                case 65366: //PgDn
                    id = BUTTON_MENU_NextTake;
                    break;
                case 65360: //Home
                    //First Take
                    id = MENU_FirstTake;
                    break;
                case 65367: //End
                    //Last Take
                    id = MENU_LastTake;
                    break;
                case 116: //t
                    id = MENU_JumpToTimecode;
                    break;
                default:
                    break;
            }
            if (
             wxID_ANY != id //recognised the key
             && (
              (BUTTON_MENU_Stop == id && event.GetExtraLong() == 1) //stop has been pressed with shift,
              || (BUTTON_MENU_Stop != id && event.GetExtraLong() == 0) //any other keypress doesn't have a modifier
             )) {
                mMenuShortcuts->UpdateUI(); //otherwise doesn't update enable states, and hence act upon any changes to system state, until the menu is displayed
                wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED, id);
                AddPendingEvent(menuEvent);
            }
            break;
        }
        default:
            break;
    }
}

/// Responds to the an event from the jog/shuttle control.
/// All actions are propagated via shortcut menu events, relying on the enable states being correctly controlled downstream to prevent inappropriate actions
/// @param event The command event.
void IngexguiFrame::OnJogShuttleEvent(wxCommandEvent& event)
{
    wxCommandEvent menuEvent(wxEVT_COMMAND_MENU_SELECTED);
    menuEvent.SetId(wxID_ANY); //indicates no hit so far
    switch ((JogShuttleEventType) event.GetId()) {
        case JOG :
            if (mCuePointsDlg->IsModal()) {
                //Scroll up and down the list of cue points
                mCuePointsDlg->Scroll(event.GetInt());
            }
            else {
                menuEvent.SetId(MENU_Pause);
                AddPendingEvent(menuEvent);
                menuEvent.SetId(event.GetInt() ? MENU_StepForwards : MENU_StepBackwards);
            }
            break;
        case SHUTTLE :
            if (mPlayer) mPlayer->PlayAbsolute(event.GetInt());
            break;
        case JS_BUTTON_PRESSED :
            switch (event.GetInt()) {
                case 1: //1st row, left hand side
                    menuEvent.SetId(BUTTON_MENU_Record);
                    break;
                case 2: //1st row, second from left
                    menuEvent.SetId(MENU_MarkCue);
                    break;
                case 3: case 4: //1st row, right hand two
                    if (mJSListener->IsPressed(14) || mJSListener->IsPressed(15) //either black button on Shuttle Pro 2
                     || (mJSListener->IsPressed(4) && mJSListener->IsPressed(3))) { //or both stop buttons must be pressed
                        if (OperationAllowed(BUTTON_MENU_Stop)) {
                            //cancel any cue point entering operation
                            mCuePointsDlg->EndModal(wxID_CANCEL);
                        }
                        menuEvent.SetId(BUTTON_MENU_Stop);
                    }
                    break;
                case 5: //2nd row, left hand side
                    menuEvent.SetId(MENU_PlayRecordings);
                    break;
                case 6: //2nd row, second from left
                    menuEvent.SetId(MENU_PlayBackwards);
                    break;
                case 7: //2nd row, middle
#ifndef DISABLE_SHARED_MEM_SOURCE
                    if (mPlayer && ETOE == mPlayer->GetMode()) {
                        //Switch out of EE mode and into whatever mode was selected before
                        mPlayer->SetPreviousMode();
                    }
                    else if (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus) {
#endif
                        menuEvent.SetId(MENU_Pause);
#ifndef DISABLE_SHARED_MEM_SOURCE
                    }
                    else {
                        menuEvent.SetId(MENU_EtoE);
                    }
#endif
                    break;
                case 8: //2nd row, second from right
                    menuEvent.SetId(MENU_PlayForwards);
                    break;
                case 9: //2nd row, right hand side
                    menuEvent.SetId(MENU_PlayFiles);
                    break;
                case 10: //top left long silver button
                    menuEvent.SetId(MENU_Up);
                    break;
                case 12: //bottom left long silver button
                    menuEvent.SetId(MENU_Down);
                    break;
                case 11: //top right long silver button
                    menuEvent.SetId(BUTTON_MENU_PrevTake);
                    break;
                case 13: //bottom right long silver button
                    menuEvent.SetId(BUTTON_MENU_NextTake);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (wxID_ANY != menuEvent.GetId()) { //recognised the keypress
        AddPendingEvent(menuEvent);
    }
}

/// Responds to the stop button being pressed.
/// @param event The command event.
void IngexguiFrame::OnStop( wxCommandEvent& WXUNUSED( event ) )
{
    if (OperationAllowed(BUTTON_MENU_Stop)) {
        if (RECORDING == mStatus || RUNNING_UP == mStatus) {
            ProdAuto::MxfTimecode now;
            mTimepos->GetTimecode(&now);
            Log(wxT("Stop button pressed during recording or run-up @ ") + Timepos::FormatTimecode(now));
            mRecorderGroup->Stop(false, now, mDescriptionCtrl->GetLineText(0).Trim(false).Trim(true), mEventList->GetLocators());
            SetStatus(RUNNING_DOWN);
        }
        else if (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || PAUSED == mStatus) {
            SetStatus(STOPPED);
        }
    }
}

/// Responds to the Play/Pause/Cue button being pressed.
/// Adds a cue point, or tells the player.
/// @param event The command event.
void IngexguiFrame::OnCue( wxCommandEvent& WXUNUSED( event ) )
{
    switch (mStatus) {
        case RECORDING :
            {
                ProdAuto::MxfTimecode timecode;
                mTimepos->GetStartTimecode(&timecode);
                const int64_t frameCount = mTimepos->GetFrameCount();
                timecode.samples += frameCount; //will wrap automatically on display
                if (mPlayer) mPlayer->DivertKeyPresses(); //so that key presses in the player window are sent to the dialogue
                if (wxID_OK == mCuePointsDlg->ShowModal(Timepos::FormatTimecode(timecode))) {
                    if (mCuePointsDlg->ValidCuePointSelected()) mEventList->AddEvent(EventList::CUE, 0, mCuePointsDlg->GetDescription(), frameCount, mCuePointsDlg->GetColourIndex());
                }
                if (mPlayer) mPlayer->DivertKeyPresses(false);
                break;
            }
        case PLAYING : case PLAYING_BACKWARDS :
            if (mPlayer) mPlayer->Pause();
            break;
        case PAUSED :
            if (mPlayer) mPlayer->Play(true, false); //always play forwards (more logical than following the direction last set by the keyboard)
            break;
        default :
            break;
    }
//  SetFocus();
}

/// Deletes the cue point corresponding to the currently selected event list item.
void IngexguiFrame::OnDeleteCue( wxCommandEvent& WXUNUSED( event ) )
{
    mEventList->DeleteCuePoint();
}

/// Responds to the chunk button being pressed.
/// Starts a new chunk and resets the chunk countdown timer.
void IngexguiFrame::OnChunkButton( wxCommandEvent& WXUNUSED(event) )
{
    mChunkingDlg->RunFrom(); //just in case it's about to chunk anyway
    mChunkingDlg->Realign(); //to realign the start of the chunk after the one we're about to instigate, if alignment is enabled
    ProdAuto::MxfTimecode now;
    Log(wxT("Chunk button pressed @ ") + mTimepos->GetTimecode(&now));
    mRecorderGroup->Stop(true, now, mDescriptionCtrl->GetLineText(0).Trim(false).Trim(true), mEventList->GetLocators());
}

/// Responds to trigger event (set by chunking dialogue to the chunk interval) by initiating a chunking operation.
/// @param event Contains a pointer to the timecode of the chunk end, minus postroll.  Must be deleted.
void IngexguiFrame::OnTimeposEvent(wxCommandEvent& event)
{
    Log(wxT("Automatic chunk @ ") + Timepos::FormatTimecode(*((ProdAuto::MxfTimecode *) event.GetClientData())));
    mRecorderGroup->Stop(true, *((ProdAuto::MxfTimecode *) event.GetClientData()), mDescriptionCtrl->GetLineText(0).Trim(false).Trim(true), mEventList->GetLocators());
    delete (ProdAuto::MxfTimecode *) event.GetClientData();
}

/// Responds to an event (cue point, start, chunk start or stop point) being selected by the user or by new recordings arriving
/// Informs the player and supplies paths to the test mode dialogue so it knows where to erase files from.
/// @param event The command event; ExtraLong is set to indicate a forced reload.  Do not use GetItem() to determine the selection as this may not be valid.
void IngexguiFrame::OnEventSelection(wxListEvent& event)
{
    if (mPlayer) mTestModeDlg->SetRecordPaths(mPlayer->SelectRecording(mEventList->GetCurrentChunkInfo(), mEventList->GetCurrentCuePoint(), event.GetExtraLong()));
}

/// Responds to an event (cue point) being double-clicked.
/// Informs the player, and starts playback if paused.
/// @param event The command event.
void IngexguiFrame::OnEventActivated(wxListEvent& WXUNUSED(event))
{
    if (mPlayer) {
        mPlayer->SelectRecording(mEventList->GetCurrentChunkInfo(), mEventList->GetCurrentCuePoint());;
        if (PAUSED == mStatus) mPlayer->Play();
    }
}

/// Called when something gets focus.
/// If it's the description control, disables conflicting shortcuts so that they go to the control.
/// @param event The focus event.
void IngexguiFrame::OnFocusGot(wxFocusEvent & event)
{
    if (TEXTCTRL_Description == event.GetId()) {
        TextFieldHasFocus(true);
    }
    else {
        event.Skip();
    }
}

/// Called when something loses focus.
/// If it's the description control, enables conflicting shortcuts so that they can be used.
/// @param event The focus event.
void IngexguiFrame::OnFocusLost(wxFocusEvent & event)
{
    if (TEXTCTRL_Description == event.GetId()) {
        TextFieldHasFocus(false);
    }
    else {
        event.Skip();
    }
}

/// Disables conflicting shortcuts when editing an event description.
void IngexguiFrame::OnEventBeginEdit(wxListEvent& WXUNUSED(event))
{
    TextFieldHasFocus(true);
}

/// Re-enables conflicting shortcuts after editing an event description.
void IngexguiFrame::OnEventEndEdit(wxListEvent& WXUNUSED(event))
{
    TextFieldHasFocus(false);
}

/// Enables or disables the alphanumeric shortcuts to allow the characters they use to be entered into text boxes.
/// @param hasFocus True to disable the shortcuts.
void IngexguiFrame::TextFieldHasFocus(const bool hasFocus)
{
    mTextFieldHasFocus = hasFocus;
    mMenuShortcuts->UpdateUI(); //otherwise doesn't update enable states and hence act upon the change until the menu is displayed
}

/// Responds to the an event from the group of recorders.
/// @param event The command event.
/// The following event IDs are recognised, the enumerations being in the RecorderGroupCtrl namespace:
/// NEW_RECORDER: Makes sure project name is set or disconnects.  Sets mode to connected.  Sets status to recording if the new recorder is recording.  Logs.
///     Event string: The recorder name.
///     Event extra long: The recorder is recording.
/// RECORD: Starts position counter and adds a start event to the event list.
///     Event client data: Ptr to a ProdAuto::MxfTimecode object with the start timecode.  Deletes this.
/// CHUNK_START: Tells the chunking dialogue to start counting down to the next chunking point.
///     Event client data: Ptr to a ProdAuto::MxfTimecode object with the start timecode.  Deletes this.
/// RECORDER_STARTED: Log.
///     Event string: The recorder name.
///     Event int: Non-zero for success.
///     Event client data: Ptr to a ProdAuto::MxfTimecode object with the start timecode.  Deletes this.
/// STOP: Starts position counter, and adds a stop event to the event list, and sets status to STOPPED.
///     Event client data: Ptr to a ProdAuto::MxfTimecode object with the stop timecode.  Deletes this.
/// CHUNK_END: Adds a chunk event to the event list.
///     Event client data: Ptr to a ProdAuto::MxfTimecode object with the chunking timecode.  Deletes this.
/// SET_TRIGGER: Sets a trigger for the next chunk start.
///     Event client data: Ptr to a ProdAuto::MxfTimecode object with the chunking timecode.  Deletes this.
/// RECORDER_STOPPED: Log.  If successful and not a router recorder, add recording details to the event list.
///     Event string: The recorder name.
///     Event int: Non-zero for success.
///	Event extra-long: non-zero if a router recorder.
///     Event client data: if not a router recorder, ptr to a RecorderData object, containing track/file data.  Deletes this.
/// REMOVE_RECORDER: Logs. Sets mode to disconnected if no recorders remaining.
///     Event string: The recorder name.
/// DISPLAY_TIMECODE: Displays the given timecode, enabling auto-increment.
///     Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
/// DISPLAY_TIMECODE_STUCK: Displays the given timecode, disabling auto-increment.
///     Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
/// DISPLAY_TIMECODE_MISSING: Displays timecode as being missing.
/// COMM_FAILURE: Informs the source tree.
///     Event string: The recorder name.
/// TIMECODE_STUCK: Informs the source tree.
///     Event string: The recorder name.
///     Event client data: Ptr to a RecorderData object, containing a timecode.  Deletes this.
/// TIMECODE_MISSING: Informs the source tree.
///     Event string: The recorder name.
/// TIMECODE_SOURCE: Informs the source tree.
///     Event string: The recorder name.
/// SET_TRIGGER: Adds a CHUNK event and sets a trigger to start the next chunk when the time is reached.
///     Event client data: pointer to timecode of start of next chunk.  Deletes this.
/// PROJECT_NAMES: Sends to project names dialogue, if present.
///     Event client data: Ptr to a RecorderData object, containing list of strings.  Deletes this.
/// DIE: Exits the application, as this is a result of the RecorderGroup being destroyed when the application needs to exit.

void IngexguiFrame::OnRecorderGroupEvent(wxCommandEvent& event) {
    switch ((RecorderGroupCtrl::RecorderGroupCtrlEventType) event.GetId()) {
        case RecorderGroupCtrl::NEW_RECORDER :
            Log(wxT("NEW_RECORDER: \"") + event.GetString() + wxT("\""));
            //event contains all the details about the recorder
            if (mRecorderGroup->GetCurrentProjectName().IsEmpty()) { //not yet selected, or just removed, the project name, and we have the project names from the database now as we've got a recorder
                SetProjectName(); //possibly add names, and choose one
                if (mRecorderGroup->GetCurrentProjectName().IsEmpty()) {
                    Log(wxT("Disconnecting because no project name set"));
                    mRecorderGroup->DisconnectAll();
                }
            }
            if (!mRecorderGroup->GetCurrentProjectName().IsEmpty() && event.GetExtraLong()) {
                //this recorder is still connected and is already recording
                Log(wxT(" ...already recording"));
                SetStatus(RECORDING); //will prevent any more recorders being added
                ProdAuto::MxfTimecode startTimecode;
                mTimepos->GetStartTimecode(&startTimecode);
                mEventList->AddEvent(EventList::START, &startTimecode, mRecorderGroup->GetCurrentProjectName()); //didn't start now - started before the preroll period
                mTimepos->SetPositionUnknown(); //don't know when recording started
            }
            break;
        case RecorderGroupCtrl::RECORD : {
                ProdAuto::MxfTimecode* tc = (ProdAuto::MxfTimecode *) event.GetClientData();
                //start the position display counting
                mTimepos->Record(*tc);
                //add a start event
                mEventList->AddEvent(EventList::START, tc, mRecorderGroup->GetCurrentProjectName());
                delete tc;
                break;
            }
        case RecorderGroupCtrl::CHUNK_START : {
                ProdAuto::MxfTimecode* tc = (ProdAuto::MxfTimecode *) event.GetClientData();
                //start chunking timer (if enabled), on chunk length from now (no alignment)
                mChunkingDlg->RunFrom(*tc, mRecorderGroup->GetChunkingPostroll(), false);
                delete tc;
                break;
            }
        case RecorderGroupCtrl::RECORDER_STARTED : {
                ProdAuto::MxfTimecode* tc = (ProdAuto::MxfTimecode *) event.GetClientData();
                if (event.GetInt()) { //successful
                    Log(wxT("RECORDING successfully on \"") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(*tc));
                }
                else {
                    Log(wxT("RECORDING failed on \"") + event.GetString() + wxT("\""));
                }
                delete tc;
                break;
            }
        case RecorderGroupCtrl::STOP : {
                ProdAuto::MxfTimecode* tc = (ProdAuto::MxfTimecode *) event.GetClientData();
                mTimepos->Stop(*tc);
                mEventList->AddEvent(EventList::STOP, tc, mRecorderGroup->GetCurrentDescription(), mTimepos->GetFrameCount());
                delete tc;
                break;
            }
        case RecorderGroupCtrl::CHUNK_END : {
                ProdAuto::MxfTimecode* tc = (ProdAuto::MxfTimecode *) event.GetClientData();
                mEventList->AddEvent(EventList::CHUNK, tc, mRecorderGroup->GetCurrentDescription(), mTimepos->GetFrameCount());
                delete tc;
                break;
            }
        case RecorderGroupCtrl::SET_TRIGGER : {
                ProdAuto::MxfTimecode* tc = (ProdAuto::MxfTimecode *) event.GetClientData();
                mTimepos->SetTrigger((ProdAuto::MxfTimecode *) event.GetClientData(), mRecorderGroup, false); //allow trigger to be in the past (whereupon it will happen immediately)
                delete tc;
                break;
            }
        case RecorderGroupCtrl::RECORDER_STOPPED :
            if (event.GetInt()) { //successful
                Log(wxT("STOPPED successfully on \"") + event.GetString() + wxT("\"") + (event.GetExtraLong() ? wxT(" (Router recorder)") : wxEmptyString));
                if (!event.GetExtraLong()) { //not a router recorder
                    if (((RecorderData *) event.GetClientData())->GetTrackList().operator->()) {
                        //Add the recorded files to the take info (assumes a STOPPED or CHUNK_END event has already been received, so mEventList knows if it's a chunk or not and therefore which ChunkInfo to add it to)
                        mEventList->AddRecorderData((RecorderData *) event.GetClientData());
                        //need to reload the player as more files have appeared
                        if (mPlayer) mPlayer->SelectRecording(mEventList->GetCurrentChunkInfo(), 0, true);
                    }
                    //logging
                    CORBA::StringSeq_var fileList = ((RecorderData *) event.GetClientData())->GetStringSeq();
                    wxString msg = wxString::Format(wxT("%d element%s returned%s"), fileList->length(), 1 == fileList->length() ? wxEmptyString : wxT("s"), fileList->length() ? wxT(":") : wxT("."));
                    for (size_t i = 0; i < fileList->length(); i++) {
                        msg += wxT("\n\"") + wxString(fileList[i], *wxConvCurrent) + wxT("\"");
                    }
                    Log(msg);
                }
            }
            else {
                Log(wxT("STOPPED failure on \"") + event.GetString() + wxT("\""));
            }
            if (!event.GetExtraLong()) delete (RecorderData *) event.GetClientData();
            break;
        case RecorderGroupCtrl::REMOVE_RECORDER :
            Log(wxT("REMOVE_RECORDER for \"") + event.GetString() + wxT("\""));
            if (0 == mTree->GetRecorderCount() && IsRecording()) { //this check needed in case we disconnect from a recorder that's recording/running up and we can't contact
                SetStatus(STOPPED);
            }
            break;
        case RecorderGroupCtrl::DISPLAY_TIMECODE :
//spam          Log(wxT("DISPLAY_TIMECODE ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
            mTimepos->SetTimecode(((RecorderData *) event.GetClientData())->GetTimecode(), false);
            delete (RecorderData *) event.GetClientData();
            break;
        case RecorderGroupCtrl::DISPLAY_TIMECODE_STUCK :
            Log(wxT("DISPLAY_TIMECODE_STUCK ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
            mTimepos->SetTimecode(((RecorderData *) event.GetClientData())->GetTimecode(), true);
            delete (RecorderData *) event.GetClientData();
            break;
        case RecorderGroupCtrl::DISPLAY_TIMECODE_MISSING :
            Log(wxT("DISPLAY_TIMECODE_MISSING"));
            mTimepos->DisableTimecode(UNKNOWN_TIMECODE);
            break;
        case RecorderGroupCtrl::DISPLAY_TIMECODE_SOURCE :
            Log(wxT("DISPLAY_TIMECODE_SOURCE \"") + event.GetString() + wxT("\""));
            break;
        case RecorderGroupCtrl::COMM_FAILURE :
            Log(wxT("COMM_FAILURE for \"") + event.GetString() + wxT("\""));
            mTree->SetRecorderStateUnknown(event.GetString(), wxT("Uncontactable: retrying"));
            break;
        case RecorderGroupCtrl::TIMECODE_STUCK :
            Log(wxT("TIMECODE_STUCK for \"") + event.GetString() + wxT("\" @ ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
            mTree->SetRecorderStateProblem(event.GetString(), wxT("Timecode stuck at ") + Timepos::FormatTimecode(((RecorderData *) event.GetClientData())->GetTimecode()));
            delete (RecorderData *) event.GetClientData();
            break;
        case RecorderGroupCtrl::TIMECODE_MISSING :
            Log(wxT("TIMECODE_MISSING for \"") + event.GetString() + wxT("\""));
            mTree->SetRecorderStateProblem(event.GetString(), wxT("Timecode missing"));
            break;
        case RecorderGroupCtrl::TIMECODE_RUNNING :
            Log(wxT("TIMECODE_RUNNING for \"") + event.GetString() + wxT("\""));
            mTree->SetRecorderStateOK(event.GetString());
            break;
        case RecorderGroupCtrl::PROJECT_NAMES :
            if (mSetProjectDlg) mSetProjectDlg->SetProjectNames(((RecorderData *) event.GetClientData())->GetStringSeq(), mRecorderGroup->GetCurrentProjectName());
            delete (RecorderData *) event.GetClientData();
            break;
        case RecorderGroupCtrl::DIE :
//          if (mPlayer) delete mPlayer; //traffic control notification will be sent
            Log(wxT("Application closed.")); //this can segfault if done after Destroy()
            Destroy();
            break;
        case RecorderGroupCtrl::ENABLE_REFRESH : //never appears
            break;
    }
}

/// Responds to an event from the source tree.
/// Updates various controls (if the event string is empty)
/// Requests and sets tape IDs for the recorder indicated by the event's string (if the event string is not empty)
/// @param event The command event.
void IngexguiFrame::OnTreeEvent(wxCommandEvent& event)
{
    if (event.GetString().IsEmpty()) { //status
        switch (mStatus) {
            case RUNNING_UP:
                if (mTree->AllRecording()) { //all recorders that are supposed to are recording
                    SetStatus(RECORDING);
                    //start chunking timer (if enabled)
                    ProdAuto::MxfTimecode startTimecode;
                    mTimepos->GetStartTimecode(&startTimecode);
                    mChunkingDlg->RunFrom(startTimecode, mRecorderGroup->GetChunkingPostroll());
                }
                break;
            case RUNNING_DOWN:
                if (mTree->AllStopped()) { //all recorders that are supposed to are recording
                    SetStatus(STOPPED);
                }
            default:
                break;
        }
        if (0 == mTree->GetRecorderCount()) mTimepos->DisableTimecode();
    }
    else { //recorder name supplied - tape ID updates
        CORBA::StringSeq packageNames, tapeIds;
        mTree->GetRecorderTapeIds(event.GetString(), packageNames, tapeIds);
        mRecorderGroup->SetTapeIds(event.GetString(), packageNames, tapeIds);
    }
}

/// Sets the gui status.
/// @param stat The new status:
///     STOPPED: Not recording and not playing.
/// RECORDING
/// PLAYING
/// PLAYING_BACKWARDS
/// RUNNING_UP: Waiting for recorders to respond to record commands.
/// RUNNING_DOWN: Waiting for recorders to respond to stop commands.
/// PAUSED
/// UNKNOWN: Waiting for a response from a recorder.
void IngexguiFrame::SetStatus(Stat status)
{
    bool changed = false;
    if (mStatus != status) {
        mStatus = status;
        changed = true;
    }
    switch (mStatus) {
        case STOPPED:
            if (changed) {
                Log(wxT("status: STOPPED"));
            }
            SetStatusText(wxT("Stopped."));
            mTree->EnableChanges();
            mChunkingDlg->RunFrom();
            if (mPlayer) mPlayer->Record(false);
            break;
        case RUNNING_UP:
            if (changed) {
                Log(wxT("status: RUNNING_UP"));
            }
            SetStatusText(wxT("Running up."));
            if (mPlayer) mPlayer->Record();
            mTree->EnableChanges(false);
            break;
        case RECORDING:
            if (changed) {
                Log(wxT("status: RECORDING"));
                mRecorderGroup->EnableForInput(false); //catches connecting to a recorder that's already recording
            }
            SetStatusText(wxT("Recording."));
            if (mPlayer) mPlayer->Record();
            mTree->EnableChanges(false);
            break;
        case RUNNING_DOWN:
            if (changed) {
                Log(wxT("status: RUNNING_DOWN"));
            }
            SetStatusText(wxT("Running down."));
            if (mPlayer) mPlayer->Record();
            mTree->EnableChanges(false);
            mChunkingDlg->RunFrom();
            break;
        case PLAYING:
            if (changed) {
                Log(wxT("status: PLAYING"));
            }
            SetStatusText(wxT("Playing."));
            mTree->EnableChanges();
            break;
        case PLAYING_BACKWARDS:
            if (changed) {
                Log(wxT("status: PLAYING_BACKWARDS"));
            }
            SetStatusText(wxT("Playing backwards."));
            mTree->EnableChanges();
            break;
        case PAUSED:
            if (changed) {
                Log(wxT("status: PAUSED"));
            }
            SetStatusText(wxT("Paused."));
            mTree->EnableChanges();
            break;
    }

}

/// Responds to a player-related menu selection
void IngexguiFrame::OnPlayerCommand(wxCommandEvent & event)
{
    if (mPlayer) {
        switch (event.GetId()) {
            case MENU_PlayRecordings:
                mPlayer->SetMode(RECORDINGS);
                break;
#ifndef DISABLE_SHARED_MEM_SOURCE
            case MENU_EtoE:
                mPlayer->SetMode(ETOE);
                break;
#endif
            case MENU_PlayFiles:
                mPlayer->SetMode(FILES);
                break;
            case MENU_PlayerDisable:
                mPlayer->Enable(!event.IsChecked());
                break;
            case MENU_OpenMOV:
                mPlayer->Open(OPEN_MOV);
                break;
            case MENU_OpenMXF:
                mPlayer->Open(OPEN_MXF);
                break;
            case MENU_OpenServer:
                mPlayer->Open(OPEN_FROM_SERVER);
                break;
            case MENU_PlayerEnableSDIOSD:
                mPlayer->EnableSDIOSD(event.IsChecked());
                break;
            case MENU_PlayerExtAccelOutput:
                mPlayer->ChangeOutputType(prodauto::DUAL_DVS_AUTO_OUTPUT);
                break;
            case MENU_PlayerAccelOutput:
                mPlayer->ChangeOutputType(prodauto::X11_AUTO_OUTPUT);
                break;
            case MENU_PlayerExtOutput:
                mPlayer->ChangeOutputType(prodauto::DVS_OUTPUT);
                break;
            case MENU_PlayerExtUnaccelOutput:
                mPlayer->ChangeOutputType(prodauto::DUAL_DVS_X11_OUTPUT);
                break;
            case MENU_PlayerUnaccelOutput:
                mPlayer->ChangeOutputType(prodauto::X11_OUTPUT);
                break;
            case MENU_PlayerAbsoluteTimecode:
                mPlayer->ChangeOSDType(SOURCE_TIMECODE);
                break;
            case MENU_PlayerRelativeTimecode:
                mPlayer->ChangeOSDType(CONTROL_TIMECODE);
                break;
            case MENU_PlayerNoOSD:
                mPlayer->ChangeOSDType(OSD_OFF);
                break;
            case MENU_PlayerOSDTop:
                mPlayer->ChangeOSDPosition(OSD_TOP);
                break;
            case MENU_PlayerOSDMiddle:
                mPlayer->ChangeOSDPosition(OSD_MIDDLE);
                break;
            case MENU_PlayerOSDBottom:
                mPlayer->ChangeOSDPosition(OSD_BOTTOM);
                break;
            case MENU_PlayerNoLevelMeters:
                mPlayer->ChangeNumLevelMeters(0);
                break;
            case MENU_PlayerTwoLevelMeters:
                mPlayer->ChangeNumLevelMeters(2);
                break;
            case MENU_PlayerFourLevelMeters:
                mPlayer->ChangeNumLevelMeters(4);
                break;
            case MENU_PlayerEightLevelMeters:
                mPlayer->ChangeNumLevelMeters(8);
                break;
            case MENU_PlayerSixteenLevelMeters:
                mPlayer->ChangeNumLevelMeters(16);
                break;
            case MENU_PlayerAudioFollowsVideo:
                mPlayer->EnableAudioFollowsVideo(event.IsChecked());
                break;
            case MENU_PlayerLimitSplitToQuad:
                mPlayer->LimitSplitToQuad(event.IsChecked());
                break;
            case MENU_PlayerDisableScalingFiltering:
                mPlayer->DisableScalingFiltering(event.IsChecked());
                break;
        }
    }
}

/// Clears the list of events, the track buttons and anything being displayed by the player
void IngexguiFrame::ClearLog()
{
    mEventList->Clear();
    if (mPlayer) mPlayer->SelectRecording(0); //clears any recording shown and prevents non-existent recording being accessed
}

/// Responds to Clear Description button being pressed.
/// Clears the description text box and puts the focus in it for immediate editing.
/// @param event The button event.
void IngexguiFrame::OnClearDescription(wxCommandEvent & WXUNUSED(event))
{
    mDescriptionCtrl->Clear(); //generates a text change event which will disable the button
    mDescriptionCtrl->SetFocus(); //generates a focus event
}

/// Called when ENTER is pressed on the text description.
/// Gives the focus to something else so that conflicting shortcuts can be re-enabled.
/// @param event The command event.
void IngexguiFrame::OnDescriptionEnterKey(wxCommandEvent & WXUNUSED(event))
{
    FindWindowById(NOTEBOOK, this)->SetFocus(); //seems as good a thing as any - has to be something which is enabled
}

/// Responds to a logging event by logging the message within
void IngexguiFrame::OnLoggingEvent(wxCommandEvent& event)
{
    Log(event.GetString());
}

/// Deal with log messages: send to log stream
/// @param message The string to display.
void IngexguiFrame::Log(const wxString & message)
{
    if (wxDateTime::Today() != mToday) {
        mToday = wxDateTime::Today();
            wxLogMessage(wxT("Date: ") + mToday.FormatISODate());
    }
    // Contrary to the documentation, wxLogMessage does in fact want
    // a const wxChar* argument so this works fine with unicode.
    wxLogMessage(message);
}

/// Responds to the Jump to Timecode button being pressed by bringing up the Jump to Timecode dialogue.
void IngexguiFrame::OnJumpToTimecode(wxCommandEvent & WXUNUSED(event))
{
    if (mPlayer) mJumpToTimecodeDlg->Show(mTimepos->GetDefaultEditRate());
}

/// Responds to a Jump to Timecode event by trying to jump to a timecode
void IngexguiFrame::OnJumpToTimecodeEvent(wxCommandEvent & WXUNUSED(event))
{
    int64_t offset;
    if (mEventList->JumpToTimecode(mJumpToTimecodeDlg->GetTimecode(), offset)) { //timecode found, chunk selected and offset set
        mPlayer->SelectRecording(mEventList->GetCurrentChunkInfo());
        mPlayer->JumpToFrame(offset);
    }
    else {
        mJumpToTimecodeDlg->NotFoundMessage();
    }
}

/// Responds to the Take Snapshot button being pressed by doing just that
void IngexguiFrame::OnTakeSnapshot(wxCommandEvent & WXUNUSED(event))
{
    if (mPlayer) { //sanity check
        std::string fileName = mPlayer->GetCurrentFileName();
        unsigned long offset = mPlayer->GetLatestFrameDisplayed();
        if (!wxExecute(wxString::Format(wxT("player --exit-at-end --disable-shuttle --raw-out /tmp/snapshot%%d.raw --clip-start %ld --clip-duration 1 -m \""), offset) + wxString(fileName.c_str(), *wxConvCurrent) + wxT("\""), wxEXEC_SYNC)) { //FIXME: This assumes a command-line player is installed
            wxString snapshotStem = mPlayer->GetPlaybackName();
            for (size_t i = 0; i < wxFileName::GetForbiddenChars().Len(); i++) {
                snapshotStem.Replace(wxFileName::GetForbiddenChars().Mid(i, 1), wxT("_"));
            }
            snapshotStem.Replace(wxString(wxFileName::GetPathSeparator()), wxT("_"));
            snapshotStem = mSnapshotPath + wxFileName::GetPathSeparator() + wxT("snapshot-") + snapshotStem;
            //FIXME: The following command is specific to MJPEG SD formats!  Because the data order is planar, the cropping has to be done at the output stage or the chroma breaks.  This may happen to the already-encoded picture which is unfortunate as the black lines may introduce artifacts.  Cropping at the top is to get rid of the VBI; cropping at the bottom is to get rid of one black line (but an even number of lines must be removed).
            wxExecute(wxT("ffmpeg -s 720x592 -f rawvideo -pix_fmt yuv422p -y -i /tmp/snapshot0.raw -s 1024x592 -croptop 16 -cropbottom 2 \"") + snapshotStem + wxString::Format(wxT("-%03d.jpg\""), mSnapshotIndex++));
        }
    }
}

/// Returns true if in a state associated with recording.
bool IngexguiFrame::IsRecording()
{
    return RECORDING == mStatus || RUNNING_UP == mStatus || RUNNING_DOWN == mStatus;
}

/// Updates control states via periodic WX pseudo-events
void IngexguiFrame::OnUpdateUI(wxUpdateUIEvent& event)
{
    switch (event.GetId()) {
        case FRAME:
            if (mTree->GetRecorderCount()) {
                SetTitle(wxString(TITLE) + wxT(": ") + mRecorderGroup->GetCurrentProjectName()); //must have a project name if we have recorders
            }
            else {
                SetTitle(TITLE);
            }
            break;
        case REC_PROJECT_NAME:
            if (mTree->GetRecorderCount()) {
                event.SetText(mRecorderGroup->GetCurrentProjectName());
            }
            else {
                event.SetText(wxEmptyString);
            }
            break;
        case PLAY_PROJECT_NAME:
            if (mPlayer) {
                if (mPlayer->GetPlaybackType().IsEmpty()) {
                    mPlayProjectNameBox->GetStaticBox()->Hide(); //this doesn't have an ID
                }
                else {
                    mPlayProjectNameBox->GetStaticBox()->Show();
                    mPlayProjectNameBox->GetStaticBox()->SetLabel(mPlayer->GetPlaybackType());
                }
                dynamic_cast<wxStaticText*>(event.GetEventObject())->SetLabel(mPlayer->GetPlaybackName());
                dynamic_cast<wxStaticText*>(event.GetEventObject())->SetMinSize(((wxStaticText*) event.GetEventObject())->GetSize()); //or project name is partially obscured if there was no project name available when the app was started
                mPlayProjectNameBox->Layout();
            }
            break;
        case MENU_Up:
            if (mPlayer && RECORDINGS == mPlayer->GetMode()) {
                event.SetText(wxT("Move up cue point list\tUP"));
            }
            else {
                event.SetText(wxT("Move to start of file\tUP"));
            }
            break;
        case MENU_Down:
            if (mPlayer && RECORDINGS == mPlayer->GetMode()) {
                event.SetText(wxT("Move down cue point list\tDOWN"));
            }
            else {
                event.SetText(wxT("Move to end of file\tDOWN"));
            }
            break;
        case BUTTON_MENU_PrevTake:
            if (mEventList->AtStartOfTake() && (!mPlayer || !mPlayer->WithinRecording())) { //the start of this take is selected
                if (event.GetEventObject()->IsKindOf(CLASSINFO(wxButton))) {
                    event.SetText(wxT("Previous Take"));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Move to start of previous take"));
                }
                else { //it's the menu item
                    event.SetText(wxT("Move to start of previous take\tPGUP"));
                }
            }
            else { //somewhere within a take
                if (event.GetEventObject()->IsKindOf(CLASSINFO(wxButton))) {
                    event.SetText(wxT("Start of Take"));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Move to start of take"));
                }
                else { //it's the menu item
                    event.SetText(wxT("Move to start of take\tPGUP"));
                }
            }
            break;
        case BUTTON_MENU_NextTake:
            if (mEventList->InLastTake()) {
                if (event.GetEventObject()->IsKindOf(CLASSINFO(wxButton))) {
                    event.SetText(wxT("End of Take"));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Move to end of take"));
                }
                else { //it's the menu item
                    event.SetText(wxT("Move to end of take\tPGDN"));
                }
            }
            else {
                if (event.GetEventObject()->IsKindOf(CLASSINFO(wxButton))) {
                    event.SetText(wxT("Next Take"));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Move to start of next take"));
                }
                else { //it's the menu item
                    event.SetText(wxT("Move to start of next take\tPGDN"));
                }
            }
            break;
        case MENU_FirstTake:
            GetMenuBar()->FindItem(MENU_FirstTake)->SetItemLabel(wxString::Format(wxT("Move to start of first take%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tHOME")));
            break;
        case MENU_LastTake:
            GetMenuBar()->FindItem(MENU_LastTake)->SetItemLabel(wxString::Format(wxT("Move to start of last take%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tEND")));
            break;
        case MENU_JumpToTimecode:
            GetMenuBar()->FindItem(MENU_JumpToTimecode)->SetItemLabel(wxString::Format(wxT("Jump to a particular timecode%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tT")));
            break;
        case MENU_PlayBackwards:
            GetMenuBar()->FindItem(MENU_PlayBackwards)->SetItemLabel(wxString::Format(wxT("Play/fast backwards%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tJ")));
            break;
        case MENU_Pause:
            GetMenuBar()->FindItem(MENU_Pause)->SetItemLabel(wxString::Format(wxT("Pause%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tK")));
            break;
        case MENU_PlayForwards:
            GetMenuBar()->FindItem(MENU_PlayForwards)->SetItemLabel(wxString::Format(wxT("Play/fast forwards%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tL")));
            break;
        case MENU_PlayPause:
            GetMenuBar()->FindItem(MENU_PlayPause)->SetItemLabel(wxString::Format(wxT("Play/Pause%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tSPACE")));
            break;
        case MENU_StepBackwards:
            GetMenuBar()->FindItem(MENU_StepBackwards)->SetItemLabel(wxString::Format(wxT("Step backwards%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\t3")));
            break;
        case MENU_StepForwards:
            GetMenuBar()->FindItem(MENU_StepForwards)->SetItemLabel(wxString::Format(wxT("Step forwards%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\t4")));
            break;
        case MENU_UseTapeIds:
            event.Check(SetTapeIdsDlg::AreTapeIdsEnabled(mSavedState));
            break;
        case BUTTON_TapeId:
            dynamic_cast<wxButton*>(event.GetEventObject())->SetBackgroundColour(mTree->SomeEnabled() && !mTree->AreTapeIdsOK() ? BUTTON_WARNING_COLOUR : wxNullColour);
            break;
        case BITMAP_StatusCtrl:
            switch (mStatus) {
                case STOPPED:
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(stop);
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Stopped"));
                    break;
                case RUNNING_UP:
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(blank);
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Running up"));
                    break;
                case RECORDING:
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(record);
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Recording"));
                    break;
                case RUNNING_DOWN:
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(record);
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Running down"));
                    break;
                case PLAYING:
                case PLAYING_BACKWARDS:
                    if (mPlayer) {
                        dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(PLAYING == mStatus ?wxT("Playing") : wxT("Playing backwards"));
                        switch (mPlayer->GetSpeed()) {
                            case 1:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(play);
                                break;
                            case 2:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(ffx2);
                                break;
                            case 4:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(ffx4);
                                break;
                            case 8:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(ffx8);
                                break;
                            case 16:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(ffx16);
                                break;
                            case 32:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(ffx32);
                                break;
                            case 64:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(ffx64);
                                break;
                            case -1:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(play_backwards);
                                break;
                            case -2:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(frx2);
                                break;
                            case -4:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(frx4);
                                break;
                            case -8:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(frx8);
                                break;
                            case -16:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(frx16);
                                break;
                            case -32:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(frx32);
                                break;
                            case -64:
                                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(frx64);
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case PAUSED:
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(paused);
                    dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Paused"));
                    break;
            }
            break;
        case TIMECODE_BOX:
            if (mRecorderGroup->GetTimecodeRecorder().IsEmpty()) {
                dynamic_cast<wxStaticBox*>(event.GetEventObject())->SetLabel(wxT("Studio Timecode"));
            }
            else {
                dynamic_cast<wxStaticBox*>(event.GetEventObject())->SetLabel(wxT("Studio Timecode (") + mRecorderGroup->GetTimecodeRecorder() + wxT(")"));
            }
            break;
        case BITMAP_Alert:
            if (0 == mTree->GetRecorderCount()) {
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(blank);
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxEmptyString);
            }
            else if (mTree->HasProblem()) {
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(exclamation);
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Recorder problem"));
            }
            else if (!mTree->HasAllSignals()) {
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(concerned);
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Signals missing on enabled tracks"));
            }
            else if (mTree->IsUnknown()) {
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(blank);
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Awaiting response"));
            }
            else {
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetBitmap(happy);
                dynamic_cast<wxStaticBitmap*>(event.GetEventObject())->SetToolTip(wxT("Status OK"));
            }
            break;
        case BUTTON_MENU_Record: {
            wxString legend;
            if (mTree->GetRecorderCount()) {
                legend = wxString::Format(wxT("Record (%d track%s)"), mTree->EnabledTracks(), mTree->EnabledTracks() == 1 ? wxEmptyString : wxT("s"));
            }
            else {
                legend = wxT("Record");
            }
            if (event.GetEventObject()->IsKindOf(CLASSINFO(RecordButton))) {
                event.SetText(legend);
                switch (mStatus) {
                    case RUNNING_UP:
                        dynamic_cast<RecordButton*>(event.GetEventObject())->Pending(true);
                        break;
                    case RECORDING:
                        dynamic_cast<RecordButton*>(event.GetEventObject())->Record();
                        break;
                    case RUNNING_DOWN:
                        dynamic_cast<RecordButton*>(event.GetEventObject())->Pending(false);
                        break;
                    default:
                        dynamic_cast<RecordButton*>(event.GetEventObject())->Normal();
                        break;
                }
            }
            else {
                event.SetText(legend + wxT("\tF1"));
            }
            break;
        }
        case BUTTON_MENU_Stop:
            if (event.GetEventObject()->IsKindOf(CLASSINFO(wxButton))) {
                if (RECORDING == mStatus || RUNNING_DOWN == mStatus) {
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Stop recording after postroll"));
                }
                else {
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxEmptyString);
                }
            }
            break;
        case BUTTON_Cue:
            switch (mStatus) {
                case RUNNING_UP: case RUNNING_DOWN:
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxEmptyString);
                    break;
                case STOPPED:
                    if (mPlayer) {
                        event.SetText(wxT("Play"));
                        dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Play from current position"));
                    }
                    else {
                        event.SetText(wxEmptyString);
                        dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxEmptyString);
                    }
                    break;
                case RECORDING:
                    event.SetText(mCuePointsDlg->DefaultCueMode() ? wxT("Mark cue") : wxT("Mark cue..."));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Store a cue point for subsequent review"));
                    break;
                case PLAYING: case PLAYING_BACKWARDS:
                    event.SetText(wxT("Pause"));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Freeze playback"));
                    break;
                case PAUSED:
                    if (mPlayer && mPlayer->AtRecordingEnd()) {
                        event.SetText(wxT("Replay"));
                        dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Playback from start"));
                    }
                    else {
                    event.SetText(wxT("Play"));
                    dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(wxT("Play from current position"));
                    }
                    break;
            }
            break;
        case BUTTON_Chunk:
            event.SetText(mChunkingDlg->GetChunkButtonLabel());
            dynamic_cast<wxButton*>(event.GetEventObject())->SetToolTip(mChunkingDlg->GetChunkButtonToolTip());
            dynamic_cast<wxButton*>(event.GetEventObject())->SetBackgroundColour(mChunkingDlg->GetChunkButtonColour());
            break;
        case MENU_PlayerDisable:
            event.Check(!mPlayer || !mPlayer->IsEnabled());
            break;
        case MENU_PlayerExtAccelOutput:
            event.Check(mPlayer && prodauto::DUAL_DVS_AUTO_OUTPUT == mPlayer->GetOutputType(false)); //the desired output type - label will describe any fallback
            if (mPlayer) event.SetText(mPlayer->GetOutputTypeLabel(prodauto::DUAL_DVS_AUTO_OUTPUT));
            break;
        case MENU_PlayerAccelOutput:
            event.Check(mPlayer && prodauto::X11_AUTO_OUTPUT == mPlayer->GetOutputType(false)); //the desired output type - label will describe any fallback
            if (mPlayer) event.SetText(mPlayer->GetOutputTypeLabel(prodauto::X11_AUTO_OUTPUT));
            break;
        case MENU_PlayerExtOutput:
            event.Check(mPlayer && prodauto::DVS_OUTPUT == mPlayer->GetOutputType(false)); //the desired output type - label will describe any fallback
            if (mPlayer) event.SetText(mPlayer->GetOutputTypeLabel(prodauto::DVS_OUTPUT));
            break;
        case MENU_PlayerExtUnaccelOutput:
            event.Check(mPlayer && prodauto::DUAL_DVS_X11_OUTPUT == mPlayer->GetOutputType(false)); //the desired output type - label will describe any fallback
            if (mPlayer) event.SetText(mPlayer->GetOutputTypeLabel(prodauto::DUAL_DVS_X11_OUTPUT));
            break;
        case MENU_PlayerUnaccelOutput:
            event.Check(mPlayer && prodauto::X11_OUTPUT == mPlayer->GetOutputType(false)); //the desired output type - label will describe any fallback
            if (mPlayer) event.SetText(mPlayer->GetOutputTypeLabel(prodauto::X11_OUTPUT));
            break;
        case MENU_PlayerAbsoluteTimecode:
            event.Check(mPlayer && SOURCE_TIMECODE == mPlayer->GetOSDType());
            break;
        case MENU_PlayerRelativeTimecode:
            event.Check(mPlayer && CONTROL_TIMECODE == mPlayer->GetOSDType());
            break;
        case MENU_PlayerNoOSD:
            event.Check(mPlayer && OSD_OFF == mPlayer->GetOSDType());
            break;
        case MENU_PlayerOSDTop:
            event.Check(mPlayer && OSD_TOP == mPlayer->GetOSDPosition());
            break;
        case MENU_PlayerOSDMiddle:
            event.Check(mPlayer && OSD_MIDDLE == mPlayer->GetOSDPosition());
            break;
        case MENU_PlayerOSDBottom:
            event.Check(mPlayer && OSD_BOTTOM == mPlayer->GetOSDPosition());
            break;
        case MENU_PlayerNoLevelMeters:
            event.Check(mPlayer && 0 == mPlayer->GetNumLevelMeters());
            break;
        case MENU_PlayerTwoLevelMeters:
            event.Check(mPlayer && 2 == mPlayer->GetNumLevelMeters());
            break;
        case MENU_PlayerFourLevelMeters:
            event.Check(mPlayer && 4 == mPlayer->GetNumLevelMeters());
            break;
        case MENU_PlayerEightLevelMeters:
            event.Check(mPlayer && 8 == mPlayer->GetNumLevelMeters());
            break;
        case MENU_PlayerSixteenLevelMeters:
            event.Check(mPlayer && 16 == mPlayer->GetNumLevelMeters());
            break;
        case MENU_PlayerAudioFollowsVideo:
            event.Check(mPlayer && mPlayer->IsAudioFollowingVideo());
            break;
        case MENU_PlayerLimitSplitToQuad:
            event.Check(mPlayer && mPlayer->IsSplitLimitedToQuad());
            break;
        case MENU_PlayerDisableScalingFiltering:
            event.Check(mPlayer && mPlayer->IsScalingFilteringDisabled());
            break;
        case MENU_PlayerEnableSDIOSD:
            event.Check(mPlayer && mPlayer->IsSDIOSDEnabled());
            break;
        case MENU_Mute:
            GetMenuBar()->FindItem(MENU_Mute)->SetItemLabel(wxString::Format(wxT("Mute/unmute playback audio%s"), mTextFieldHasFocus ? wxEmptyString : wxT("\tM")));
            break;
        default:
            break;
    }
    //enable/disable
    bool found;
    bool enabled = OperationAllowed(event.GetId(), &found);
    if (found) event.Enable(enabled);
    //hide Jump to Timecode dialogue if it shouldn't be visible
    if (event.GetId() == BUTTON_JumpToTimecode //use the button event rather than the menu event because the menu event is affected by text field focus, which is irrelevant here
     && !enabled) mJumpToTimecodeDlg->Hide();
}

/// Indicates if the given operation is allowed.
/// @param operation The ID of the operation.
/// @param found If non-zero, dereferenced value is set to true if the shortcut is known about, or false if it is not.
/// @return True if shortcut is allowed.
bool IngexguiFrame::OperationAllowed(const int operation, bool* found)
{
    bool enabled = false;
    if (found) *found = true;
    switch (operation) {
        case MENU_SetRolls:
            enabled = mTree->GetRecorderCount();
            break;
        case MENU_SetProjectName:
            if (mTree->GetRecorderCount()) {
                switch (mStatus) {
                    case STOPPED: case RUNNING_DOWN: case PLAYING: case PLAYING_BACKWARDS: case PAUSED:
                        enabled = true;
                        break;
                    default:
                        break;
                }
            }
            break;
        case MENU_MarkCue:
            enabled = (RECORDING == mStatus) || (RUNNING_DOWN == mStatus);
            break;
        case MENU_PrevTrack:
            enabled = mPlayer && mPlayer->HasEarlierTrack();
            break;
        case MENU_NextTrack:
            enabled = mPlayer && mPlayer->HasLaterTrack();
            break;
        case MENU_StepBackwards:
            enabled = mPlayer && PAUSED == mStatus && mPlayer->WithinRecording();
            break;
        case MENU_StepForwards:
            enabled = mPlayer && PAUSED == mStatus && !mPlayer->AtRecordingEnd();
            break;
        case MENU_Up:
            if (mPlayer) {
                switch (mPlayer->GetMode()) {
                    case RECORDINGS:
                        enabled = mEventList->GetItemCount() && (!mEventList->AtTop() || mPlayer->WithinRecording());
                        break;
                    case FILES:
                        enabled = mPlayer->WithinRecording();
                        break;
                    default:
                        break;
                }
            }
            break;
        case MENU_Down:
            if (mPlayer) {
                switch (mPlayer->GetMode()) {
                    case RECORDINGS:
                        enabled = mEventList->GetItemCount() && !mEventList->AtBottom();
                        break;
                    case FILES:
                        enabled = !mPlayer->AtRecordingEnd();
                        break;
                    default:
                        break;
                }
            }
            break;
        case BUTTON_MENU_PrevTake:
            enabled = (!mPlayer || RECORDINGS == mPlayer->GetMode()) && mEventList->GetItemCount() && (!mEventList->AtTop() || !mPlayer || mPlayer->WithinRecording());
            break;
        case BUTTON_MENU_NextTake:
            enabled = (!mPlayer || RECORDINGS == mPlayer->GetMode()) && mEventList->GetItemCount() && !mEventList->AtBottom();
            break;
        case MENU_FirstTake:
            enabled = !mEventList->AtTop() || (mPlayer && mPlayer->WithinRecording());
            break;
        case MENU_LastTake:
            enabled = !mEventList->AtBottom();
            break;
        case MENU_PlayBackwards:
            enabled = mPlayer && mPlayer->WithinRecording() && !mPlayer->AtMaxReverseSpeed();
            break;
        case MENU_Pause:
            enabled = mPlayer && !IsRecording() && mPlayer->IsOK() && PAUSED != mStatus;
            break;
        case MENU_PlayForwards:
            enabled = mPlayer && !IsRecording() && mPlayer->IsOK() && !mPlayer->AtMaxForwardSpeed();
            break;
        case MENU_PlayPause:
            enabled = mPlayer && mPlayer->IsOK() && !IsRecording() && (PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || (PAUSED == mStatus && (!mPlayer->LastPlayingBackwards() || mPlayer->WithinRecording())));
            break;
        case MENU_ClearLog:
            enabled = mEventList->GetCurrentChunkInfo();
            break;
        case BUTTON_JumpToTimecode:
        case MENU_JumpToTimecode:
            enabled = mPlayer && RECORDINGS == mPlayer->GetMode() && !IsRecording() && mEventList->GetCurrentChunkInfo();
            break;
        case BUTTON_RecorderListRefresh:
            enabled = mRecorderGroup->IsEnabledForInput();
            break;
        case BUTTON_TapeId:
            enabled = SetTapeIdsDlg::AreTapeIdsEnabled(mSavedState) && mTree->UsingTapeIds();
            break;
        case BUTTON_MENU_Record:
        case MENU_TestMode:
#ifdef ALLOW_OVERLAPPED_RECORDINGS
            enabled = mTree->SomeEnabled() && mTree->AreTapeIdsOK() && mRecorderGroup->IsEnabledForInput() && (RECORDING != mStatus && RUNNING_UP != mStatus);
#else
            enabled = mTree->SomeEnabled() && mTree->AreTapeIdsOK() && mRecorderGroup->IsEnabledForInput() && (RECORDING != mStatus && RUNNING_UP != mStatus && RUNNING_DOWN != mStatus);
#endif
            break;
        case BUTTON_MENU_Stop:
            enabled = RUNNING_UP == mStatus || RECORDING == mStatus || RUNNING_DOWN == mStatus;
            break;
        case BUTTON_Cue:
            enabled = RECORDING == mStatus || PLAYING == mStatus || PLAYING_BACKWARDS == mStatus || PAUSED == mStatus;
            break;
        case BUTTON_Chunk:
            enabled = mChunkingDlg->CanChunk();
            break;
        case BUTTON_TakeSnapshot:
            enabled = mPlayer && mPlayer->IsOK() && mPlayer->GetCurrentFileName().size() && PAUSED == mStatus;
            break;
        case BUTTON_DeleteCue:
            enabled = RECORDING == mStatus //no point in deleting cue points after recording, as the descriptions have already been sent to the recorder
             && mEventList->LatestChunkCuePointIsSelected(); //it's an appropriate cue point
            break;
        case BUTTON_ClearDescription:
            enabled = mDescriptionCtrl->GetLineLength(0);
            break;
        case MENU_PlayRecordings:
            enabled = mPlayer && mPlayer->ModeAllowed(RECORDINGS);
            break;
#ifndef DISABLE_SHARED_MEM_SOURCE
        case MENU_EtoE:
            enabled = mPlayer && mPlayer->ModeAllowed(ETOE);
            break;
#endif
        case MENU_PlayFiles:
            enabled = mPlayer && mPlayer->ModeAllowed(FILES);
            break;
        case MENU_PlayerExtAccelOutput:
        case MENU_PlayerExtOutput:
        case MENU_PlayerExtUnaccelOutput:
            enabled = mPlayer && mPlayer->ExtOutputIsAvailable();
            break;
        default:
            if (found) *found = false;
            break;
    }
    return enabled;
}
