/*
 * $Id: ItemDlg.cpp,v 1.1 2007/09/11 14:08:20 stuart_hc Exp $
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

// ItemDlg.cpp : implementation file
//

#include "stdafx.h"
#include "GalleryLogger.h"
#include "ItemDlg.h"


// ItemDlg dialog

IMPLEMENT_DYNAMIC(ItemDlg, CDialog)
ItemDlg::ItemDlg(CWnd* pParent /*=NULL*/)
    : CDialog(ItemDlg::IDD, pParent)
    , mDescription(_T(""))
    , mSeqNumbers(_T(""))
{
}

ItemDlg::~ItemDlg()
{
}

void ItemDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_ITEM_DESC, mDescription);
    DDX_Text(pDX, IDC_EDIT_ITEM_SEQ, mSeqNumbers);
}


BEGIN_MESSAGE_MAP(ItemDlg, CDialog)
END_MESSAGE_MAP()


// ItemDlg message handlers


/*
    Copy from/to database Item

    The current approach of conversion to/from a comma-separated
    list means no commas or spaces in the references.
*/

void ItemDlg::CopyFrom(prodauto::Item * item)
{
    mDescription = item->description.c_str();

    std::string seq_numbers;
    for (unsigned int i = 0; i < item->scriptSectionRefs.size(); ++i)
    {
        if (i != 0)
        {
            seq_numbers += ", ";
        }
        seq_numbers += item->scriptSectionRefs[i];
    }
    mSeqNumbers = seq_numbers.c_str();
}

void ItemDlg::CopyTo(prodauto::Item * item)
{
    item->description = (const char *) mDescription;

    char * buf = new char[mSeqNumbers.GetLength() + 1];
    ::strcpy(buf, (const char *) mSeqNumbers);
    const char * delim_set = ", ";
    char * str = buf;
    const char * token;
    item->scriptSectionRefs.clear();
    while (NULL != (token = ::strtok(str, delim_set)))
    {
        str = NULL;  // for subsequent calls to strtok
        item->scriptSectionRefs.push_back(token);
    }
    delete [] buf;
}


