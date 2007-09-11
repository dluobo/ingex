// GalleryLogger.h : main header file for the GALLERYLOGGER application
//

#if !defined(AFX_GALLERYLOGGER_H__8E7928F1_5A34_48EF_9BA5_223F5566D9B7__INCLUDED_)
#define AFX_GALLERYLOGGER_H__8E7928F1_5A34_48EF_9BA5_223F5566D9B7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerApp:
// See GalleryLogger.cpp for the implementation of this class
//

class CGalleryLoggerApp : public CWinApp
{
public:
	CGalleryLoggerApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGalleryLoggerApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CGalleryLoggerApp)
	afx_msg void OnAppAbout();
	afx_msg void OnFileImport();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
	virtual int ExitInstance();

	const TCHAR * NamingService();
	void NamingService(const TCHAR * ns);

private:
	CString mNamingServiceString;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GALLERYLOGGER_H__8E7928F1_5A34_48EF_9BA5_223F5566D9B7__INCLUDED_)
