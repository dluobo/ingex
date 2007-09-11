// RecorderSelectDlg.cpp : implementation file
//

#pragma warning(disable:4786) // // To suppress some <vector>-related warnings

#include "stdafx.h"
#include "gallerylogger.h"
#include "RecorderSelectDlg.h"

#include <CorbaUtil.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// RecorderSelectDlg dialog


RecorderSelectDlg::RecorderSelectDlg(CWnd* pParent /*=NULL*/)
	: CDialog(RecorderSelectDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(RecorderSelectDlg)
	mSelection = _T("");
	//}}AFX_DATA_INIT
}


void RecorderSelectDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(RecorderSelectDlg)
	DDX_Control(pDX, IDOK, mButtonOk);
	DDX_Control(pDX, IDC_LIST_RECORDERS, mRecorderList);
	DDX_LBString(pDX, IDC_LIST_RECORDERS, mSelection);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(RecorderSelectDlg, CDialog)
	//{{AFX_MSG_MAP(RecorderSelectDlg)
	ON_LBN_SELCHANGE(IDC_LIST_RECORDERS, OnSelchangeListRecorders)
	ON_LBN_DBLCLK(IDC_LIST_RECORDERS, OnDblclkListRecorders)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// RecorderSelectDlg message handlers

BOOL RecorderSelectDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	std::vector<std::string> recorders;
	CorbaUtil::Instance()->GetRecorderList(recorders);

	for(int i = 0; i < recorders.size(); ++i)
	{
		mRecorderList.AddString(recorders[i].c_str());
	}
	
	mButtonOk.EnableWindow(FALSE); // Button is greyed until a selection is made

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void RecorderSelectDlg::OnSelchangeListRecorders() 
{
	int sel = mRecorderList.GetCurSel();
	if (sel != LB_ERR)
	{
		CString selservice;
		mRecorderList.GetText(sel, selservice);
		if (!selservice.IsEmpty())
		{
			mButtonOk.EnableWindow(TRUE);
			return;
		}
	}
	mButtonOk.EnableWindow(FALSE);
}

void RecorderSelectDlg::OnDblclkListRecorders() 
{
	mButtonOk.PostMessage(WM_LBUTTONDOWN);
	mButtonOk.PostMessage(WM_LBUTTONUP);
}

const char * RecorderSelectDlg::Selection()
{
	return mSelection;
}
