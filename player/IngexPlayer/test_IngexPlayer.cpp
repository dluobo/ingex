#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "LocalIngexPlayer.h"


using namespace prodauto;
using namespace std;


#define CHECK(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "TEST ERROR: '%s' failed at line %d\n", #cmd, __LINE__); \
        exit(1); \
    }



class TestIngexPlayerListener : public IngexPlayerListener
{
public:    
    TestIngexPlayerListener(LocalIngexPlayer* player)
        : IngexPlayerListener(player), _player(player) {};
    virtual ~TestIngexPlayerListener() {};

    virtual void frameDisplayedEvent(const FrameInfo* frameInfo)
    {
        printf("Frame %lld displayed\n", frameInfo->position);
    }
    
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo)
    {
        printf("Frame %lld dropped\n", lastFrameInfo->position);
    }
    
    virtual void stateChangeEvent(const MediaPlayerStateEvent* event)
    {
        printf("Player state has changed\n");
        if (event->lockedChanged)
        {
            printf("   Locked/unlocked state = %d\n", event->locked);
        }
        if (event->playChanged)
        {
            printf("   play/pause state = %d\n", event->play);
        }
        if (event->stopChanged)
        {
            printf("   stop state = %d\n", event->stop);
        }
        if (event->speedChanged)
        {
            printf("   speed = %dX\n", event->speed);
        }
    }
    
    virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo)
    {
        printf("End of source reached (%lld)\n", lastReadFrameInfo->position);
    }
    
    virtual void startOfSourceEvent(const FrameInfo* firstReadFrameInfo)
    {
        printf("Start of source reached (%lld)\n", firstReadFrameInfo->position);
    }
    
    virtual void playerCloseRequested()
    {
        printf("Player close requested - exiting\n");
        exit(1);
    }
    
    virtual void playerClosed()
    {
        printf("Player has closed\n");
    }
    
    virtual void keyPressed(int key)
    {
        printf("Key pressed %d\n", key);
        if (key == 'q')
        {
            printf("'q' for quit was pressed\n");
            exit(1);
        }
    }
    
    virtual void keyReleased(int key)
    {
        printf("Key released %d\n", key);
    }

    virtual void progressBarPositionSet(float position)
    {
        printf("Progress bar position set at %f\n", position);
        _player->seek((int64_t)(position * 1000), SEEK_SET, PERCENTAGE_PLAY_UNIT);
    }

    
private:
    LocalIngexPlayer* _player;
};


static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>> <<inputs>>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help               display this usage message\n");
    fprintf(stderr, "Inputs:\n");
    fprintf(stderr, "  -m, --mxf  <file>        MXF file input\n");
//    fprintf(stderr, "  --rawin  <file>          Raw UYVY video file input\n");
    fprintf(stderr, "\n");
}

