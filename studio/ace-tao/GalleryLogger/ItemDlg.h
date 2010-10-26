/*
 * $Id: ItemDlg.h,v 1.2 2010/10/26 18:34:55 john_f Exp $
 *
 * Dialog to edit an Item.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef ItemDlg_h
#define ItemDlg_h

#include "Item.h"

// ItemDlg dialog

class ItemDlg : public CDialog
{
    DECLARE_DYNAMIC(ItemDlg)

public:
    ItemDlg(CWnd* pParent = NULL);   // standard constructor
    virtual ~ItemDlg();

// Dialog Data
    enum { IDD = IDD_ITEMDLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

    DECLARE_MESSAGE_MAP()

public:
    void CopyFrom(prodauto::Item * item);
    void CopyTo(prodauto::Item * item);

public:
    CString mDescription;
    CString mSeqNumbers;
};

#endif //#ifndef ItemDlg_h

