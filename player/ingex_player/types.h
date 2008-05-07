#ifndef __TYPES_H__
#define __TYPES_H__



typedef enum
{
    WHITE_COLOUR = 0,
    LIGHT_WHITE_COLOUR,
    LIGHT_GREY_COLOUR,
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
    unsigned char Y;
    unsigned char U;
    unsigned char V;
} YUVColours;

static const YUVColours g_rec601YUVColours[] = 
{
    {235, 128, 128},    /* WHITE_COLOUR */
    {210, 128, 128},    /* LIGHT_WHITE_COLOUR */
    {190, 128, 128},    /* LIGHT_GREY_COLOUR */
    {210, 16, 146},     /* YELLOW_COLOUR */
    {169, 166, 16},     /* CYAN_COLOUR */
    {144, 53, 34},      /* GREEN_COLOUR */
    {106, 200, 221},    /* MAGENTA_COLOUR */
    {81, 90, 240},      /* RED_COLOUR */
    {40, 240, 110},     /* BLUE_COLOUR */
    {16, 128, 128},     /* BLACK_COLOUR */
    {165, 43, 180},     /* ORANGE_COLOUR */
};

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


