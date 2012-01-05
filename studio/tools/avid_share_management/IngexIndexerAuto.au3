#comments-start

$Id: IngexIndexerAuto.au3,v 1.1 2012/01/05 15:31:10 john_f Exp $

Avid share mapping and indexing control

Copyright (C) 2012  British Broadcasting Corporation.
All Rights Reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

#comments-end


#include <GUIConstantsEx.au3>
#include <StaticConstants.au3>
#include <ListviewConstants.au3>
#include <TreeviewConstants.au3>
#include <WindowsConstants.au3>
#include <ButtonConstants.au3>
#include <WinAPI.au3>
#include <StructureConstants.au3>
#include <NetShare.au3>
#include-once



Opt("MustDeclareVars", 1)
Opt("GUIOnEventMode", 1)

Global $currentShare, $gui1, $ChildWin, $logo;_main()

Global $filemenu, $fileitem, $helpmenu, $helpitem, $settingsitem ; menu()
Global $infoitem, $exititem, $recentfilesmenu, $seperator1, $viewmenu; file menu()
Global $file, $help  ; buttons()
Global $cancelbutton, $okbutton ; buttons()

Global $splash, $serverIP ; map()_Main()

Global $box[1000], $int = 1, $list, $isShare, $label

Global $admin = IniRead(@WorkingDir & "\ingexConfig.ini", "Settings", "admin", "")
Global $pmap = IniRead(@WorkingDir & "\ingexConfig.ini", "Settings", "mapping", "")
Global $drive = IniRead(@WorkingDir & "\ingexConfig.ini", "Settings", "drive_mapping", "")

Global $optAD, $IPSplash, $optPM, $optMP, $serverList, $addIP, $exitBtn
Global $addBtn, $removeBtn, $driveGUI, $uMount, $uMountDrive
_Main()


; #################################################
;	FUNCTIONS : _main()
; Generates GUI 
; Holds While loop to monitor for user input (msg)
; From user input trigers necessary functions
; Calls: Map(), Menu (), Buttons(), ReName(), _IP_Combo()
; Global Variables: $gui1, $logo, $admin, $fileitem, $infoitem
;					$helpitem, $settingsitem, $okbutton, $IPsplash
;					$splash, $cancelbutton, $exititem
; ################################################
Func _Main()

	$gui1 = GUICreate("Ingex Indexing Control", 450, 400)
	;Save the position of the parent window
	
	$logo = GUICtrlCreatePic("logo.gif", 10, 10, 171, 46)
	GUICtrlCreateLabel("Select the desired recording days below to enable indexing by this machine", 10, 92)
	GUICtrlCreateLabel("Deselect recordings to prevent indexing by this machine", 10, 112)
	
	
	
	Menu()
	Buttons()
	_IP_Combo()
	
	if $admin = "true" then MsgBox(0, "admin mode", "you are in admin mode")
	
	While 1
		
;~ 		GUICtrlSetOnEvent($fileitem, "_fileItem")
		GUICtrlSetOnEvent($infoitem, "_infoItem")
		GUICtrlSetOnEvent($helpitem, "_helpItem")
		GUICtrlSetOnEvent($settingsitem, "_settingsItem")
		GUICtrlSetOnEvent($okbutton, "_okbutton")
		GUICtrlSetOnEvent($IPsplash, "_IPsplash")
		GUICtrlSetOnEvent($splash, "_Proj_List")
		GUICtrlSetOnEvent($uMountDrive, "unMount")
		
		GUISetOnEvent($GUI_EVENT_CLOSE, "_exit", $gui1)
		GUICtrlSetOnEvent($cancelbutton, "_exit")
		GUICtrlSetOnEvent($exititem, "_exit")

		GUISetState(@SW_SHOW)

	WEnd
	
	
EndFunc


; #################################################
;	FUNCTIONS : _Proj_List()
; Reads $splash (drop down menu)  
; Clears existing lable
; Maps Drive 
; Re-lables GUI
; From user input trigers necessary functions
; Calls: _Clear_Lable(), ClearMem(), MapDrive(), Label()
; Global Variables: $splash, $currentShare, $serverIP
; #################################################
Func _Proj_List()

	IF GUICtrlRead($splash) <> "Select a share" Then 
		$currentShare = "\\" & $serverIP & "\" & GUICtrlRead($splash)
		_Clear_Label()
		ClearMem()
		MapDrive()
		GUISwitch($gui1)
		Label()
		GUISetState()
	EndIf
		
EndFunc


