/*
 * $Id: HTTPTapeExport.h,v 1.2 2010/09/01 16:05:23 philipn Exp $
 *
 * HTTP interface to the tape export
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef __RECORDER_HTTP_TAPE_EXPORT_H__
#define __RECORDER_HTTP_TAPE_EXPORT_H__


#include "TapeExport.h"
#include "HTTPServer.h"
#include "BarcodeScanner.h"
#include "Threads.h"
#include "Timing.h"


namespace rec
{


class HTTPTapeExport : public BarcodeScannerListener, public HTTPConnectionHandler, public HTTPSSIHandler
{
public:
    HTTPTapeExport(HTTPServer* server, TapeExport* tapeExport, BarcodeScanner* scanner);
    virtual ~HTTPTapeExport();

    // from BarcodeScannerListener    
    virtual void newBarcode(std::string barcode);
    
    // from HTTPConnectionHandler
    virtual void processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection);

    // from HTTPSSIHandler
    virtual void processSSIRequest(std::string name, HTTPConnection* connection);
    
    
    void getTapeExportStatus(HTTPConnection* connection);
    
    void checkBarcodeSelection(HTTPConnection* connection);

    void getCacheContents(HTTPConnection* connection);
    void deleteCacheItems(HTTPConnection* connection);
    
    void startNewAutoSession(HTTPConnection* connection);
    void startNewManualSession(HTTPConnection* connection);
    
    void abortSession(HTTPConnection* connection);
    
    void getLTOContents(HTTPConnection* connection);
    
    
    
    static std::string getPSEReportsURL();
    
private:
    void checkBarcodeStatus();
    bool isLTOBarcode(std::string barcode);

    TapeExport* _tapeExport;
    
    std::string _barcode;
    int _barcodeCount;
    Timer _barcodeExpirationTimer;
    Mutex _barcodeMutex;
};    



};





#endif

