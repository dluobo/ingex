#if !defined(AFX_RECORDERSELECTDLG_H__F62E9469_BC8C_43DA_916B_350EC92775A8__INCLUDED_)
#define AFX_RECORDERSELECTDLG_H__F62E9469_BC8C_43DA_916B_350EC92775A8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RecorderSelectDlg.h : header file
//

#include <vector>
#include <string>

/////////////////////////////////////////////////////////////////////////////
// RecorderSelectDlg dialog

class RecorderSelectDlg : public CDialog
{
// Construction
public:
	RecorderSelectDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(RecorderSelectDlg)
	enum { IDD = IDD_RECORDERSELECT };
	CButton	mButtonOk;
	CListBox	mRecorderList;
	CString	mSelection;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(RecorderSelectDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	const char * Selection();

protected:

	// Generated message map functions
	//{{AFX_MSG(RecorderSelectDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeListRecorders();
	afx_msg void OnDblclkListRecorders();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RECORDERSELECTDLG_H__F62E9469_BC8C_43DA_916B_350EC92775A8__INCLUDED_)
