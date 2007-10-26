#ifndef __YUVLIB_TEXT_OVERLAY__
#define __YUVLIB_TEXT_OVERLAY__

#ifdef __cplusplus
extern "C" {
#endif

//#include <fontconfig.h>


#define MAX_CHARS_IN_SET        32

typedef struct
{
    int		w;		// width in Y samples
    int		h;		// height in picture lines
    int		ssx, ssy;	// horiz & vert UV subsampling ratios
    BYTE*	buff;		// image buffer
    BYTE*	Cbuff;		// UV image buffer
} overlay;

typedef struct
{
    int		width, height;	// dimensions of complete timecode
    overlay	tc_ovly[11];	// character overlay
} timecode_data;

typedef struct
{
    char chars[MAX_CHARS_IN_SET];
    overlay cs_ovly[MAX_CHARS_IN_SET];
    int numChars;
} char_set_data;

typedef void* p_info_rec;

void free_info_rec(p_info_rec* p_info);

void free_overlay(overlay* ovly);

// Create a text overlay. If the width exceeds max_width the text will
// be wrapped (at a word break) to fit.
YUV_error text_to_overlay(p_info_rec* info, overlay* ovly, char* text,
                          int max_width, int min_width,
                          int x_margin, int y_margin,
                          int center,
                          int tab_width,
                          int enable_align_right,
                          char* font, const int size,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den);

// Create a multi line text overlay. Line breaks occur either when the max
// width is reached, or when a \n character occurs.
YUV_error ml_text_to_ovly(p_info_rec* info, overlay* ovly, char* text,
                          int max_width, char* font, const int size, int margin,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den);

// Create a 2x2 grid with a short text string in each space.
// Not very useful except in the "quad split" program.
YUV_error text_to_4box(p_info_rec* info, overlay* ovly,
                       char* txt_0, char* txt_1, char* txt_2, char* txt_3,
                       int max_width, char* font, const int size,
                       const int aspect_ratio_num, const int aspect_ratio_den);

// Superimpose a text overlay on a video frame at position (x, y).
// The text colour is set by txtY (16..235), txtU & txtV (128-112..128+112).
// Use the YUV_601 or YUV_709 routine to convert R,G,B values to this range.
// The text is set in a rectangle of strength "box" (0..100)
YUV_error add_overlay(overlay* ovly, YUV_frame* frame, int x, int y,
                      BYTE txtY, BYTE txtU, BYTE txtV, int box);

void free_timecode(timecode_data* tc_data);

// Render a set of characters to use when overlaying a timecode.
YUV_error init_timecode(p_info_rec* info, timecode_data* tc_data,
                        char* font, const int size,
                        const int aspect_ratio_num, const int aspect_ratio_den);

// Superimpose a timecode on a video frame at position (x, y).
// The timecode is computed from the frame number, assuming 25 frames/second.
// See also the notes on add_overlay above.
YUV_error add_timecode(timecode_data* tc_data, const int frameNo,
                       YUV_frame* frame, int x, int y,
                       BYTE txtY, BYTE txtU, BYTE txtV, int box);

                       
YUV_error char_to_overlay(p_info_rec* info, overlay* ovly, char character,
                          char* font, const int size,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den);

void free_char_set(char_set_data* cs_data);

YUV_error char_set_to_overlay(p_info_rec* info, char_set_data* cs_data, 
                          char* cset, char* font, const int size,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den);

                       
#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_TEXT_OVERLAY__