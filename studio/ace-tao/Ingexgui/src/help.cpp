/***************************************************************************
 *   $Id: help.cpp,v 1.26 2011/11/11 11:21:23 john_f Exp $                *
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

#include "help.h"
#include "ingexgui.h" //for constants
#include "controller.h" //for constants
#include <wx/choicebk.h>

/// Sets up a paged dialogue of help text.
/// @param parent The parent window.
HelpDlg::HelpDlg(wxWindow * parent)
: wxDialog(parent, wxID_ANY, (const wxString &) wxT("Ingex Control Help"), wxDefaultPosition, wxSize(600, 400), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) //cygwin needs the cast
{
    wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(sizer);
    wxChoicebook * choicebook = new wxChoicebook(this, -1);
    sizer->Add(choicebook, 1, wxEXPAND | wxALL, 10);

    wxTextCtrl * qs = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    wxString message = wxT("1) Click the recorder names you wish to use in the #Recorder Selection# box.\n\n2) Select/add a suitable project name from the #Misc->Set Project Name...# menu if desired.\n\n3) Set appropriate values for pre-roll and post-roll from the #Misc->Set pre- and post-roll...# menu if desired.\n\n4) If you don't want to record with all tracks, open up the #Recorders# tree to the desired level by clicking on the small arrows, and click on #R# symbols.\n\n5) Press the #Set tape IDs# button and make sure there is a suitable tape ID for each package of tracks, or disable tape IDs in the #Misc# menu.\n\n6) Fill in the #Description# box if desired.\n\n7) Press the #Record# button to start recording, and the #Stop# button to finish.");
    StyleAndWrite(qs, message);
    choicebook->AddPage(qs, wxT("Quick Start"));

    wxTextCtrl * project = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("Each recording must be associated with a project name, which is shown in the window header as well as in the #Record# tab.  A list of project names is obtained from the recorders you connect to, and the #Set project name...# option from the #Misc# menu allows you to add new names to this list, and also to select the one to use.  The selection is remembered between sessions, but if the controller cannot find a saved preferences file, when you first connect to a recorder it will prompt you to select from the list the recorder supplies.  If that list is empty, it will prompt you to enter a new project name.  If you cancel the request, you will be disconnected from the recorder.");
    StyleAndWrite(project, message);
    choicebook->AddPage(project, wxT("Project Names"));

    wxTextCtrl * cuePoints = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("Cue points, or locators, can be added to recordings.  From the #Misc# menu you can bring up a dialogue which enables you to select between generating simple markers or creating up to ten different predefined cue points, each with text and a colour.  The text is entered in the left hand column, while clicking on the right hand column produces a pop-up menu to select the colour.  If you press #ENTER# while editing the text, the next cell down becomes highlighted, allowing you to enter text for several cue points quickly.\n\nSee the \"Cue point display and controls\" help tab for instructions about how to add cue points to recordings.");
    StyleAndWrite(cuePoints, message);
    choicebook->AddPage(cuePoints, wxT("Cue Points (Locators)"));

    wxTextCtrl * recorder = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("In order to locate recorders, the controller must be given a reference to a CORBA name server as command line arguments.  If you started the controller from a desktop icon, it should already be set up correctly with these.  (The warning given when these arguments are omitted shows the required format.)\n\nAvailable recorders are listed in the #Recorder Selection# box.  #Refresh list# will update this, and can be used to try again should a transient network problem occur, or the collection of available recorders has changed since the last time the list was updated.\n\nSelecting a recorder from the list will attempt to connect to it.  If any tracks are currently recording, the controller will go into the record state, displaying an unknown recording duration.  Otherwise, it will go into the stopped state.  All channels are enabled for recording by default.  The selected recorder will display approximately how much time it can record for, given the free disk space.  If this is less than an hour, the recorder will be highlighted in orange.  Note that the figure doesn't take account of how many channels are enabled, instead basing its calculation on the worst-case of all channels.  It is also not aware of several recorders sharing the same disks, which will reduce recording time accordingly.  It is blanked during recording and is only updated at the end of a recording, on reconnection or when #Refesh list# is pressed.\n\nAny number of recorders can be selected.  Click on them again to disconnect (whereupon you will be asked to confirm; right-clicking avoids this step).  You cannot do this while recording.");
    StyleAndWrite(recorder, message);
    choicebook->AddPage(recorder, wxT("Recorder selection"));

    wxTextCtrl * roll = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("When connected to at least one recorder, a dialogue can be accessed from the #Misc# menu to set pre-roll and post-roll.  These settings are remembered between sessions.\n\nThe #Pre-roll# slider allows you to add a variable amount of pre-roll to each recording.  Its maximum value depends on the capabilities of the recorders.\n\nThe #Post-roll# slider operates in a similar way.  Recorders are likely to allow an infinite amount of post-roll, in which case it is limited to ") + wxString::Format(wxT("%d frame%s."), DEFAULT_MAX_POSTROLL, DEFAULT_MAX_POSTROLL == 1 ? wxEmptyString : wxT("s")) + wxT("  During post-roll the record button shows the \"running down\" state");
#ifdef ALLOW_OVERLAPPED_RECORDINGS
    message += wxT(" and a new recording can be started, which will overlap the current recording.");
#else
    message += wxT(".");
#endif
    StyleAndWrite(roll, message);
    choicebook->AddPage(roll, wxT("Pre-roll and Post-roll"));

    wxTextCtrl * tapeIds = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("If tape IDs are enabled in the #Misc# menu, recordings cannot be made unless all enabled tracks (apart from those of router recorders) have a corresponding tape ID.  #Set tape IDs# will be orange if a missing tape ID is preventing you from recording.  Pressing this button (whether orange or not) brings up a dialogue for manipulating tape IDs, which are remembered between sessions.  It is designed to allow you to enter and update the information for several tapes quickly.  Press the #Help# button on the dialogue for details about how to do this.");
    StyleAndWrite(tapeIds, message);
    choicebook->AddPage(tapeIds, wxT("Tape IDs"));

    wxTextCtrl * indicators = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("Below the recorder selector and refresh button is a row of indicators and ");
#ifndef DISABLE_SHARED_MEM_SOURCE
    message += wxT("three");
#else
    message += wxT("two");
#endif
    message += wxT(" buttons.  The first indicator shows the recorder status: stopped, running-up, recording, playing (forwards and backwards, and at various speeds), or paused.  Its function is echoed in the status text at the bottom of the window.\n\nTo the right of the status display is #Studio timecode#, as obtained from the recorders.  The title shows which recorder is supplying this timecode, if any.  (The controller prefers to use non-router recorders.)  It is followed by a health indicator which will warn of a problem - look at the source tree in the record tab for details.\n\nNext, there is #Position#, which corresponds to the current recording.  This will show question marks if the recorder was already recording when you connected to it.\n\nFinally, the three buttons control the player mode.  #Recordings# causes the player to play back recordings from the cue point display at the bottom of the window (and is only enabled if there are any recordings listed).  #Opened Files# plays back what has been opened from the #Player# menu (and is only enabled once something has been opened).");
#ifndef DISABLE_SHARED_MEM_SOURCE
    message += wxT("  #E to E#  shows live incoming material if the application is running on a recorder.");
#endif
    StyleAndWrite(indicators, message);
    choicebook->AddPage(indicators, wxT("Indicators and Player Mode Buttons"));

    wxTextCtrl * transport = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("When connected to at least one recorder, the #Record# button indicates the total number of tracks enabled to record.  If this is greater than zero, and all the enabled tracks (apart from router recorders) have tape IDs (if enabled in the #Misc# menu), the button will be a dull red, indicating that recording is possible.  (If lack of tape IDs is preventing a recording occurring, #Set Tape IDs# will be orange to remind you that you must define some tape IDs before you can record.)  When pressed, #Record# will flash bright red (\"running up\") until all applicable recorders have responded successfully to the record command, and will then become steady bright red.  Similarly, when stopping, it will flash dull red (\"running down\") during postroll and while waiting for a response.  #F1# provides a shortcut key for recording.\n\n#Stop# can only be pressed while recording, and sets the displayed status to stopped immediately, even if recorders are still post-rolling.  #Shift+F5# provides a shortcut key for stopping.\n\nWhile recording, #Mark cue# (or shortcut #F2#) adds a line to the cue point display to mark a point of interest.  Its label is #Mark cue...# if using multiple cue point types to indicate that it will bring up a dialogue.\n\nAfter at least one recording has been made, #Mark cue# becomes a play/pause button, explained in the instructions for the player.  #Stop# has no effect during playback.\n\n#Chunk# can be pressed during recording to create a new chunk immediately.  See separate help section on chunking for more details.\n\n#Record# can be pressed during playback, which will immediately halt the player (to reduce load on the recording system) and commence a new recording.");
    StyleAndWrite(transport, message);
    choicebook->AddPage(transport, wxT("Transport controls"));

    wxTextCtrl * recordTab = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("Below the transport controls is the #Record# tab, which can be clicked to reveal the #Project# name (as set from the #Misc# menu), the #Description# field and the record source tree.  The space given to this tab can be varied by dragging the dotted splitter line below it.\n\nThe contents of the #Description# field is read when #Stop# is pressed, and is associated with the recording just made.  Note that while the box has focus (i.e. it contains the cursor and you can type into it), shortcut keys which would interfere with the control (such as #J#, #K# and #L#) will type into the box rather than perform their normal shortcut function.\n\nThe source tree, below #Description#, can be expanded to four levels by clicking the small arrows.  These levels show each individual media track, grouped by package, in turn grouped by recorder.  At the track (lowest) level, the colour of the track name indicates its type: green for video, blue for audio and cyan for router.  If a video input has no signal, the background colour will be orange.  At the package level, the tape ID is shown in brackets after the package name.  (To configure this, press #Set tape IDs#.)  The source tree will also display status messages at the recorder level and the track level.\n\nWhile not recording, the symbol next to each entry in the tree can be clicked to toggle record enable for it and all child entries.  The symbols for the packages and above will reflect the individual tracks, with a partially enabled indication if appropriate.  While recording, the symbols change to show recording or partially recording.\n\nIf there is a problem or an inability to communicate, an alert symbol (exclamation mark) or query symbol (question mark) is shown.  This takes priority over the other indications, so parent symbols will reflect the status.  (The health indicator next to the timecode display will also show that a problem has occurred.)  Each recorder is interrogated every ") + (POLLING_INTERVAL == 1000 ? wxT("second") : wxString::Format(wxT("%d seconds"), POLLING_INTERVAL/1000)) + wxT(" or so and the symbols are updated accordingly.\n\nIf a video track with no signal is enabled to record, its orange background will propagate up the tree and cause the health indicator to change.  While a missing signal will not prevent a recording being made, it is a problem which requires attention.");
    StyleAndWrite(recordTab, message);
    choicebook->AddPage(recordTab, wxT("Record tab"));

    wxTextCtrl * player = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("As soon as a recording has been completed,");
#ifndef DISABLE_SHARED_MEM_SOURCE
    message += wxT(" or #E to E# is selected,");
#endif
    message += wxT(" the player is activated (unless disabled via the #Player# menu).");
#ifdef HAVE_DVS
    message += wxT("  If an SDI card has been detected, playback can be to an external monitor exclusively, or as well as, the computer monitor - select from the #Player type# submenu of the #Player# menu.  (If external playback is disabled, an SDI card will output whatever is connected to its input.)");
#endif
    message += wxT("\n\nThe player window will only appear for the first time when at least one video file from the recording is available, which may take a while, depending on post-roll and the configuration of the recorders.  If not all files appear simultaneously, the player will start with what is available and reload automatically (maintaining its position in the recording) as more files appear.\n\nThe video tracks recorded during the take appear as radio buttons when the #Playback# tab, below the transport controls, is clicked.  The space given to this tab can be varied by dragging the dotted splitter line below it.  Each radio button has a tool-tip showing the associated file name.  Only the tracks currently available are enabled.\n\nA split view is also provided, which will divide the player window into four or nine depending on the number of tracks available and whether the split is limited to four sources in the #Player# menu.  (If greater than four or nine, the first four or nine are shown.)  The player window title will show the names of the tracks displayed.  Note that it takes more processing power and bandwidth to display a split view than an individual track, so the split view may play slowly depending on the capabilities of the computer and the network.\n\nThe first pair of audio tracks are normally played, but a #Player# menu option allows the audio to follow the current video source.\n\nThe #Playback# tab also shows the project name used when the recording was made.\n\nPlayer-specific shortcut keys (see #Shortcuts# submenu of #Help# menu) are #F7# and #F8# to cycle through the tracks, #J#, #K# and #L# for reverse (with fast reverse)/pause/forwards (with fast forwards), #Space# to toggle between forwards/reverse (depending on what was last selected) and pause, #3# and #4# for stepping by frame, and #M# to toggle audio mute.  Shortcut keys will work when either the controller or the player window has focus on the computer desktop, but some will not work if the #Description# field has focus - see the #Record tab# help section.  In addition, when the player has focus, left and right arrows become available as alternatives to #3# and #4# for stepping.\n\nThe player also responds to the mouse.  You can drag the progress bar pointer, or click anywhere on the progress bar to jump to that position.  You can also click on the image to toggle between the split view and the source shown in the area in which you click.\n\nWhen a new recording is made, the player window is blanked rather than disappearing.  This allows the player to maintain its position on the screen during a session.");
    message += wxT("\n\nIt is possible to use the player to open arbitrary MXF and MOV files, or to play back complete recordings from a server.  To do this, select the appropriate option from the #Player#->#Open# submenu.  You can select multiple MXF files at once, whereas you can only select one MOV file.\n\nThe #From server...# option (shortcut #Ctrl-O#) brings up a dialogue which allows you to select a recording (group of MXF files) by project name, date and time.  If there is no directory for offline quality files on the server, the online quality files are browsed, the #Prefer Online# button will have no effect, and #Online only# will be shown next to it.  However, if an offline directory is present, this will be used to browse the files, because offline files will appear there first and, being smaller, require less server bandwidth to play.  If any online files of the selected recording are found, #Online available# is shown next to the #Prefer Online# button, and if this button is pressed in, it will be orange and the online files will be played.  (Note that if copying to the server is still in progress, a recording may be missing some audio or video files despite being shown, and the offline files will appear before the online files.  If files are missing, try opening the recording again later.)");
    message += wxT("\n\nWhen you have chosen file(s) to play, the #Play files# button is enabled, which allows you to switch between playing the file(s), recordings you have made this session, and E-E mode.");
    message += wxT("\n\nThe #Player# menu also contains options to move the on-screen display around, show the position within the file on-screen instead of timecode, and to switch off the on-screen display completely");
#ifdef HAVE_DVS
    message += wxT(", or on the external monitor");
#endif
    message += wxT(".  Another option allows an \"unaccelerated\" player to be used, if the computer screen display is garbled due to a hardware incompatibility.  Normally, the controller tries to use an \"accelerated\" player, because this takes less processing power, has the correct aspect ratio, and can be re-sized.  Only one accelerated player can be in operation at a time, so a second controller will produce an unaccelerated player (indicated in the player's window header).  If the first controller is subsequently stopped, or its player closed, the second player will revert to accelerated when it loads a different recording.");
    StyleAndWrite(player, message);
    choicebook->AddPage(player, wxT("Video player and playback tab"));

    wxTextCtrl * jogshuttle = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("A Shuttle Pro or Shuttle Pro 2 USB jog/shuttle control can be used to provide the following functions:\n\nJog wheel and shuttle ring: jog/shuttle the player.\n\nTop row of buttons (L-R):\n\t#Record#\n\t#Mark cue#\n\t#Stop#*\n\t#Stop#.*\n\n*The two right hand buttons need to be pressed simultaneously, or one of the buttons plus one of the black buttons on the Shuttle Pro 2, to stop.  If you are entering a cue point, the dialogue will be dismissed and the cue point will not be stored.\n\nSecond row of buttons (L-R):\n\tPlay #Recordings# mode (if available)\n\tPlay/fast backwards\n\tPause");
#ifndef DISABLE_SHARED_MEM_SOURCE
    message += wxT(" if playing, then toggle #E to E# mode");
#endif
    message += wxT("\n\tPlay/fast forwards\n\tPlay #Opened Files# mode (if opened).");
    message += wxT("\n\nLeft hand long silver buttons:\n\tUp/Down cue points.\n\nRight hand long silver buttons:\n\tsame as #Previous Take# and #Next Take# buttons.");
    StyleAndWrite(jogshuttle, message);
    choicebook->AddPage(jogshuttle, wxT("Jog/Shuttle Control"));


    wxTextCtrl * cues = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("At the bottom of the controller window is a list of start, stop, chunk and cue point events.  The space given to this display can be varied by dragging the dotted splitter line above it.  The display shows the type of event, when it occurred both in absolute timecode and relative position, and a description of the event.  For start and chunk events, the description is copied from the #Description# text entry field when the recording is finished or a new chunk is started.  For stop events, the description is the project name.\n\nCue points can be added at will during recording with #Mark cue# (or #F2#) or the jog/shuttle control.  If the cue point mode (accessed from the #Misc# menu) is set to use a single cue point type, a cue point with no description will immediately be added.  Otherwise, the current timecode will be captured and a dialogue will appear showing this and the list of predefined cue points.  One of these can be chosen by pressing a number key, clicking on a row label or scrolling the highlighted row with the jog wheel and pressing #F2#, #ENTER# or the jog/shuttle button that brought up the dialogue (see the \"Jog/Shuttle Control\" help tab).  The highlighted description can also be edited by clicking it, after which it can be chosen quickly by pressing #ENTER# twice.\n\nOnce a cue point has been added to the list, its description (but not its colour) can be edited by clicking on the appropriate entry in the #Event# column, or it can be selected and removed with #Delete Cue Point#.  This can only be done while recording of the current chunk is still in progress, because the cue points are sent to the recorder(s) at the end of the chunk.\n\nDuring playback, cue points are shown in the player and are automatically highlighted as they are played through.  The player will jump to them if they are clicked, and start playing from them if they are double-clicked.  (Note that the player's burnt-in timecode display appears in red when at a cue point.)  The adjacent take navigation buttons allow different recordings to be selected quickly (and have shortcuts - see the #Help->Shortcuts# menu).\n\nThere is a short pause when playing from one chunk to another, due to the player loading the files corresponding to the new chunk.\n\nThere are #Misc# menu options to clear the cue point list, and prevent it logging more than one recording.  There is also an option to adjust the maximum number of chunks in the list, beyond which the earliest chunks are removed.  This may result in the earliest recording not having a start entry.\n\nThe contents of the list are remembered, but only for the current day.");
    StyleAndWrite(cues, message);
    choicebook->AddPage(cues, wxT("Cue point display and controls"));

    wxTextCtrl * chunking = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("Chunking is splitting a recording up into shorter files to allow copying/editing before completion.  The controller achieves chunking by instructing the recorder(s) to stop recording and then start recording one frame later.\n\nChunking is set up from the #Chunking...# option in the #Misc# menu.  You can enable or disable automatic chunking with the tick box, and set the chunking interval in minutes.\n\nIn addition, you can align the chunks so that they happen at precise timecodes, such as the top of each minute or quarter hour (seconds and frames both zero; minutes zero, 15, 30 or 45).  The alignment facility works by performing the first chunking operation at the specified point.  For example, if you have set the alignment to 15 minutes, and you start recording when timecode is currently 10:20:03:12, the first chunking will occur at 10:30:00:00 regardless of the chunk length; thereafter it will chunk at the specified interval even if it is shorter than 15 minutes.  (If a recording is started less than about 30 seconds before the alignment point, the chunking operation is delayed until the next alignment point to avoid creating a very short chunk.)  If settings are changed in the chunking dialogue while a recording is in progress, the current chunk will not be affected but the next chunk's length will correspond to the new settings.\n\nThe #Chunk# button in the main window starts a new chunk whenever it is pressed, with the length of this chunk being calculated according to the automatic chunking settings, as if it were the first chunk in the recording.  The button shows the approximate time in minutes and seconds to the next chunk; if automatic chunking is not enabled it will show dashes, and if automatic chunking with alignment is enabled, but a recording is not taking place, it will show question marks because the length of the first chunk is not known until recording commences.");
    StyleAndWrite(chunking, message);
    choicebook->AddPage(chunking, wxT("Chunking"));

    wxTextCtrl * test = new wxTextCtrl(choicebook, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    message = wxT("The #Misc# menu has a #Test mode...# option.  This produces a dialogue box which allows you to make repeated random or fixed length recordings, with random or fixed length gaps in between.  To use test mode, first create a situation where recording is possible, then open the dialogue and adjust the record and gap time minimum and maximum values if necessary.  (If minimum and maximum values are the same, random times will not be generated.)\n\nYou can also enable automatic erasing of the oldest recordings to try to prevent the disk(s) filling up.  (Note that this will only remove files from the currently-used directories of the first encodings of the connected recorders.  If recorders are doing additional encodings, these files will not be erased, so arrange for the largest files to be generated by the first encoding to maximise the time before the disk(s) is/are full.)\n\nWhen #Run# is pressed, recordings will be made until #Run# is pressed again or the dialogue is closed.  If the dialogue is closed while recording is happening, recording will continue until stopped manually.");
    StyleAndWrite(test, message);
    choicebook->AddPage(test, wxT("Test mode"));
};

/// Write the given string to the given text box, interpreting styling characters
/// Styling characters: # indicates start and end of red text
/// @param ctrl The text box to write to
/// @param message The string to write
void HelpDlg::StyleAndWrite(wxTextCtrl * ctrl, wxString & message) {
    wxTextAttr styled(wxColour(wxT("RED")));
    int stylePos;
    while (wxNOT_FOUND != (stylePos = message.Find(wxT("#")))) {
        ctrl->WriteText(message.Left(stylePos)); //non-styled text
        long styleStart = ctrl->GetLastPosition();
        message = message.Mid(stylePos + 1); //remove text that's been written (plus style start char)
        stylePos = message.Find(wxT("#"));
        ctrl->WriteText(message.Left(stylePos)); //going to be styled text
        ctrl->SetStyle(styleStart, ctrl->GetLastPosition(), styled);
        message = message.Mid(stylePos + 1); //remove text that's been written (plus style end char)
    }
    ctrl->WriteText(message);
}


/// Sets up a version reporting dialogue.
/// @param parent The parent window.
AboutDlg::AboutDlg(wxWindow * parent)
: wxDialog(parent, wxID_ANY, (const wxString &) wxT("About"))
{
    wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(sizer);
    wxTextCtrl * textBox = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_RICH | wxTE_MULTILINE | wxTE_AUTO_URL | wxTE_CENTRE);
    sizer->Add(textBox, 1, wxEXPAND | wxALL, 10);
    wxString message = wxT("This is Ingexgui, the Ingex tapeless studio recorder controller.  DVS SDI card support ");
#ifndef HAVE_DVS
    message += wxT("not ");
#endif
    message += wxT("included.  Shared memory (\"E to E\") support ");
#ifdef DISABLE_SHARED_MEM_SOURCE
    message += wxT("not ");
#endif
    message += wxT("included.  Please send feedback to matthewmarks@users.sourceforge.net.\n\nVersion $Id: help.cpp,v 1.26 2011/11/11 11:21:23 john_f Exp $\n\nCopyright (C) British Broadcasting Corporation 2006-2009 - All rights reserved.\n\n$Date: 2011/11/11 11:21:23 $.");
    textBox->SetValue(message);
};
