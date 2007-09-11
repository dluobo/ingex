#ifndef __TYPES_H__
#define __TYPES_H__



typedef enum
{
    WHITE_COLOUR = 0,
    YELLOW_COLOUR,
    CYAN_COLOUR,
    GREEN_COLOUR,
    MAGENTA_COLOUR,
    RED_COLOUR,
    BLUE_COLOUR,
    BLACK_COLOUR,
    ORANGE_COLOUR
} Colour;

typedef struct
{
    int num;
    int den;
} Rational;

static const Rational g_palFrameRate = {25, 1};
static const Rational g_profAudioSamplingRate = {48000, 1};

typedef struct
{
    int isDropFrame;
    unsigned int hour;
    unsigned int min;
    unsigned int sec;
    unsigned int frame;
} Timecode;

static const Timecode g_invalidTimecode = {0, 99, 99, 99, 99};


typedef struct
{
    int type;
    char name[32];
    Colour colour;
} MarkConfig;

typedef struct
{
    MarkConfig configs[32];
    int numConfigs;
} MarkConfigs;



#endif


