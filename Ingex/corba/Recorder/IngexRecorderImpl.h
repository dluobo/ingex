/*
 * $Id: IngexRecorderImpl.h,v 1.2 2007/01/30 12:25:17 john_f Exp $
 *
 * Servant class for Recorder.
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

#ifndef IngexRecorderImpl_h
#define IngexRecorderImpl_h

#include <vector>
#include <string>

#include "RecorderS.h"

#include "DataSourceImpl.h"
#include "IngexRecorder.h"
#include "CopyManager.h"


class  IngexRecorderImpl
  : public DataSourceImpl, public virtual POA_ProdAuto::Recorder
{
public:
  // Singelton accessor
	static IngexRecorderImpl * Instance() { return mInstance; }
  // Constructor 
  IngexRecorderImpl (void);
  
  // Destructor 
  virtual ~IngexRecorderImpl (void);

  // Servant destroy
  void Destroy();

  // Initialisation
  bool Init(std::string name, std::string db_user, std::string db_pw);

  // Identity
  const char * Name() const { return mName.c_str(); }

// IDL operations
  virtual
  ::ProdAuto::MxfDuration MaxPreRoll (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::MxfDuration MaxPostRoll (
      
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
  ::ProdAuto::Rational EditRate (
      
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
  
  virtual
  ::ProdAuto::Recorder::ReturnCode Start (
      ::ProdAuto::MxfTimecode & start_timecode,
      const ::ProdAuto::MxfDuration & pre_roll,
      const ::ProdAuto::BooleanList & rec_enable,
      const char * tag
   )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  ::ProdAuto::Recorder::ReturnCode Stop (
      ::ProdAuto::MxfTimecode & stop_timecode,
      const ::ProdAuto::MxfDuration & post_roll,
      ::ProdAuto::StringList_out files
    )
    throw (
      ::CORBA::SystemException
    );
  
  virtual
  void UpdateConfig (
      
    )
    throw (
      ::CORBA::SystemException
    );
  
	void NotifyCompletion(IngexRecorder * rec);

private:
// methods
	void DoStop(framecount_t timecode, framecount_t post_roll);
	void ReadTrackConfig(unsigned int n_tracks);
	void UpdateSources();
// data
	ProdAuto::TrackList_var mTracks;
	ProdAuto::TrackStatusList_var mTracksStatus;
	ProdAuto::MxfDuration mMaxPreRoll;
	ProdAuto::MxfDuration mMaxPostRoll;
	std::string mName;
	unsigned int mTrackCount;
	bool mRecording;
	IngexRecorder * mpIngexRecorder;
    CopyManager mCopyManager;

// static instance pointer
	static IngexRecorderImpl * mInstance;
};


#endif //#ifndef IngexRecorderImpl_h