; #################################################
;	FUNCTIONS : _MapTest()
; Tests for "\Avid MediaFiles\MXF" inside $currentShare
; Success:
;		Generates List
;		Lables GUI
;		Sets GUI state for user input 
; Failure:
;		Tests for admin mode
;		If Admin:   warns user about lack of media files
;					lists all subdirectories
; 		Else:		tells user there are no media files
;		Clears Memory
; Calls: List(), Label(), ClearMem()
; Global Variables: $currentShare, $gui1, $admin
; #################################################
Func _MapTest()
	IF $currentShare <> "" Then
		IF FileExists($currentShare & "\Avid MediaFiles\MXF") Then 
			GUISwitch($gui1)
			List()
			Label()
			GUISetState()
		Else 
			if $admin = "true" Then
				MsgBox(16, "Warning", "There are no media files folder for this project," & @CRLF & "As in Admin mode, all subdirectories for this map will be listed")
				GUISwitch($gui1)
				List()
				GUISetState()
			else 
				MsgBox(16, "Warning", "There are no media files folder for this project")
			EndIf
		EndIf
	Else
		ClearMem()
	EndIf
EndFunc
		

; #################################################
;	FUNCTIONS : _infoItem()
; Prints hardcoded info about App
; Calls: 
; Global Variables: 
; #################################################
Func _infoItem()
	MsgBox(0, "Info", "Ingex share mapping and control" & @CRLF & "Copyright (c) 2012 BBC" & @CRLF & "Released under GNU General Public License" )
EndFunc	


; #################################################
;	FUNCTIONS : _helpItem()
; Prints hardcoded help info about drive mapping
; Calls: 
; Global Variables: 
; #################################################
Func _helpItem()
	MsgBox(0, "Help", "The drives are mapped with projects as follows:" & @CRLF & "Share (IP Address) -> Project Name -> /Avid MediaFiles/MXF/Ingex.*" & @CRLF & @CRLF & "To add serverIP's to the drop down list, use File -> Settings")
EndFunc


; #################################################
;	FUNCTIONS : _settingsItem()
; Gets location of $gui1 and opens a new child gui for settings
; Swiches control to $ChildWin 
; Sets CtrlOnEvents for:
;	$exitBtn =		_S_exit
;	$optAD =		_S_Admin
;	$optPM =		_S_PerMap
;	$optMP =		_S_DrvLtr
;	$addBtn = 		_S_addIP
;	$removeBtn =	_S_removeIP
; Calls: on user input any of the above _S_* functions
; Global Variables: $gui1, $childWin
; #################################################
Func _settingsItem()
	Local $ParentWin_Pos
	GUISetState(@SW_DISABLE, $gui1)
	GUISwitch($ChildWin)
	$ParentWin_Pos = WinGetPos($gui1, "")
	$ChildWin = GUICreate("Settings", 440, 540, $ParentWin_Pos[0] + 100, $ParentWin_Pos[1] + 100, -1, -1, $gui1)
	GUISwitch($ChildWin)
	_S_Display()
	GUISetState()
	;Show the child window/Make the child window visible
	
	GUISetOnEvent($GUI_EVENT_CLOSE, "_S_exit", $ChildWin)
	GUICtrlSetOnEvent($exitBtn, "_S_exit")
	GUICtrlSetOnEvent($optAD, "_S_Admin")
	GUICtrlSetOnEvent($optPM, "_S_PerMap")
	GUICtrlSetOnEvent($optMP, "_S_DrvLtr")
	GUICtrlSetOnEvent($addBtn, "_S_addIP")
	GUICtrlSetOnEvent($removeBtn, "_S_removeIP")
	
	GUISetState(@SW_SHOW)
EndFunc


; #################################################
;	FUNCTIONS : _S_exit
; Deletes $ChildWin
; Switchs control to $gui1
; Enables $gui1
; Restores $gui1
; Calls: 
; Global Variables: $gui1, $ChildWin
; #################################################
Func _S_exit()
	GUIDelete($ChildWin)
	GUISwitch($gui1)
	GUISetState(@SW_ENABLE, $gui1)
	GUISetState(@SW_RESTORE, $gui1)
	
EndFunc	


; #################################################
;	FUNCTIONS : _okbutton()
; Tests for chosen server ($IPsplash) and project ($splash)
; Success
;		If $isShare (share and project tested)
;		ReName() -> tests for any directory change
;		ClearMem() -> resets any used global variables
;		List() -> generates list of directories
;		Label() -> prints gui message
;
;		If not $isShare
;		_missingDrive() -> Checks network for share
;		Then follows if $isShare = True
; Calls: ReName(), ClearMem(), List(), Label(), _MissingDrive()
; Global Variables: $IPsplash, $splash, $isShare
; #################################################
Func _okbutton()
	if GUICtrlRead($IPsplash) = "Select an IP Address" Then
		MsgBox(16, "Error", "Please select an IP address first")
	ElseIf GUICtrlRead($splash) = "Select a share" Then
		MsgBox(16, "Error", "Please select a share first")
	Else
		if $isShare Then	
			ReName()
			ClearMem()
			List()
			Label()
		Else 
			_MissingDrive()			
			ReName()
			ClearMem()
			List()
			Label()
		EndIf
	EndIf
EndFunc

