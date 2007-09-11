#ifndef __PRODAUTO_CORBA_INGEX_PLAYER_H__
#define __PRODAUTO_CORBA_INGEX_PLAYER_H__


#include <IngexPlayer.h>


namespace prodauto
{

    
class CorbaIngexPlayerData;

    
typedef enum PlayerOutputType
{
    /* playout to SDI via DVS card */
    DVS_OUTPUT,
    /* playout to X11 window */
    X11_OUTPUT
};


/* IngexPlayer that passes Corba messages to player on a server */
class CorbaIngexPlayer : public IngexPlayer
{
public:
    CorbaIngexPlayer(int argc, char* argv[]);
    
    virtual ~CorbaIngexPlayer();
    

    /* opens the files and start playing. The opened parameter indicates for each file whether
    it was successfully opened or not */
    virtual bool start(std::vector<std::string> mxfFilenames, std::vector<bool>& opened);
    
    /* functions inherited from IngexPlayer */
    virtual bool registerListener(IngexPlayerListener* listener);
    virtual bool unregisterListener(IngexPlayerListener* listener);
    virtual bool stop();
    virtual bool toggleLock();
    virtual bool play();
    virtual bool pause();
    virtual bool togglePlayPause();
    virtual bool seek(int64_t offset, int whence);
    virtual bool playSpeed(int speedFactor);
    virtual bool step(bool forward);
    virtual bool mark();
    virtual bool clearMark();
    virtual bool clearAllMarks();
    virtual bool seekNextMark();
    virtual bool seekPrevMark();
    virtual bool toggleOSD();
    virtual bool nextOSDTimecode();
    virtual bool switchNextVideo();
    virtual bool switchPrevVideo();
    virtual bool switchVideo(int index);
    

private:
    CorbaIngexPlayerData* _data;
};


};



#endif


