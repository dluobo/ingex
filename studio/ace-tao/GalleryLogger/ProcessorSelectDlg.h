#if !defined(AFX_PROCESSORSELECTDLG_H__77C69E9C_C455_4400_9272_1D2B7495AFA9__INCLUDED_)
#define AFX_PROCESSORSELECTDLG_H__77C69E9C_C455_4400_9272_1D2B7495AFA9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProcessorSelectDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ProcessorSelectDlg dialog

class ProcessorSelectDlg : public CDialog
{
// Construction
public:
	ProcessorSelectDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(ProcessorSelectDlg)
	enum { IDD = IDD_PROCESSORSELECT };
	CButton	mButtonOk;
	CListBox	mList;
	CString	mSelection;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ProcessorSelectDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
public:
	const char * Selection() { return mSelection; }
protected:

	// Generated message map functions
	//{{AFX_MSG(ProcessorSelectDlg)
	afx_msg void OnSelchangeList();
	afx_msg void OnDblclkList();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSORSELECTDLG_H__77C69E9C_C455_4400_9272_1D2B7495AFA9__INCLUDED_)