; #################################################
;	FUNCTIONS : UnMount()
; Called to unmount $isShare
; Calls: Map()
; Global Variables:
; #################################################
Func UnMount()
	if $isShare Then
		if MsgBox(1, "Warning", "This will unmount " & $uMount & @CRLF & DriveMapGet($uMount) ) = 1 Then
			DriveMapDel($uMount)
			GUICtrlSetState($uMountDrive, $GUI_DISABLE)
			ClearMem()
			_Clear_Label()
		EndIf
	EndIf
EndFunc

; #################################################
;	FUNCTIONS : _IPSplash()
; Reads Server Dropdown menu and sets $serverIP for Map()
;		ClearMem -> Clears and global variable
;		_Clear_Label() -> Clear gui message
;		Set $serverIP from $IPsplash, then Map()
; Calls: Map()
; Global Variables: $IPsplash, $splash, $serverIP
; #################################################
Func _IPsplash()
	If GUICtrlRead($IPsplash) <> "Select an IP Address" Then
		ClearMem()
		_Clear_Label()
		$serverIP = GUICtrlRead($IPsplash)
		GUICtrlDelete($splash)
		Map() 
		GUISetState(@SW_SHOW)
	EndIf
EndFunc


; #################################################
;	FUNCTIONS : exit()
; Exits App
; Calls: 
; Global Variables: 
; #################################################
Func _exit()
	Exit
EndFunc	


; #################################################
;	FUNCTIONS : _S_addIP()
; Adds IP address from form on 'Settings' window to list for dropdown
; Tests for wrong input
;		If Success: Error Msg 
;		If Fail: 	Disables window
;					Pings given IPaddress
;			If Success:
;					Re-enables window
;					Writes address to ini file and increases the address count
;					
; Calls: 
; Global Variables: 
; #################################################
Func _S_addIP()
	local $ipN, $ipAd
;~ 	MsgBox(0, "box", "box= " & GUICtrlRead($addIP,1))
	if GUICtrlRead($addIP,1) = "Add new server address" Then
		MsgBox(16, "Error", "Please add an IP address")
	Else
		GUISetState(@SW_DISABLE, $ChildWin)
		if ping(GUICtrlRead($addIP,1)) > 0 Then
			GUISetState(@SW_ENABLE, $ChildWin)
			$ipN =  IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "addresses", "") + 1
			$ipAd = IniWrite(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $ipN, GUICtrlRead($addIP))
			$ipAd = IniWrite(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "addresses", $ipN)
			GUICtrlSetData($serverList, GUICtrlRead($addIP))
			GUICtrlSetData($IPsplash, GUICtrlRead($addIP))
		Else
			GUISetState(@SW_ENABLE, $ChildWin)
			MsgBox(16, "Error", "Machine not Found")
		EndIf	
	EndIf
EndFunc	

Func _S_removeIP()
	local $ipN, $ipRm
	if GUICtrlRead($serverList,1) = "" Then
		MsgBox(16, "Error", "Please select an IP address")
	Else
;~ 	if $addIP <> "Add new server address" Then
		$ipN =  IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "addresses", "")
		$ipRm = GUICtrlRead($serverList,1)
		for $i = 1 to $ipN
;~ 				MsgBox(0, "Ini", $ipRM & @CRLF & IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $i, ""))
				if $ipRm = IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $i, "") Then
					IniDelete(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $i)
					IniWrite(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "addresses", $ipN - 1)
				EndIf
		Next		
;~ 		IniDelete(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $ipN)
		GUICtrlDelete($serverList)
		_S_ServerList()
		
		GUISwitch($gui1)
		GUICtrlDelete($IPsplash)
		_IP_Combo()
		GUISwitch($ChildWin)
	EndIf
EndFunc	
		

Func _S_Display()
		GUICtrlCreateLabel("Ingex Configuration", 10, 10)
			
		GUICtrlCreateGroup("Prefrences", 10, 45, 200, 90)
		$optPM = GUICtrlCreateCheckbox("Use persistant mapping", 25, 60)
		GUICtrlSetState($optPM, $GUI_UNCHECKED)
		If $pmap = "true" Then
			GUICtrlSetState($optPM, $GUI_CHECKED)
		EndIf
		$optAD = GUICtrlCreateCheckbox("Administration Rights", 25, 80)
		If $admin = "true" Then
			GUICtrlSetState($optAD, $GUI_CHECKED)
		EndIf
		$optMP = GUICtrlCreateCheckbox("Select specific drive letters", 25, 100)
		If $drive = "true" Then
			GUICtrlSetState($optMP, $GUI_CHECKED)
		EndIf
		GUICtrlCreateGroup("", -99, -99, 1, 1)  ;close group
		_S_IP_Address()
		$exitBtn = GUICtrlCreateButton("OK", 350, 495, 70, 20) 

EndFunc

