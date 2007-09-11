#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "CorbaIngexPlayer.h"


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
    TestIngexPlayerListener() {};
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
        if (event->speedFactorChanged)
        {
            printf("   speedFactor = %dX\n", event->speedFactor);
        }
        if (event->lastMarkSetChanged)
        {
            printf("   lastMarkSet = %lld\n", event->lastMarkSet);
        }
        if (event->lastMarkRemovedChanged)
        {
            printf("   lastMarkRemoved = %lld\n", event->lastMarkRemoved);
        }
        if (event->allMarksCleared)
        {
            printf("   allMarksCleared\n");
        }
    }
    
    virtual void playerClose()
    {
        printf("Player has closed\n");
    }
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
    auto_ptr<CorbaIngexPlayer> player;
    vector<string> filenames;
    vector<bool> opened;
    int cmdlnIndex = 1;
    TestIngexPlayerListener listener;

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
    
    
    player = auto_ptr<CorbaIngexPlayer>(new CorbaIngexPlayer(0, NULL));
    
    CHECK(player->registerListener(&listener));
    
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
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->step(1));

    sleep(1);
    CHECK(player->step(0));
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->seek(-5, SEEK_CUR));
    
    sleep(1);
    CHECK(player->seek(0, SEEK_END));
    
    sleep(1);
    CHECK(player->seek(0, SEEK_SET));
    
    sleep(1);
    CHECK(player->togglePlayPause());
    CHECK(player->playSpeed(2));
    
    sleep(1);
    CHECK(player->togglePlayPause());
    
    sleep(1);
    CHECK(player->mark());
    
    sleep(1);
    CHECK(player->clearMark());
    
    sleep(1);
    CHECK(player->mark());
    
    sleep(1);
    CHECK(player->clearAllMarks());
    
    sleep(1);
    CHECK(player->seek(0, SEEK_SET));
    sleep(1);
    CHECK(player->mark());
    sleep(1);
    CHECK(player->seek(50, SEEK_SET));
    sleep(1);
    CHECK(player->mark());
    
    sleep(1);
    CHECK(player->seekPrevMark());
    
    sleep(1);
    CHECK(player->seekNextMark());
    
    sleep(1);
    CHECK(player->toggleOSD());

    sleep(1);
    CHECK(player->toggleOSD());
    
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
    
    sleep(2);
    CHECK(player->stop());
    
    return 0;   
}

