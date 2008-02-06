/*
 * $Id: SimplerouterloggerImpl.h,v 1.2 2008/02/06 16:59:00 john_f Exp $
 *
 * Servant class for RouterRecorder.
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

#ifndef SimplerouterloggerImpl_h
#define SimplerouterloggerImpl_h

#include <vector>
#include <string>

#include "RecorderS.h"

#include "RecorderImpl.h"
#include "TimecodeReader.h"
#include "Observer.h"

class SourceReader;
class Router;
class CutsDatabase;

class  SimplerouterloggerImpl
  : public Observer, public RecorderImpl
{
public:
  // Singelton accessor
	//static SimplerouterloggerImpl * Instance() { return mInstance; }
  // Constructor 
  SimplerouterloggerImpl (void);
  
  // Destructor 
  virtual ~SimplerouterloggerImpl (void);


  // Initialisation
  bool Init(const std::string & rport, const std::string & tcport, unsigned int dest, const std::string & db_file);

  void Destination(unsigned int dest) { mDestination = dest; }


  void SetTimecodePort(std::string tcp);

  void SetRouterPort(std::string rp);

  void StartSavingFile(const std::string & filename);

  void StopSavingFile();

  void Name(const std::string & name) { mName = name; }

  
  
  virtual
  ::ProdAuto::Recorder::ReturnCode Start (
      ::ProdAuto::MxfTimecode & start_timecode,
      const ::ProdAuto::MxfDuration & pre_roll,
      const ::CORBA::BooleanSeq & rec_enable,
      const char * project,
      const char * description,
      const ::CORBA::StringSeq & tapes,
      ::CORBA::Boolean test_only
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::Recorder::ReturnCode Stop (
      ::ProdAuto::MxfTimecode & stop_timecode,
      const ::ProdAuto::MxfDuration & post_roll,
      ::CORBA::StringSeq_out files
    )
    throw (
      ::CORBA::SystemException
    );

  virtual
  char * RecordingFormat (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::TrackList * Tracks (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::TrackStatusList * TracksStatus (
      
    )
    throw (
      ::CORBA::SystemException
    ); 


    // From Observer base class
    void Observe(std::string msg);


private:
// static instance pointer
	static SimplerouterloggerImpl * mInstance;

    FILE * mpFile;
	std::string mRouterPort;
	std::string mTimecodePort;
    std::string mName;

    Router * mpRouter;
    TimecodeReader * mpTcReader;
	SourceReader * mpSrcReader;
    CutsDatabase * mpCutsDatabase;

    unsigned int mDestination;

    std::string mLastSrc;
    std::string mLastTc;
};


#endif //#ifndef SimplerouterloggerImpl_h