Func _S_PerMap()
	if GUICtrlRead($optPM) = 4 Then
		if $pmap = "true" Then
			$pmap = "false"
			IniWrite(@WorkingDir & "\ingexConfig.ini", "Settings", "mapping", $pmap)
		EndIf
	ElseIf GUICtrlRead($optPM) = 1 Then
		if $pmap = "false" Then
			$pmap = "true"
			IniWrite(@WorkingDir & "\ingexConfig.ini", "Settings", "mapping", $pmap)
		EndIf
	EndIf

EndFunc	

Func _S_Admin()
	if GUICtrlRead($optAD) = 4 Then
		if $admin = "true" Then
			$admin = "false"
			IniWrite(@WorkingDir & "\ingexConfig.ini", "Settings", "admin", $admin)
		EndIf
	ElseIf GUICtrlRead($optAD) = 1 Then
		if $admin = "false" Then
			$admin = "true"
			IniWrite(@WorkingDir & "\ingexConfig.ini", "Settings", "admin", $admin)
		EndIf
	EndIf

EndFunc

Func _S_DrvLtr()
	if GUICtrlRead($optMP) = 4 Then
		if $drive = "true" Then
			$drive = "false"
			IniWrite(@WorkingDir & "\ingexConfig.ini", "Settings", "drive_mapping", $drive)
		EndIf
	ElseIf GUICtrlRead($optMP) = 1 Then
		if $drive = "false" Then
			$drive = "true"
			IniWrite(@WorkingDir & "\ingexConfig.ini", "Settings", "drive_mapping", $drive)
		EndIf
	EndIf

EndFunc


Func _S_IP_Address()
	local $ipN, $ipAd 
	GUICtrlCreateGroup("Storage Server Configuration", 10, 150, 400, 300)
;~ 	GUICtrlCreateLabel("Input IP Address list", 25, 170)
	$addIP = GUICtrlCreateInput("Add new server address", 20, 170, 300, 20)
	$addBtn = GUICtrlCreateButton("Add", 320, 170, 70, 20)
;~ 	GUICtrlSetState($addBtn, $GUI_FOCUS)
	$removeBtn = GUICtrlCreateButton("Remove", 320, 200, 70, 20)
	_S_ServerList()
	GUICtrlCreateGroup("", -99, -99, 1, 1)  ;close group
	
	
	
	
EndFunc	

Func _S_ServerList()
	GUISwitch($ChildWin)
	$serverList = GUICtrlCreateList("", 20, 200, 300, 250)
	local $ipN =  IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "addresses", "")
	for $i = 1 to $ipN
		GUICtrlSetData($serverList, IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $i, ""))
	Next
EndFunc