int main (int argc, const char** argv)
{
    auto_ptr<LocalIngexPlayer> player;
    vector<string> filenames;
    vector<bool> opened;
    int cmdlnIndex = 1;
    auto_ptr<TestIngexPlayerListener> listener;
    bool dvsCardIsAvailable = false;

    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-m") == 0 ||
            strcmp(argv[cmdlnIndex], "--mxf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --mxf\n");
                return 1;
            }
            filenames.push_back(argv[cmdlnIndex + 1]);
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }
    
    if (filenames.size() == 0)
    {
        usage(argv[0]);
        fprintf(stderr, "No inputs\n");
        return 1;
    }

    filenames.push_back("filenotexist.mxf");
    
    
    
    player = auto_ptr<LocalIngexPlayer>(new LocalIngexPlayer(X11_AUTO_OUTPUT));
    listener = auto_ptr<TestIngexPlayerListener>(new TestIngexPlayerListener(player.get())); 
    
    printf("Version %s, build %s\n", player->getVersion().c_str(),
        player->getBuildTimestamp().c_str());
    
    CHECK(player->setX11WindowName("test 0"));
    CHECK(player->start(filenames, opened));
    vector<string>::const_iterator iterFilenames;
    vector<bool>::const_iterator iterOpened;
    for (iterFilenames = filenames.begin(), iterOpened = opened.begin();
        iterFilenames != filenames.end() && iterOpened != opened.end();
        iterFilenames++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s'\n", (*iterFilenames).c_str());
        }
        else
        {
            printf("Failed to open '%s'\n", (*iterFilenames).c_str());
        }
    }
    
    printf("Actual output type = %d\n", player->getActualOutputType());
    sleep(2);

    CHECK(player->start(filenames, opened));
    printf("Actual output type = %d\n", player->getActualOutputType());
    sleep(2);
    
    CHECK(player->start(filenames, opened));
    printf("Actual output type = %d\n", player->getActualOutputType());
    sleep(2);
    
    CHECK(player->close());
    printf("Closed player\n");
    sleep(2);
    
    
    dvsCardIsAvailable = player->dvsCardIsAvailable();
    if (dvsCardIsAvailable)
    {
        printf("DVS card is available\n");
        player->setOutputType(DUAL_DVS_AUTO_OUTPUT, 1.0);
    }
    else
    {
        printf("DVS card is NOT available\n");
    }
    
    CHECK(player->setX11WindowName("test 1"));
    CHECK(player->start(filenames, opened));
    for (iterFilenames = filenames.begin(), iterOpened = opened.begin();
        iterFilenames != filenames.end() && iterOpened != opened.end();
        iterFilenames++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s'\n", (*iterFilenames).c_str());
        }
        else
        {
            printf("Failed to open '%s'\n", (*iterFilenames).c_str());
        }
    }
    
    CHECK(player->seek(0, SEEK_END, FRAME_PLAY_UNIT));

    // seeking beyond the eof
    sleep(1);
    printf("Seeking beyond end...\n");
    CHECK(player->seek(-5, SEEK_END, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->setX11WindowName("A new name"));

    sleep(3);
    printf("\nRestarting with non-existing files...\n\n");
    opened.clear();
    vector<string> badFilenames;
    badFilenames.push_back("filenotexist.mxf");
    CHECK(player->setX11WindowName("test 2"));
    CHECK(player->start(badFilenames, opened) != 1);
    
    
    sleep(2);
    printf("\nRestarting...\n\n");
    opened.clear();
    CHECK(player->setX11WindowName("test 3"));
    CHECK(player->start(filenames, opened));
    for (iterFilenames = filenames.begin(), iterOpened = opened.begin();
        iterFilenames != filenames.end() && iterOpened != opened.end();
        iterFilenames++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s'\n", (*iterFilenames).c_str());
        }
        else
        {
            printf("Failed to open '%s'\n", (*iterFilenames).c_str());
        }
    }
    
    sleep(1);
    CHECK(player->step(1));

    sleep(1);
    CHECK(player->step(0));

    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->seek(-5, SEEK_CUR, FRAME_PLAY_UNIT));
    
    sleep(1);
    CHECK(player->seek(5, SEEK_END, FRAME_PLAY_UNIT));
    CHECK(player->togglePlayPause());

    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    
    sleep(1);
    CHECK(player->togglePlayPause());
    CHECK(player->playSpeed(2));
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->mark(0));
    
    sleep(1);
    CHECK(player->clearMark());
    
    sleep(1);
    CHECK(player->mark(0));
    
    sleep(1);
    CHECK(player->clearAllMarks());

    sleep(1);
    CHECK(player->pause());
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    CHECK(player->markPosition(10, 0));
    CHECK(player->markPosition(20, 1));
    CHECK(player->markPosition(30, 2));
    CHECK(player->markPosition(40, 3));
    
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    
    sleep(1);
    CHECK(player->clearAllMarks());
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    sleep(1);
    CHECK(player->mark(0));
    sleep(1);
    CHECK(player->seek(50, SEEK_SET, FRAME_PLAY_UNIT));
    sleep(1);
    CHECK(player->mark(0));
    
    sleep(1);
    CHECK(player->seekPrevMark());
    
    sleep(1);
    CHECK(player->seekNextMark());
    
    sleep(1);
    CHECK(player->setOSDScreen(OSD_EMPTY_SCREEN));
    
    sleep(1);
    CHECK(player->nextOSDScreen());

    sleep(1);
    CHECK(player->nextOSDScreen());
    
    sleep(1);
    CHECK(player->nextOSDScreen());
    
    sleep(1);
    CHECK(player->setOSDScreen(OSD_PLAY_STATE_SCREEN));
    
    sleep(1);
    CHECK(player->nextOSDTimecode());
    
    sleep(1);
    CHECK(player->nextOSDTimecode());
    
    sleep(1);
    CHECK(player->switchNextVideo());
    
    sleep(1);
    CHECK(player->switchPrevVideo());
    
    sleep(1);
    CHECK(player->switchVideo(1));
    
    sleep(1);
    CHECK(player->switchVideo(0));

    sleep(1);
    CHECK(player->switchVideo(1));

    sleep(1);
    CHECK(player->switchVideo(2));

    sleep(1);
    CHECK(player->switchVideo(3));

    sleep(1);
    printf("set timecode 0\n");
    CHECK(player->setOSDTimecode(0, -1, -1));

    sleep(1);
    printf("set timecode 1\n");
    CHECK(player->setOSDTimecode(1, -1, -1));

    sleep(1);
    printf("set first control timecode 1\n");
    CHECK(player->setOSDTimecode(0, CONTROL_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set second control timecode 1\n");
    CHECK(player->setOSDTimecode(1, CONTROL_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set first source timecode 1\n");
    CHECK(player->setOSDTimecode(0, SOURCE_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set second source timecode 1\n");
    CHECK(player->setOSDTimecode(1, SOURCE_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set first VITC timecode 1\n");
    CHECK(player->setOSDTimecode(0, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE));

    sleep(1);
    printf("set first LTC timecode 1\n");
    CHECK(player->setOSDTimecode(0, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE));

    sleep(1);
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    CHECK(player->play());

    
    sleep(2);
    printf("\nRestarting...\n\n");
    opened.clear();
    CHECK(player->setX11WindowName("test 4"));
    CHECK(player->start(filenames, opened));
    for (iterFilenames = filenames.begin(), iterOpened = opened.begin();
        iterFilenames != filenames.end() && iterOpened != opened.end();
        iterFilenames++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s'\n", (*iterFilenames).c_str());
        }
        else
        {
            printf("Failed to open '%s'\n", (*iterFilenames).c_str());
        }
    }
    sleep(4);
    
    sleep(1);
    CHECK(player->play());

    sleep(1);
    CHECK(player->pause());

    
    sleep(1);
    printf("\nSwitching output type and restarting...\n\n");
    if (dvsCardIsAvailable)
    {
        player->setOutputType(DUAL_DVS_X11_OUTPUT, 1.0);
    }
    else
    {
        player->setOutputType(X11_AUTO_OUTPUT, 1.0);
    }

    opened.clear();
    CHECK(player->setX11WindowName("test 5"));
    CHECK(player->start(filenames, opened));
    for (iterFilenames = filenames.begin(), iterOpened = opened.begin();
        iterFilenames != filenames.end() && iterOpened != opened.end();
        iterFilenames++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s'\n", (*iterFilenames).c_str());
        }
        else
        {
            printf("Failed to open '%s'\n", (*iterFilenames).c_str());
        }
    }
    printf("Actual output type is %d\n", player->getActualOutputType());
    
    CHECK(player->play());
    sleep(2);
    
    CHECK(player->pause());
    sleep(1);
    
    
    sleep(2);
    printf("\nResetting and restarting (scale == 1.5)...\n\n");
    player->reset();
    sleep(4);

    if (dvsCardIsAvailable)
    {
        player->setOutputType(DUAL_DVS_X11_XV_OUTPUT, 1.5);
    }
    else
    {
        player->setOutputType(X11_XV_OUTPUT, 1.5);
    }
    
    opened.clear();
    CHECK(player->setX11WindowName("test 6"));
    CHECK(player->start(filenames, opened));
    for (iterFilenames = filenames.begin(), iterOpened = opened.begin();
        iterFilenames != filenames.end() && iterOpened != opened.end();
        iterFilenames++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s'\n", (*iterFilenames).c_str());
        }
        else
        {
            printf("Failed to open '%s'\n", (*iterFilenames).c_str());
        }
    }
    
    sleep(1);
    CHECK(player->play());
    
    sleep(1);
    CHECK(player->pause());
    
    sleep(1);
    CHECK(player->reviewStart(50));
    
    sleep(1);
    CHECK(player->reviewEnd(50));
    
    sleep(1);
    CHECK(player->seek(100, SEEK_SET, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->review(50));
    
    sleep(1);
    
    // sleep(1);
    // CHECK(player->stop());
    
    return 0;   
}
