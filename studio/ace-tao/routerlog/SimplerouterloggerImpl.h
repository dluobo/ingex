// SimplerouterloggerImpl.h

// Copyright (c) 2006. British Broadcasting Corporation. All Rights Reserved.

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
  bool Init(const std::string & rport, const std::string & tcport, unsigned int dest);

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
      //const ::ProdAuto::BooleanList & rec_enable,
      const ::CORBA::BooleanSeq & rec_enable,
	  const char * tag
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

    unsigned int mDestination;
};


#endif //#ifndef SimplerouterloggerImpl_h