Func _MissingDrive()
	if MsgBox(1, "Warning", "The project selected is not mapped." & @CRLF & "Would you like to map this?") = 1 then
		if $pmap = "true" Then
			if $drive = "true" Then
				_L_DriveLettering()
			Else
				Local $map = DriveMapAdd("*", "\\" & $serverIP & "\" & GUICtrlRead($splash), 9)
				$currentShare = "\\" & $serverIP & "\" & GUICtrlRead($splash)
				_Set_IsShare()
				_MapTest()
				Label()
			EndIf
		Else
			if $drive = "true" Then
				_L_DriveLettering()
			Else
				Local $map = DriveMapAdd("*", "\\" & $serverIP & "\" & GUICtrlRead($splash), 8)
				$currentShare = "\\" & $serverIP & "\" & GUICtrlRead($splash)
				_Set_IsShare()
				_MapTest()
				Label()
			EndIf 
		EndIf
	Else
		$isShare = ""
		List()
		Label()
	EndIf
EndFunc

Func _L_exit()
	GUIDelete($driveGUI)
	GUISwitch($gui1)
	Label()
	GUISetState(@SW_ENABLE, $gui1)
	GUISetState(@SW_RESTORE, $gui1)
	_MapTest()
	
EndFunc	

Func _L_DriveLettering()
	Local $ParentWin_Pos
	
	GUISetState(@SW_DISABLE, $gui1)
	$ParentWin_Pos = WinGetPos($gui1, "")
	$driveGUI = GUICreate("Map Letter Select", 300, 80, $ParentWin_Pos[0] + 100, $ParentWin_Pos[1] + 100, -1, -1, $gui1)
	GUISwitch($driveGUI)
	
	GUICtrlCreateLabel("Select letter for media drive's mapping", 20, 10)
	_DriveLetters()
	GUISetState()
	
	Local $exitBtn = GUICtrlCreateButton("Apply", 200, 35, 70, 20)
	
	
	GUISetOnEvent($GUI_EVENT_CLOSE, "_L_exit", $driveGUI)
	GUICtrlSetOnEvent($exitBtn, "_L_exit")
	GUICtrlSetOnEvent($driveSplash, "_L_map")
	
	GUISetState(@SW_SHOW)

	
EndFunc

Func _Set_IsShare()
	$isShare = DriveGetLabel("\\" & $serverIP & "\" & GUICtrlRead($splash))
	If @error Then
		MsgBox(1, "Error", "Unable to Map Drive= " & @error)
	EndIf	
EndFunc


Func _L_map()
	If GUICtrlRead($driveSplash) = "Select a drive letter" Then
		MsgBox(16, "Error", "Please select a drive letter")
	ElseIf GUICtrlRead($driveSplash) = "Any available drive" Then
		if $pmap = "true" Then
			Local $map = DriveMapAdd("*", "\\" & $serverIP & "\" & GUICtrlRead($splash), 9)
			_Set_IsShare()
		Else
			Local $map = DriveMapAdd("*", "\\" & $serverIP & "\" & GUICtrlRead($splash), 8)
			_Set_IsShare()
		EndIf
	Else 
		if $pmap = "true" Then
			Local $map = DriveMapAdd(GUICtrlRead($driveSplash), "\\" & $serverIP & "\" & GUICtrlRead($splash), 9)
			_Set_IsShare()
		Else
			Local $map = DriveMapAdd(GUICtrlRead($driveSplash), "\\" & $serverIP & "\" & GUICtrlRead($splash), 8)
			_Set_IsShare()
		EndIf
	EndIf
	
EndFunc

Func _DriveLetters()
	Local $i, $iI, $drives = DriveGetDrive("all")	
	Global $driveSplash = GUICtrlCreateCombo("Select a drive letter", 45, 35, 130)

	GUICtrlSetState(-1, $GUI_CHECKED)
	Local $letters[21], $used[21], $a = 1
	
	GUICtrlSetData($driveSplash, "Any available drive")
	
	$letters[0] = 20
	$letters[1] = "g"
	$letters[2] = "h"
	$letters[3] = "i"
	$letters[4] = "j"
	$letters[5] = "k"
	$letters[6] = "l"
	$letters[7] = "m"
	$letters[8] = "n"
	$letters[9] = "o"
	$letters[10] = "p"
	$letters[11] = "q"
	$letters[12] = "r"
	$letters[13] = "s"
	$letters[14] = "t"
	$letters[15] = "u"
	$letters[16] = "v"
	$letters[17] = "w"
	$letters[18] = "x"
	$letters[19] = "y"
	$letters[20] = "z"

	local $test
	For $i = 1 to $letters[0]
		$test = 0
		For $iI = 1 to $drives[0]
			if $letters[$i] & ":" = $drives[$iI] Then
;~ 				GUICtrlSetData($driveSplash, $letters[$i])
				$test = 1
			EndIf
		Next
		If $test <> 1 Then
			GUICtrlSetData($driveSplash, StringUpper($letters[$i]) & ":")
		EndIf
		
	Next
	
EndFunc	

Func ClearMem()
	while $int > 0
		GUICtrlDelete($box[$int])
		$box[$int] = 0
		$int = $int - 1
	WEnd	
	GUICtrlDelete($list)
	$int = 1
	
EndFunc


Func _Net_Share_APIBufferFree($pBuffer)
	Local $aResult
	
	$aResult = DllCall("NetAPI32.dll", "int", "NetApiBufferFree", "ptr", $pBuffer)
	Return $aResult[0] = 0
	
EndFunc

Func _Clear_Label()
		if $uMount Then
			GUICtrlDelete($uMount)
		EndIf
		GUICtrlDelete($label)
EndFunc


Func Label()
	_Clear_Label()
	if GUICtrlRead($splash)= "Select a share" Then
		$label = GUICtrlCreateLabel("                                       " & @CRLF & "                                       " & @CRLF, 250, 40) 
	ElseIf $currentShare <> "" Then
		if Not $isShare Then
			$label = GUICtrlCreateLabel("WARNING" & @CRLF & "This media store is not mapped", 250, 40) 
			GUICtrlSetState($uMountDrive, $GUI_DISABLE)
		Else
			Local $drives = DriveGetDrive("all")
			for $i =1 to $drives[0]
				if DriveMapGet($drives[$i]) = $currentShare Then
					$label = GUICtrlCreateLabel("Media Mapped to: " & StringUpper($drives[$i]) & "                                 " & @CRLF & "                                 " & @CRLF & "                                  ", 250, 40) 
					GUICtrlSetState($uMountDrive, $GUI_ENABLE)
					$uMount = StringUpper($drives[$i])
				EndIf
			Next
		EndIf
	EndIf
EndFunc




; #################################################
;	FUNCTIONS : Menu()
; Creates the top of GUI menu options
; Will have ability to open and edit .ini file: STILL TODO
;
; Uses Global Varables: $filemenu, $fileitem, $helpMenu, $helpitem, $infoitem, $exititem, $seperator1
; 
; #################################################

Func Menu()
	
	$filemenu = GUICtrlCreateMenu("&File")
	
	$helpmenu = GUICtrlCreateMenu("&Help")
	$helpitem = GUICtrlCreateMenuItem("Help", $helpmenu)
	GUICtrlSetState(-1, $GUI_DEFBUTTON)
	
	$settingsitem = GUICtrlCreateMenuItem("Settings", $filemenu)
	GUICtrlSetState(-1, $GUI_DEFBUTTON)
	$infoitem = GUICtrlCreateMenuItem("Info", $filemenu)
	$exititem = GUICtrlCreateMenuItem("Exit", $filemenu)
	
	
	$seperator1 = GUICtrlCreateMenuItem("", $filemenu, 2) ; create a seperator line
		
	GUISetState()
	
	
EndFunc	


; #################################################
;	FUNCTIONS : Buttons()
; Places Buttons on GUI
; 
; Uses Global Varables: $okbutton, $cancelbutton
; #################################################

Func Buttons ()

	
	$okbutton = GUICtrlCreateButton("Apply",285 , 350, 70 ,20)
	GUICtrlSetState(-1, $GUI_FOCUS)
	$cancelbutton = GUICtrlCreateButton("Exit", 365, 350, 70, 20)
	$uMountDrive = GUICtrlCreateButton("Un-Mount", 320, 300, 70, 20)
	GUICtrlSetState($uMountDrive, $GUI_DISABLE)

EndFunc

; #################################################
;	FUNCTIONS : IniEdit()
; Opens Ini File to be edited
; 
; Called after $settingsitem from File Menu
; #################################################
Func IniEdit()

	Local $logo
	Local $msg2, $text, $item, $cancelbutton2
	
	
	
	$gui2 = GUICreate("Ingex Settings", 300, 300)

	$logo = GUICtrlCreatePic("logo.gif", 10, 10, 171, 46)
	GUISetState()
	GUISetStyle($WS_POPUP)
	GUICtrlCreateLabel("Configuration Change", 10, 92)
	
	$cancelbutton2 = GUICtrlCreateButton("Exit", 220, 225, 70, 20)
	
	
	While 1
		$msg2 = GUIGetMsg()		
		IF $msg2 = $GUI_EVENT_CLOSE Or $msg2 = $cancelbutton2 Then ExitLoop
	WEnd
	GUIDelete($gui2)


	

EndFunc
	

; #################################################
;	FUNCTIONS : Map()
; Reads .ini file and tests server first- then mounts all aviable drives 
; Success -> displays all available projects in "Combo List" (drop down) 
; INI: workingdirectory\ingexConfig.ini is required
; Uses Global Varables: $serverIP
; Note: Max of 100 projects in .ini file 
; #################################################

Func _IP_Combo()
	GUISwitch($gui1)
	Local $iI
	
;~ 	$ipVAR = IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip", "")
	Local $ipNum = IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "addresses", "")
	If $ipNum <> "" Then
		$IPSplash = GUICtrlCreateCombo("Select an IP Address", 240, 10, 200)
		GUISetState()
		Local $ipVAR[$ipNum + 1]
		$ipVAR[0] = $ipNum 
;~ 		MsgBox(0, "IP NUM", $ipNum)
		for $iI = 1 to $ipVAR[0] 
;~ 			MsgBox(0, "iI", $iI)
			$ipVAR[$iI] = IniRead(@WorkingDir & "\ingexConfig.ini", "SharedStorage", "ip" & $iI, "")
;~ 			MsgBox(0, "ip", $ipVAR[$iI])
			GUICtrlSetData($IPSplash, $ipVAR[$iI])
		Next
	EndIf	
;~ 	If $ipVAR <> "" Then
;~ 		IF Ping($ipVAR) > 0 Then
;~ 			$serverIP = $ipVAR
;~ 		EndIf
;~ 	EndIf

EndFunc

Func _IP_Missing()
	MsgBox(0, "Error", "IP Address not found" & @CRLF & "Please use the settings menu item to configure")

EndFunc




Func Map()
	
	Local $projName[100], $projChoice, $i, $test, $maptest, $splashBox, $gui2, $serverIPtest, $iI
	
	If Ping($serverIP) = 0 Then	
		_IP_Missing()

		
	Else	
		$splash = GUICtrlCreateCombo("Select a share", 10, 65, 200)
		local $tInfo, $iI

		local $ret = DllCall("NetAPI32.dll", "int", "NetShareEnum", "wstr", "\\" & $serverIP, "int", 2, "int*", 0, "int", -1, "int*", 0, "int*", 0, "int*", 0)
		if @error Then
			MsgBox(0, "error", "error here as well too: " & @error)
		EndIf
;~ 		MsgBox(0, "here", "here " & $ret[5]) 
		Local $iCount = $ret[5]
		
		Local $aInfo[$iCount + 1][8]
		$aInfo[0][0] = $iCount
		Local $pInfo = $ret[3]
		If $ret[5] = 0 Then
			MsgBox(16, "Warning", "No shares found on this machine")
		Else
			For $iI = 1 to $iCount	
				$tInfo = DllStructCreate($tagSHARE_INFO_2, $pInfo)
	;~			MsgBox(0, "here", "here " & $iI) 
				$aInfo[$iI][0] = _WinAPI_WideCharToMultiByte(DllStructGetData($tInfo, "netname"))
	;~ 			MsgBox(0, "here", "here $aInfo[$iI][0] " & $aInfo[$iI][0]) 

				$aInfo[$iI][1] = DllStructGetData($tInfo, "type")
				$aInfo[$iI][2] = _WinAPI_WideCharToMultiByte(DllStructGetData($tInfo, "remark"))
				$aInfo[$iI][3] = (DllStructGetData($tInfo, "permissions"))
				$aInfo[$iI][4] = (DllStructGetData($tInfo, "max_uses"))
				$aInfo[$iI][5] = (DllStructGetData($tInfo, "current_user"))
				$aInfo[$iI][6] = _WinAPI_WideCharToMultiByte(DllStructGetData($tInfo, "path"))
				$aInfo[$iI][7] = _WinAPI_WideCharToMultiByte(DllStructGetData($tInfo, "passwd"))
				

				
				If $aInfo[$iI][1] = 0 Then
					GUICtrlSetData($splash, $aInfo[$iI][0])
				EndIf

				$pInfo += DllStructGetSize($tInfo)
			Next		
		EndIf
		
		_Net_Share_APIBufferFree($ret[3])
		
	EndIf
	
	If $serverIP > "" Then

		
		GUISetState(@SW_SHOW)	
			

	EndIf
	
EndFunc


; #################################################
;	FUNCTIONS : MapDrive()
; Called from _Main() when combo Box is activated
; Maps Selected Drive and Displayes Shared Storage IP Address on GUI
;
; Uses Global Varables: $serverIP, $currentShare
; Calls: GetDirectories if Success
; #################################################

Func MapDrive()
	Local $share, $i, $root
	$share = GUICtrlRead($splash)
	$root = "\\" & $serverIP & "\" & $share
	$isShare = ""
	
	local $drives= DriveGetDrive("NETWORK")
	If NOT @error Then
		for $i =1 to $drives[0]
;~ 			MsgBox(0, "", "found " & $drives[$i])
			if DriveMapGet($drives[$i]) = $root Then
;~ 				MsgBox(0, "Test", "Drives= " & DriveMapGet($drives[$i]) & @CRLF & "Root= " & $root)
				$isShare = DriveGetLabel("\\" & $serverIP & "\" & $share)
			EndIf
		Next
	EndIf
;~ 	MsgBox(0, "isShare = ", $isShare)
	if $isShare Then

		$currentShare = "\\" & $serverIP & "\" & $share
;~ 		GUICtrlCreateLabel("Connected to:" & @CRLF &  "    " & $currentShare, 200, 20) 
		IF FileExists($currentShare & "\Avid MediaFiles\MXF") Then 
			List()
		Else 
			MsgBox(16, "Warning", "There are no media files folder for this project!")
			if $admin = "true" Then
				List()
			EndIf
			
		EndIf
	Else
;~ 		MsgBox(0,"Error", "Drive not mapped")
		_MissingDrive()

		
	EndIf
EndFunc


; #################################################
;	FUNCTIONS : List()
; Generates List View 
; Displayes all relevant directories, and their current state
; 
; Uses Global Varables: $box[1000], $int
; Memory: FileFindFirstFile -> must be closed at end 
; #################################################

Func List()
	Local $dir[1000], $all[1000]
	Local $pos = 130, $t1, $t2, $test2, $i = 1, $ii = 1, $in = 1, $test, $test2 = "false"
	Local $filename, $name1, $name2
	$list = 0

	
	$list = GUICtrlCreateListView ("     Media Folder         | Indexer", 10, 130, 300, 200, $LVS_ICON, $LVS_EX_CHECKBOXES)	
;~ 	MsgBox(0,"Current Share", "Share = " & $currentShare)
	

	;LOOP: Searches through all files in the directory $currentShaer\nik\
	Local $searchA = FileFindFirstFile($currentShare & "\Avid MediaFiles\MXF\*")
	While 1
		$dir[$int] = FileFindNextFile($searchA)
		If @error Then ExitLoop
;~ 		MsgBox (0, "dir", "$dir[$int] = " & $dir[$int] & @CRLF & "int = " & $int) 
		
		If StringInStr($dir[$int], @computername, 0, 1) = 1 then
;~ 			msgbox(0, "Contains computerName", "computername " & $dir[$int])
			$box[$int] = GUICtrlCreateListViewItem($dir[$int] & "| LOCAL INDEXING", $list)
			GUICtrlSetState($box[$int], $GUI_CHECKED)
			$pos = $pos + 20		
			$int = $int+1	
		ElseIf StringInStr($dir[$int], "INGEX", 0, 1) = 1 then
;~ 			msgbox(0, "Contains ingex", "ingex " & $dir[$int])
			$box[$int] = GUICtrlCreateListViewItem($dir[$int] & "| no indexing", $list)
			GUICtrlSetState($box[$int], $GUI_UNCHECKED)
			$pos = $pos + 20
			$int = $int+1	
		Else
;~ 			MsgBox(0, "Neither", "Contains neither name " & $dir[$int])   
			$box[$int] = GUICtrlCreateListViewItem($dir[$int] & "| no indexing", $list)
			GUICtrlSetColor($box[$int], 0x999999)
			$pos = $pos + 20
			$int = $int+1	
		EndIf
	WEnd

	FileClose($searchA)
EndFunc


; #################################################
;	FUNCTIONS : ReName()
; Takes user input and changes the directory names to specified
; Success -> ??
; 
; Uses Global Varables: $box[1000], $int
; 
; #################################################

Func ReName()
	Local $i= $int, $filename, $test, $test2, $name1, $name2, $name3, $newName1, $newName2, $pos
	Local $searchT1, $searchT2
	
;~ 		MsgBox(0, "COMPUTER NAME", "COMPUTERNAME  " & @ComputerName )
		
	

	While $i <> 0
;~ 		MsgBox(0, "i", "i = " & $i)

;~ 		MsgBox(0, "File Names", GUICtrlRead($box[$i]) & @CRLF & @CRLF & GUICtrlRead($box[$i], 1))


		; IF: Item in list is "un-ticked" then compare to see if item is @ComputerName.* 
		; SUCCESS -> item has changed from ticked to un-ticked, reName from @computername.* to ingex.*

		
		If GUICtrlRead($box[$i], 1) = 4 Then			
			$filename = String(GUICtrlRead($box[$i]))
			$searchT1 = FileFindFirstFile($currentShare & "\Avid MediaFiles\MXF\" & @ComputerName & ".*")
			While 1
				$name1 = FileFindNextFile($searchT1)
				$test = $name1 & "| LOCAL INDEXING|"	
				
				If $test = $filename Then
					IF MsgBox(1, "Warning", "Local indexing on " & $name1 & "will no-longer be available " & @CRLF & "Are you sure?") = 1 then 
					
						$newName1 = StringReplace( $name1 , @ComputerName , "INGEX")
						DirMove($currentShare & "\Avid MediaFiles\MXF\" & $name1, $currentShare & "\Avid MediaFiles\MXF\" & $newName1)
						ExitLoop
					Else
						ExitLoop
					EndIf
				EndIf	
				If $test == "| LOCAL INDEXING|" Then ExitLoop	
				If @error Then ExitLoop
			WEnd		
			FileClose($searchT1)
		EndIf
		
		; IF: Item in list is "ticked" then compare to see if item is ingex.* 
		; SUCCESS -> item has changed from un-ticked to ticked, reName from ingex.* to @ComputerName.*
		If GUICtrlRead($box[$i], 1) = 1 Then			
			$filename = String(GUICtrlRead($box[$i]))
			$searchT2 = FileFindFirstFile($currentShare & "\Avid MediaFiles\MXF\*")
			While 1
				$name2 = FileFindNextFile($searchT2)
				$test2 = $name2 & "| no indexing|"	
				
				
				
				If $test2 = $filename Then
						If StringInStr($filename, "INGEX", 0, 1) = 0 then
							If $admin <> "true" Then
								MsgBox(0, "ERROR", "Unable to make changes due to permissions." & @CRLF & "Set administrator rights in [Settings]")
								ExitLoop
							Else 
;~ 								MsgBox(0, "oldName", "old name = " & $name2)
								IF MsgBox(1, "Warning", "This will take control over " & $name2 & @CRLF & "Are you sure?") = 1 then 
									$pos = StringInStr($name2, ".")
									
									if $pos = 0 Then
										$newName2 = @ComputerName & "." & $name2
									Else
										$name3 = StringTrimLeft($name2, $pos-1)	
										$newName2 = @ComputerName & $name3
									EndIf
								
;~ 									MsgBox(0, "newName", "New name = " & $newName2)
									DirMove($currentShare & "\Avid MediaFiles\MXF\" & $name2, $currentShare & "\Avid MediaFiles\MXF\" & $newName2)
								EndIf
							EndIf
						Else
				
	;~ 					MsgBox(0, "CHANGE", "From NO control to HAVE control")
						$newName2 = StringReplace( $name2 , "INGEX" , @ComputerName)
						DirMove($currentShare & "\Avid MediaFiles\MXF\" & $name2, $currentShare & "\Avid MediaFiles\MXF\" & $newName2)
						ExitLoop
					EndIf
				EndIf	
				If $test2 == "| no indexing |" OR  $test2 == "| |" Then ExitLoop	
				If @error Then ExitLoop
			WEnd		
			FileClose($searchT2)
		EndIf
		$i = $i-1
		
		
	WEnd


EndFunc

