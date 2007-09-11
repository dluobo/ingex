# Microsoft Developer Studio Project File - Name="routerlogger" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=routerlogger - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "routerlogger.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "routerlogger.mak" CFG="routerlogger - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "routerlogger - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "routerlogger - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "routerlogger - Win32 Debug"

# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "."
# PROP Intermediate_Dir "Debug\routerlogger"
# PROP Target_Dir ""
MTL=midl.exe
# ADD MTL /D "_DEBUG" /nologo /mktyplib203 /win32
# ADD CPP /nologo /MDd /W3 /Gm /GR /GX /Zi /Gy /I "$(ACE_ROOT)" /I "$(TAO_ROOT)" /I "$(TAO_ROOT)\orbsvcs" /I "..\Corba" /I "..\IDL\Generated" /I "..\IDL" /I "..\common" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x409 /i "$(ACE_ROOT)" /i "$(TAO_ROOT)" /i "$(TAO_ROOT)\orbsvcs" /i "..\Corba" /i "..\IDL\Generated" /i "..\IDL" /i "..\common" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /machine:IX86
# ADD LINK32 advapi32.lib user32.lib ACEd.lib TAOd.lib TAO_AnyTypeCoded.lib TAO_PortableServerd.lib TAO_CosNamingd.lib TAO_Valuetyped.lib TAO_CodecFactoryd.lib TAO_PId.lib TAO_Messagingd.lib common2d.lib idld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /libpath:"." /libpath:"$(ACE_ROOT)\lib" /libpath:"..\common" /libpath:"..\IDL"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "routerlogger - Win32 Release"

# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release\routerlogger"
# PROP Target_Dir ""
MTL=midl.exe
# ADD MTL /D "NDEBUG" /nologo /mktyplib203 /win32
# ADD CPP /nologo /MD /W3 /GR /GX /O2 /I "$(ACE_ROOT)" /I "$(TAO_ROOT)" /I "$(TAO_ROOT)\orbsvcs" /I "..\Corba" /I "..\IDL\Generated" /I "..\IDL" /I "..\common" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x409 /i "$(ACE_ROOT)" /i "$(TAO_ROOT)" /i "$(TAO_ROOT)\orbsvcs" /i "..\Corba" /i "..\IDL\Generated" /i "..\IDL" /i "..\common" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /machine:IX86
# ADD LINK32 advapi32.lib user32.lib ACE.lib TAO.lib TAO_AnyTypeCode.lib TAO_PortableServer.lib TAO_CosNaming.lib TAO_Valuetype.lib TAO_CodecFactory.lib TAO_PI.lib TAO_Messaging.lib common2.lib idl.lib /nologo /subsystem:console /pdb:none /machine:I386 /libpath:"." /libpath:"$(ACE_ROOT)\lib" /libpath:"..\common" /libpath:"..\IDL"

!ENDIF 

# Begin Target

# Name "routerlogger - Win32 Debug"
# Name "routerlogger - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;cxx;c"
# Begin Source File

SOURCE="DataSourceImpl.cpp"
# End Source File
# Begin Source File

SOURCE="quartzRouter.cpp"
# End Source File
# Begin Source File

SOURCE="routerlogger_main.cpp"
# End Source File
# Begin Source File

SOURCE="routerloggerApp.cpp"
# End Source File
# Begin Source File

SOURCE="SimplerouterloggerImpl.cpp"
# End Source File
# Begin Source File

SOURCE=.\SourceReader.cpp
# End Source File
# Begin Source File

SOURCE="StatusDistributor.cpp"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hh"
# Begin Source File

SOURCE="DataSourceImpl.h"
# End Source File
# Begin Source File

SOURCE="quartzRouter.h"
# End Source File
# Begin Source File

SOURCE="routerloggerApp.h"
# End Source File
# Begin Source File

SOURCE="SimplerouterloggerImpl.h"
# End Source File
# Begin Source File

SOURCE=.\SourceReader.h
# End Source File
# Begin Source File

SOURCE="StatusDistributor.h"
# End Source File
# End Group
# Begin Group "Documentation"

# PROP Default_Filter ""
# Begin Source File

SOURCE="output.txt"
# End Source File
# Begin Source File

SOURCE="README.txt"
# End Source File
# End Group
# End Target
# End Project
