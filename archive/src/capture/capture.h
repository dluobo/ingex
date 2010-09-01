/*
 * $Id: capture.h,v 1.2 2010/09/01 16:05:22 philipn Exp $
 *
 * Read frames from the DVS SDI capture card, write to an MXF file, generate a 
 * browse copy file and perform PSE analysis  
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

#ifndef Capture_h
#define Capture_h

#include <inttypes.h>
#include "dvs_clib.h"
#include "dvs_fifo.h"

#include "pse.h"
#include "browse_encoder.h"
#include "DigiBetaDropoutDetector.h"
#include "LTCDecoder.h"
#include "FrameWriter.h"
#include "MXFWriter.h"
#include "DatabaseThings.h"
#include "Threads.h"

#include <string>
#include <vector>
#include <utility>
using std::string;
using std::vector;
using std::pair;

// maximum number of VBI lines that are read for VITC or LTC
#define MAX_VBI_LINES       32

// maximum number of audio streams for capture
#define MAX_CAPTURE_AUDIO   8


enum RecordState { STARTED, IN_PROGRESS, COMPLETED, FAILED };
typedef struct {
    RecordState record_state;
    rec::Timecode start_vitc;
    rec::Timecode start_ltc;
    rec::Timecode current_vitc;
    rec::Timecode current_ltc;
    int current_framecount;
    int64_t file_size;
    int64_t browse_file_size;
    rec::UMID materialPackageUID;
    rec::UMID filePackageUID;
    rec::UMID tapePackageUID;
    long digiBetaDropoutCount;
    int dvs_buffers_empty;
    int num_dvs_buffers;
    int capture_buffer_pos;
    int store_buffer_pos;
    int browse_buffer_pos;
    int ring_buffer_size;
} RecordStats;

typedef struct {
    bool video_ok;
    bool audio_ok;
    bool dltc_ok;
    bool vitc_ok;
    bool recording;
} GeneralStats;

typedef enum {
    PAL,            // picture 720x576 at 1/25, using card's h/w vitc reader
    PALFF,          // picture 720x592 at 1/25, using s/w VITC+LTC VBI reader
    PAL_10BIT,      // picture 720x576 at 1/25, 10-bit, using card's h/w vitc reader
    PALFF_10BIT,    // picture 720x592 at 1/25, 10-bit, using s/w VITC+LTC VBI reader
    NTSC,           // picture 720x486 at 29.97hz, using card's h/w/ vitc
    HD1080i50       // picture 1920x1080 at 25.00hz interlaced, SDI timecode
} VideoMode;

typedef struct
{
    uint8_t* video;
    int16_t* audio;
    rec::Timecode ltc;
    rec::Timecode vitc;
    bool readyToRead;
    bool isEndOfSequence;
    bool isBlackFrame;
}  BrowseFrame;

typedef enum {
    NO_LTC_SOURCE,
    DIGITAL_LTC_SOURCE,
    ANALOGUE_LTC_SOURCE,
    VBI_LTC_SOURCE,
    AUDIO_TRACK_LTC_SOURCE,
} LTCSource;

typedef enum {
    NO_VITC_SOURCE,
    ANALOGUE_VITC_SOURCE,
    VBI_VITC_SOURCE
} VITCSource;

class CaptureListener
{
public:
    virtual ~CaptureListener() {}
    
    virtual void sdiStatusChanged(bool videoOk, bool audioOk, bool dltcOk, bool vitcOK, bool recordingOk) = 0;
    virtual void sdiCaptureDroppedFrame() = 0;
    virtual void mxfBufferOverflow() = 0;
    virtual void browseBufferOverflow() = 0;
};

class Capture
{
public:
    // Constructor is passed an optional Listener which is called
    // whenever a change to the input video/audio status occurs
    Capture(CaptureListener* listener = NULL);

    ~Capture();

    // Create a thread to capture video+audio from hardware to ring buffer.
    // Returns fail if hardware not found or not enough memory.
    bool init_capture(int ringBufferSize);

    bool start_record(  const char *filename,
                        const char *browseFilename, 
                        const char *browseTimecodeFilename, 
                        const char *pseReportFilename,
                        const char *eventFilename,
                        const rec::Rational *aspectRatio);
    bool stop_record(   int duration,           // -1 means "stop now"
                        InfaxData* infaxData,
                        VTRError *vtrErrors, long numErrors,
                        int* pseResult, // set to 0 if passed, 1 if failed, -1 if pse analysis failed
                        long* digiBetaDropoutCount);

    // multi-item start and stop
    bool start_multi_item_record(const char *filename,
                                 const char *eventFilename,
                                 const rec::Rational *aspectRatio);
    bool stop_multi_item_record(int duration,           // -1 means "stop now"
                                VTRError *vtrErrors, long numErrors,
                                int* pseResult, // set to 0 if passed, 1 if failed, -1 if pse analysis failed
                                long* digiBetaDropoutCount);

    bool abort_record(void);
    
    void get_general_stats(GeneralStats *p);
    void get_record_stats(RecordStats *p);

    void set_loglev(int level);
    int inc_loglev(void);
    int dec_loglev(void);
    int get_loglev(void);
    
    // only call these functions when not recording
    
    void set_mxf_page_size(int64_t size);
    
    void set_num_audio_tracks(int num);
    
    void set_browse_enable(bool enable);
    void set_browse_video_bitrate(int browsekbps);
    void set_browse_thread_count(int count);
    void set_browse_overflow_frames(int browseOverflowFrames);

    void set_pse_enable(bool enable);
    
    void set_digibeta_dropout_enable(bool enable);
    void set_digibeta_dropout_thresholds(int lower_threshold, int upper_threshold, int store_threshold);

    void set_ingest_format(rec::IngestFormat format);
    
    void set_ltc_source(LTCSource source, int audio_track);
    void set_vitc_source(VITCSource source);
    
    void set_palff_mode(bool enable);
    void set_vitc_lines(int* lines, int num_lines);
    void set_ltc_lines(int* lines, int num_lines);
    
    void set_include_crc32(bool enable);
    
    void set_primary_timecode(rec::PrimaryTimecode ptimecode);
    
    void set_debug_avsync(bool enable);
    void set_debug_vitc_failures(bool enable);
    void set_debug_store_thread_buffer(bool enable);

private:
    friend void *capture_video_thread_wrapper(void *p_obj);
    friend void *store_video_thread_wrapper(void *p_obj);
    friend void *store_browse_thread_wrapper(void *p_obj);
    void capture_video_thread(void);
    void store_video_thread(void);
    void store_browse_thread(void);
    
    bool restart_capture(long timeout_ms, int *last_frame_captured);

    void browse_add_frame(uint8_t *video, uint8_t *audio, rec::Timecode ltc, rec::Timecode vitc);
    void browse_add_black_frame(rec::Timecode ltc, rec::Timecode vitc);
    
    friend void *encode_browse_thread_wrapper(void *p_obj);
    void encode_browse_thread(void);
    BrowseFrame* get_browse_frame_write(void);
    BrowseFrame* get_browse_frame_read(void);
    void browse_end_sequence(void);

    int last_captured(void);
    void signal_frame_ready();
    void update_last_captured(int update);
    void wait_for_frame_ready(void);
    int last_written(void);
    void update_last_written(int update);

    uint8_t *rec_ring_frame(int frame);
    uint8_t *rec_ring_8bit_video_frame(int frame);
    int ring_element_size(void);
    int ring_audio_pair_offset(int channelPair);
    int ring_vitc_offset(void);
    int ring_ltc_offset(void);

    bool init_start_input_fifo(void);
    bool init_start_output_fifo(void);
    bool stop_input_fifo(void);
    bool stop_output_fifo(void);
    void wait_for_good_SDI_signal(int required_good_frames);
    void poll_sdi_signal_status(void);
    void set_recording_status(bool recording_ok);
    void set_input_SDI_status(bool video_ok, bool audio_ok, bool dltc_ok, bool dvitc_ok);
    void report_sdi_capture_dropped_frame();
    void report_mxf_buffer_overflow();
    void report_browse_buffer_overflow();
    
    int read_timecode(uint8_t* video_data, int stride, int line, const char* type_str);
    int read_fifo_vitc(sv_fifo_buffer *pbuffer, bool vitc1_valid, bool vitc2_valid);
    int read_fifo_dltc(sv_fifo_buffer *pbuffer);
    int read_fifo_ltc(sv_fifo_buffer *pbuffer);
    bool read_audio_track_ltc(uint8_t *dvs_audio_pair, int channel, int *ltc_as_int, bool *is_prev_frame);

    void update_capture_buffer_pos(int dvs_nbuffers, int dvs_availbuffers, int pos);
    void update_store_buffer_pos(int pos);
    void update_browse_buffer_pos(int pos);

    bool open_mxf_file_writer(bool page_file);
    void close_mxf_file_writer();

private:
    pthread_mutex_t m_last_frame_captured;
    pthread_mutex_t m_last_frame_written;
    pthread_mutex_t m_frame_ready_cond;
    pthread_cond_t frame_ready_cond;
    pthread_mutex_t m_buffer_state;

    uint8_t *rec_ring;
    uint8_t *rec_8bit_video_ring;
    int ring_buffer_size;
    
    rec::IngestFormat ingest_format;
    
    LTCSource ltc_source;
    int ltc_audio_track;
    VITCSource vitc_source;
    
    bool palff_mode;

    int g_last_frame_captured;
    bool g_suspend_capture;

    int g_last_frame_written;
    int g_last_browse_frame_written;
    int g_record_start_frame;
    bool g_record_start;
    int g_record_stop_frame;
    int g_frames_written;
    int g_browse_written;
    bool g_aborted;
    rec::UMID g_materialPackageUID;
    rec::UMID g_filePackageUID;
    rec::UMID g_tapePackageUID;
    int start_vitc, start_ltc, current_vitc, current_ltc;
    int last_output_fifo_dropped;

    char mxf_filename[FILENAME_MAX];
    char browse_filename[FILENAME_MAX];
    char browse_timecode_filename[FILENAME_MAX];
    char event_filename[FILENAME_MAX];
    uint8_t *audiobuf24bit[MAX_CAPTURE_AUDIO];         // temp buffers for audio conversion
    uint8_t *video_420;             // temp buffer for video 422->420 convert
    int16_t* audio_16bitstereo;     // temp buffer for browse audio conversion
    uint8_t* black_browse_video;
    int16_t* silent_browse_audio;
    rec::Mutex mxfout_mutex;
    rec::MXFWriter *mxfout;
    rec::FrameWriter *frame_writer;
    FILE *fp_video;

    sv_handle *sv;
    sv_fifo *pinput;
    sv_fifo *poutput;
    VideoMode video_mode;
    int video_width;
    int video_height;
    int video_size;
    int video_8bit_size;
    int element_size;
    int audio_pair_offset[4];
    bool video_ok;
    bool audio_ok;
    bool dltc_ok;
    bool vitc_ok;
    bool recording_ok;          // current recording (if any) is OK
    CaptureListener* _listener;
    int vitc_lines[MAX_VBI_LINES];
    int num_vitc_lines;
    int ltc_lines[MAX_VBI_LINES];
    int num_ltc_lines;
    int num_audio_tracks;
    int64_t mxf_page_size;

    // PSE analysis
    string pse_report;          // path to pse report file
    PSE_Analyse *pse;           // records all failures to write results at end of capture
    bool pse_enabled;

    // Browse
    bool browse_enabled;
    int browse_kbps;
    int browse_thread_count;
    int browse_overflow_frames;
    BrowseFrame browse_frames[2];
    int last_browse_frame_write;
    int last_browse_frame_read;
    pthread_mutex_t m_browse_frame_ready_cond;
    pthread_cond_t browse_frame_ready_cond;
    bool browse_thread_running;
    rec::Rational aspect_ratio;
    
    // DigiBeta dropout detection
    pthread_mutex_t m_dropout_detector;
    bool digibeta_dropout_enabled;
    int digibeta_dropout_lower_threshold;
    int digibeta_dropout_upper_threshold;
    int digibeta_dropout_store_threshold;
    DigiBetaDropoutDetector *dropout_detector;
    
    // CRC-32
    bool include_crc32;
    
    // audio track LTC decoder
    LTCDecoder ltc_decoder;


    // List of all timecodes written to MXF files
    vector< pair<ArchiveTimecode, ArchiveTimecode> >    timecodes;      // VITC, LTC
    
    // primary timecode
    rec::PrimaryTimecode primary_timecode;

    // buffer state
    int dvs_buffers_empty;
    int num_dvs_buffers;
    int capture_buffer_pos;
    int store_buffer_pos;
    int browse_buffer_pos;
    
    // debug stuff
    void save_video_line(int line_number, uint8_t *);
    int loglev;
    bool debug_clapper_avsync;
    FILE *fp_saved_lines;
    bool debug_vitc_failures;
    bool debug_store_thread_buffer;
};

#endif // Capture_h

