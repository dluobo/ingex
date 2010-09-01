/*
 * $Id: capture.cpp,v 1.2 2010/09/01 16:05:22 philipn Exp $
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

/*
    A capture starts when start_record() is called. A capture_thread is running 
    all the time and it starts the DVS card's FIFO buffer and reads the frames 
    after the start_record() call.
    
    A store_video_thread reads the next frame from a ring buffer, which includes
    video, audio and timecode information, and writes it to the MXF file. At the
    same time the video data is analysed for PSE failures.
    
    A browse copy generation thread, store_browse_thread, is started when 
    start_record is called. This generate a MPEG-2 browse copy file and also
    write timecodes for each frame to a text file. It can happen that the browse
    generation cannot keep up, eg. after a couple of seconds of noisy video,
    and if this happens the encoder encodes black frames instead until it has 
    caught up.
    
    A stop_record() call stops the capture and the MXF file writing is completed 
    with the source Infax data, PSE failures and VTR errors. The browse copy 
    generation will normally complete shortly thereafter and the 
    store_browse_thread is stopped.
    
    The start_multi_item_record() and stop_multi_item_record() methods are used 
    for multi-item source tapes where the initial MXF file is only temporary. This 
    MXF file is written in segments to minimise the disk space requirements when 
    chunking the file into the individual MXF files for each item.
    
    A capture can be abort()ed at any time. A capture is also aborted if the SDI 
    signal is disrupted or when the store_video_thread cannot keep up, eg. 
    writing to disk slows or stops when the disk is full.
    
    
    The DVS FIFO is not running all the time because this results in A/V 
    synchronization errors when the SDI signal is disrupted, eg. when setting 
    the source VTR into play mode. The capture thread only starts the FIFO and
    reads frames once the VTR is playing and the SDI signal is stable. Use
    set_debug_avsync() to test A/V sync using a clapperboard sequence.
    
    The original design played out the captured data through the DVS card's 
    SDI output for monitoring purposes. However, this would sometimes result in 
    disruptions in the video signal. Instead the default DVS card loopthrough 
    signal on the SDI output is used for monitoring.
    
    The system has been tested using a Centaurus DVS card and SDK version 
    2.7p57. It has not been tested with later cards, eg. Centaurus II cards or
    later SDK versions which are said not to have the A/V synchronization 
    issues when the SDI signal is disrupted.
*/

#define __STDC_FORMAT_MACROS 1


#include <cstdio>
#include <cstdlib>
#include <malloc.h>
#include <inttypes.h>
#include <cstring>
#include <strings.h>
#include <libgen.h>
#include <cassert>
#include <syscall.h>

#include <signal.h>
#include <unistd.h> 
#include <cerrno>
#include <limits.h>
#include <ctime>
#include <sys/stat.h>

#include <pthread.h>
#include <string>

#include <stdarg.h>
#include <mxf/mxf_logging.h>    // to redirect mxf logs

#include "capture.h"
#include "logF.h"
#include "video_VITC.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "video_conversion_10bits.h"
#include "avsync_analysis.h"

#include "ArchiveMXFFrameWriter.h"
#include "D10MXFFrameWriter.h"
#include "PSEReport.h"
#include "BrowseEncoder.h"
#include "Timing.h"

#include <mxf/mxf_page_file.h>

#ifdef HAVE_PSE_FPA
#include "pse_fpa.h"
#else
#include "pse_simple.h"
#endif


using std::string;
using std::make_pair;

#define VIDEO_FRAME_WIDTH       720
#define VIDEO_FRAME_HEIGHT      576
#define VIDEO_FRAME_SIZE        (VIDEO_FRAME_WIDTH * VIDEO_FRAME_HEIGHT * 2)
#define AUDIO_FRAME_SIZE        1920 * 3        // 1 channel, 48000Hz sample rate at 24bits per sample

// need this amount of good frames before frames are stored in the capture buffer
// WAIT_GOOD_FRAME_COUNT must be >= 2 to ensure the first frame stored has a valid LTC when using AUDIO_TRACK_LTC_SOURCE
#define WAIT_GOOD_FRAME_COUNT               10
// wait this number of good frames after an error in the SDI signal
// WAIT_GOOD_FRAME_AFTER_ERROR_COUNT must be > WAIT_GOOD_FRAME_COUNT
#define WAIT_GOOD_FRAME_AFTER_ERROR_COUNT   32

#define CHK(x) {if (! (x)) { logTF("failed at %s line %d\n", __FILE__, __LINE__); } }


// helper class which remembers to suspend capture if something fails
class CaptureSuspendGuard
{
public:
    CaptureSuspendGuard(bool* suspend)
    : _suspend(suspend), _confirmed(false)
    {
        *_suspend = false;
    }
    
    ~CaptureSuspendGuard()
    {
        if (!_confirmed)
        {
            // suspend capture
            *_suspend = true;
        }
    }
    
    void confirmStatus()
    {
        // capture can remain not suspended
        _confirmed = true;
    }
    
private:
    bool* _suspend;
    bool _confirmed;
};


// helper class to signal when a thread ends
class ThreadRunningGuard
{
public:
    ThreadRunningGuard(bool* isRunning)
    : _isRunning(isRunning)
    {
        *_isRunning = true;
    }
    
    ~ThreadRunningGuard()
    {
        *_isRunning = false;
    }
    
private:
    bool* _isRunning;
};

static void log_thread_id(const char* name)
{
    logTF("%s thread id: %ld\n", name, syscall(__NR_gettid));
}

static void sleep_msec(int64_t msec)
{
    struct timespec rem;
    struct timespec tm = {msec / 1000, (msec % 1000) * 1000000};
    
    int result;
    while (true)
    {
        result = clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, &rem);
        if (result != 0 && errno == EINTR)
        {
            // sleep was interrupted by a signal - sleep for the remaining time
            tm = rem;
            continue;
        }
        
        break;
    }
}

static bool dir_exists(const char *dir)
{
    struct stat buf;
    if (stat(dir, &buf) != 0)
        return false;

    if (S_ISDIR(buf.st_mode))
        return true;
    else
        return false;
}

// Trap compiles which do not use 64bit filesizes
#if (_FILE_OFFSET_BITS < 64)
#error not using _FILE_OFFSET_BITS >= 64
#endif

static uint64_t get_filesize(const char *file)
{
    if (file == 0)
        return 0;
    
    struct stat buf;
    if (stat(file, &buf) != 0)
        return 0;

    return buf.st_size;
}

// Redirect MXF vlog messages to local vlog function
// Enabled by assigning to extern variable mxf_vlog
void redirect_mxf_vlogs(MXFLogLevel level, const char* format, va_list ap)
{
    const char *level_str = "Unknown";
    switch (level)
    {
        case MXF_DLOG:
            level_str = "Debug";
            break;            
        case MXF_ILOG:
            level_str = "Info";
            break;            
        case MXF_WLOG:
            level_str = "Warning";
            break;            
        case MXF_ELOG:
            level_str = "ERROR";
            break;            
    };
    logTF("MXF %s: \n", level_str);
    vlogTF(format, ap);
}

// Redirect MXF log messages to local log function
// Enabled by assigning to extern variable mxf_log
void redirect_mxf_logs(MXFLogLevel level, const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    redirect_mxf_vlogs(level, format, ap);
    va_end(ap);
}

static void dvsaudio32_to_16bitstereo(uint8_t *buf32, int16_t *buf16)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    for (i = 0; i < 1920*4*2; i += 8) {
        *buf16++ = (((uint16_t)buf32[i+3]) << 8) | buf32[i+2];
        *buf16++ = (((uint16_t)buf32[i+7]) << 8) | buf32[i+6];
    }
}

static void int_to_timecode(int tc_as_int, rec::Timecode *p)
{
    if (tc_as_int == -1) {      // invalid timecode, set all values to zero
        p->frame = 0;
        p->hour = 0;
        p->min = 0;
        p->sec = 0;
        return;
    }

    p->frame = tc_as_int % 25;
    p->hour = (int) (tc_as_int / (60 * 60 * 25));
    p->min = (int) ((tc_as_int - (p->hour * 60 * 60 * 25)) / (60 * 25));
    p->sec = (int) ((tc_as_int - (p->hour * 60 * 60 * 25) - (p->min * 60 * 25)) / 25);
}

static void int_to_archive_timecode(int tc_as_int, ArchiveTimecode *p)
{
    if (tc_as_int == -1) {      // invalid timecode, set all values to zero
        p->dropFrame = false;
        p->frame = 0;
        p->hour = 0;
        p->min = 0;
        p->sec = 0;
        return;
    }

    p->dropFrame = false;
    p->frame = tc_as_int % 25;
    p->hour = (int) (tc_as_int / (60 * 60 * 25));
    p->min = (int) ((tc_as_int - (p->hour * 60 * 60 * 25)) / (60 * 25));
    p->sec = (int) ((tc_as_int - (p->hour * 60 * 60 * 25) - (p->min * 60 * 25)) / 25);
}

static void write_browse_timecode(FILE* tcFile, rec::Timecode ctc, rec::Timecode vitc, rec::Timecode ltc)
{
    fprintf(tcFile, "C%02d:%02d:%02d:%02d V%02d:%02d:%02d:%02d L%02d:%02d:%02d:%02d\n", 
        ctc.hour, ctc.min, ctc.sec, ctc.frame,
        vitc.hour, vitc.min, vitc.sec, vitc.frame,
        ltc.hour, ltc.min, ltc.sec, ltc.frame);
}

static const char *videomode2str(VideoMode videomode)
{
    if (videomode == PAL) {
        return "PAL";
    }
    else if (videomode == PALFF) {
        return "PALFF";
    }
    else if (videomode == PAL_10BIT) {
        return "PAL_10BIT";
    }
    else if (videomode == PALFF_10BIT) {
        return "PALFF_10BIT";
    }
    else if (videomode == NTSC) {
        return "NTSC";
    }
    else if (videomode == HD1080i50) {
        return "HD1080i50";
    }
    return "unknown";
}

static bool check_videomode(sv_handle *sv, VideoMode videomode)
{
    sv_info status_info;
    sv_status(sv, &status_info);
    int width = status_info.xsize;
    int height = status_info.ysize;
    int nbit = status_info.nbit;

    if (videomode == PAL) {
        if (width != 720 || height != 576 || nbit != 8)
            return false;
    }
    else if (videomode == PALFF) {
        if (width != 720 || height != 592 || nbit != 8)
            return false;
    }
    else if (videomode == PAL_10BIT) {
        if (width != 720 || height != 576 || nbit != 10)
            return false;
    }
    else if (videomode == PALFF_10BIT) {
        if (width != 720 || height != 592 || nbit != 10)
            return false;
    }
    else if (videomode == NTSC) {
        if (width != 720 || height != 486 || nbit != 8)
            return false;
    }
    else if (videomode == HD1080i50) {
        if (width != 1920 || height != 1080 || nbit != 8)
            return false;
    }
    else {
        logFF("Unknown video mode");
        return false;
    }
    return true;
}

static bool set_videomode(sv_handle *sv, VideoMode videomode)
{
    int mode;
    if (videomode == PAL) {
        mode = SV_MODE_PAL;
        
        mode &= ~SV_MODE_NBIT_MASK;
        mode |= SV_MODE_NBIT_8B;
    }
    else if (videomode == PAL_10BIT) {
        mode = SV_MODE_PAL;
        
        mode &= ~SV_MODE_NBIT_MASK;
        mode |= SV_MODE_NBIT_10B;
    }
    else if (videomode == PALFF) {
        mode = SV_MODE_PALFF;
        
        mode &= ~SV_MODE_NBIT_MASK;
        mode |= SV_MODE_NBIT_8B;
    }
    else if (videomode == PALFF_10BIT) {
        mode = SV_MODE_PALFF;
        
        mode &= ~SV_MODE_NBIT_MASK;
        mode |= SV_MODE_NBIT_10B;
    }
    else if (videomode == NTSC) {
        mode = SV_MODE_NTSC;
        
        mode &= ~SV_MODE_NBIT_MASK;
        mode |= SV_MODE_NBIT_8B;
    }
    else if (videomode == HD1080i50) {
        mode = SV_MODE_SMPTE274_25I;
        
        mode &= ~SV_MODE_NBIT_MASK;
        mode |= SV_MODE_NBIT_8B;
    }
    else {
        logFF("Unknown video mode");
        return false;
    }
    
    mode &= ~SV_MODE_COLOR_MASK;
    mode |=  SV_MODE_COLOR_YUV422;

    mode |=  SV_MODE_STORAGE_FRAME;

    mode &= ~SV_MODE_AUDIO_MASK;
    mode |=  SV_MODE_AUDIO_8CHANNEL;

    mode &= ~SV_MODE_AUDIOBITS_MASK;
    mode |=  SV_MODE_AUDIOBITS_32;

    int res = sv_videomode(sv, mode);
    if (res != SV_OK) {
        logFF("Set video mode to %s: sv_videomode( %d (0x%08x)) (res=%d)\n", videomode2str(videomode), mode, mode, res);
        return false;
    }

    return true;
}

bool open_dvs(bool input_only, sv_handle **sv)
{
    int res;
    char setup[64];
    int opentype;
    
    if (input_only)
        opentype = SV_OPENTYPE_INPUT | SV_OPENTYPE_MASK_MULTIPLE;
    else
        opentype = SV_OPENTYPE_INPUT | SV_OPENTYPE_OUTPUT | SV_OPENTYPE_MASK_MULTIPLE;
    
    sprintf(setup, "PCI,card=0,channel=0");
    res = sv_openex(sv,
                    setup,
                    SV_OPENPROGRAM_APPLICATION,
                    opentype,
                    0,
                    0);
    if (res == SV_ERROR_WRONGMODE) {
        // Multichannel mode not on so doesn't like "channel=0"
        sprintf(setup, "PCI,card=0");
        res = sv_openex(sv, setup,
                        SV_OPENPROGRAM_APPLICATION,
                        opentype,
                        0,
                        0);
    }
    if (res != SV_OK) {
        logTF("%s: %s\n", setup, sv_geterrortext(res));
        return false;
    }
    
    return true;
}

void Capture::set_loglev(int level)
{
    loglev = level;
}

int Capture::inc_loglev(void)
{
    loglev++;
    return loglev;
}

int Capture::dec_loglev(void)
{
    loglev--;
    return loglev;
}

int Capture::get_loglev(void)
{
    return loglev;
}

void Capture::set_mxf_page_size(int64_t size)
{
    mxf_page_size = size;
}

void Capture::set_num_audio_tracks(int num)
{
    if (num < 0)
    {
        logTF("Error: duh, cannot capture negative number of audio streams\n");
        return;
    }
    if (num > MAX_CAPTURE_AUDIO)
    {
        logTF("Error: cannot capture %d audio streams; maximum is %d\n", num, MAX_CAPTURE_AUDIO);
        return;
    }
    
    num_audio_tracks = num;
}

void Capture::set_browse_enable(bool enable)
{
    browse_enabled = enable;
}

void Capture::set_browse_video_bitrate(int browsekbps)
{
    browse_kbps = browsekbps;
}

void Capture::set_browse_thread_count(int count)
{
    browse_thread_count = count;
}

void Capture::set_browse_overflow_frames(int browseOverflowFrames)
{
    browse_overflow_frames = browseOverflowFrames;
}

void Capture::set_pse_enable(bool enable)
{
    pse_enabled = enable;
}

void Capture::set_digibeta_dropout_enable(bool enable)
{
    digibeta_dropout_enabled = enable;
}

void Capture::set_digibeta_dropout_thresholds(int lower_threshold, int upper_threshold, int store_threshold)
{
    digibeta_dropout_lower_threshold = lower_threshold;
    digibeta_dropout_upper_threshold = upper_threshold;
    digibeta_dropout_store_threshold = store_threshold;
}

void Capture::set_ingest_format(rec::IngestFormat format)
{
    ingest_format = format;
}

void Capture::set_ltc_source(LTCSource source, int audio_track)
{
    ltc_source = source;
    ltc_audio_track = audio_track;
}

void Capture::set_vitc_source(VITCSource source)
{
    vitc_source = source;
}

void Capture::set_palff_mode(bool enable)
{
    palff_mode = enable;
}

void Capture::set_vitc_lines(int* lines, int num_lines)
{
    if (num_lines > MAX_VBI_LINES)
    {
        logTF("Error: number of VITC lines %d exceeds maximum %d\n", num_lines, MAX_VBI_LINES);
        return;
    }
    
    int i;
    for (i = 0; i < num_lines; i++)
    {
        if (lines[i] < 15 || lines[i] > 22)
        {
            logTF("Warning: VITC line %d is outside the PAL-FF VBI (15 <= VBI <= 22)\n", lines[i]);
        }
        vitc_lines[i] = lines[i];
    }
    num_vitc_lines = num_lines;
}

void Capture::set_ltc_lines(int* lines, int num_lines)
{
    if (num_lines > MAX_VBI_LINES)
    {
        logTF("Error: number of LTC lines %d exceeds maximum %d\n", num_lines, MAX_VBI_LINES);
        return;
    }
    
    int i;
    for (i = 0; i < num_lines; i++)
    {
        if (lines[i] < 15 || lines[i] > 22)
        {
            logTF("Warning: LTC line %d is outside the PAL-FF VBI (15 <= VBI <= 22)\n", lines[i]);
        }
        ltc_lines[i] = lines[i];
    }
    num_ltc_lines = num_lines;
}

void Capture::set_include_crc32(bool enable)
{
    include_crc32 = enable;
}

void Capture::set_primary_timecode(rec::PrimaryTimecode ptimecode)
{
    primary_timecode = ptimecode;
}

void Capture::set_debug_avsync(bool enable)
{
    debug_clapper_avsync = enable;
}

void Capture::set_debug_vitc_failures(bool enable)
{
    debug_vitc_failures = enable;
}

void Capture::set_debug_store_thread_buffer(bool enable)
{
    debug_store_thread_buffer = enable;
}


uint8_t *Capture::rec_ring_frame(int frame)
{
    return rec_ring + (frame % ring_buffer_size) * element_size;
}

uint8_t *Capture::rec_ring_8bit_video_frame(int frame)
{
    return rec_8bit_video_ring + (frame % ring_buffer_size) * video_8bit_size;
}

int Capture::ring_element_size(void)
{
    return element_size;
}

int Capture::ring_audio_pair_offset(int channelPair)
{
    return audio_pair_offset[channelPair];
}

int Capture::ring_vitc_offset(void)
{
    return element_size - sizeof(int) * 1;
}

int Capture::ring_ltc_offset(void)
{
    return element_size - sizeof(int) * 2;
}

int Capture::last_captured(void)
{
    pthread_mutex_lock(&m_last_frame_captured);
    int last = g_last_frame_captured;
    pthread_mutex_unlock(&m_last_frame_captured);
    return last;
}

void Capture::signal_frame_ready()
{
    pthread_mutex_lock(&m_frame_ready_cond);
    pthread_cond_broadcast(&frame_ready_cond);
    pthread_mutex_unlock(&m_frame_ready_cond);
}

void Capture::update_last_captured(int update)
{
    pthread_mutex_lock(&m_last_frame_captured);
    g_last_frame_captured = update;
    pthread_mutex_unlock(&m_last_frame_captured);

    signal_frame_ready();
}

void Capture::wait_for_frame_ready(void)
{
    // wait a maximum of 2 seconds
    struct timespec endtime;
    clock_gettime(CLOCK_MONOTONIC, &endtime);
    endtime.tv_sec += 2;
    
    pthread_mutex_lock(&m_frame_ready_cond);
    pthread_cond_timedwait(&frame_ready_cond, &m_frame_ready_cond, &endtime);
    pthread_mutex_unlock(&m_frame_ready_cond);
}

int Capture::last_written(void)
{
    pthread_mutex_lock(&m_last_frame_written);
    int last = g_last_frame_written;
    pthread_mutex_unlock(&m_last_frame_written);
    return last;
}
void Capture::update_last_written(int update)
{
    pthread_mutex_lock(&m_last_frame_written);
    g_last_frame_written = update;
    pthread_mutex_unlock(&m_last_frame_written);
}

void Capture::get_general_stats(GeneralStats *p)
{
    p->video_ok = video_ok;
    p->audio_ok = audio_ok;
    p->dltc_ok = dltc_ok;
    p->vitc_ok = vitc_ok;
    p->recording = g_record_start;
}

void Capture::get_record_stats(RecordStats *p)
{
    // STARTED state not set here
    if (g_record_start)
        p->record_state = IN_PROGRESS;
    else if (g_aborted)
        p->record_state = FAILED;
    else
        p->record_state = COMPLETED;

    int_to_timecode(start_vitc, &p->start_vitc);
    int_to_timecode(start_ltc, &p->start_ltc);
    int_to_timecode(current_vitc, &p->current_vitc);
    int_to_timecode(current_ltc, &p->current_ltc);

    p->current_framecount = g_frames_written;
    {
        LOCK_SECTION(mxfout_mutex);
        if (mxfout)
        {
            // use the writer class because multi-item sources will
            // result in mxf page files being used which are split of 1 or more files
            p->file_size = mxfout->getFileSize();
        }
        else
        {
            p->file_size = 0;
        }
    }
    p->browse_file_size = get_filesize(browse_filename);
    p->materialPackageUID = g_materialPackageUID;
    p->filePackageUID = g_filePackageUID;
    p->tapePackageUID = g_tapePackageUID;

    pthread_mutex_lock(&m_dropout_detector);
    p->digiBetaDropoutCount = 0;
    if (dropout_detector)
    {
        p->digiBetaDropoutCount = dropout_detector->getNumDropouts();
    }
    pthread_mutex_unlock(&m_dropout_detector);

    pthread_mutex_lock(&m_buffer_state);
    p->dvs_buffers_empty = dvs_buffers_empty;
    p->num_dvs_buffers = num_dvs_buffers;
    p->capture_buffer_pos = capture_buffer_pos;
    p->store_buffer_pos = store_buffer_pos;
    p->browse_buffer_pos = browse_buffer_pos;
    p->ring_buffer_size = ring_buffer_size;
    pthread_mutex_unlock(&m_buffer_state);
}

void Capture::set_recording_status(bool recording_state)
{
    if (recording_state != recording_ok)
    {
        recording_ok = recording_state;
    
        if (_listener)
            _listener->sdiStatusChanged(video_ok, audio_ok, dltc_ok, vitc_ok, recording_ok);
    }
}

void Capture::set_input_SDI_status(bool video_state, bool audio_state, bool dltc_state, bool vitc_state)
{
    if (video_state != video_ok || audio_state != audio_ok || dltc_state != dltc_ok || vitc_state != vitc_ok)
    {
        video_ok = video_state;
        audio_ok = audio_state;
        dltc_ok = dltc_state;
        vitc_ok = vitc_state;
    
        if (_listener)
            _listener->sdiStatusChanged(video_ok, audio_ok, dltc_ok, vitc_ok, recording_ok);
    }
}

void Capture::report_sdi_capture_dropped_frame()
{
    if (_listener)
        _listener->sdiCaptureDroppedFrame();
}

void Capture::report_mxf_buffer_overflow()
{
    if (_listener)
        _listener->mxfBufferOverflow();
}

void Capture::report_browse_buffer_overflow()
{
    if (_listener)
        _listener->browseBufferOverflow();
}

int Capture::read_timecode(uint8_t* video_data, int stride, int line, const char* type_str)
{
    unsigned hh, mm, ss, ff;
    uint8_t* vbiline;
    int palff_line = line - 15; // PAL_FF video starts at line 15
    
    if (palff_line >= 0)
    {
        vbiline = video_data + stride * palff_line * 2;
        if (readVITC(vbiline, 1, &hh, &mm, &ss, &ff)) {
            if (loglev > 1) logFFi("%s (%d): %02u:%02u:%02u:%02u", type_str, line, hh, mm, ss, ff);
            return tc_to_int(hh, mm, ss, ff);
        }
        else {
            // Log failure only if line is interesting
            if (! black_or_grey_line(vbiline, 720)) {
                if (debug_vitc_failures) save_video_line(palff_line * 2, vbiline);
                if (loglev > 1) logFFi("%s (%d) failed", type_str, line);
            }
        }
    }
    
    return -1;
}

// code copied from ingex/studio/capture/dvs_sdi.c
int Capture::read_fifo_vitc(sv_fifo_buffer *pbuffer, bool vitc1_valid, bool vitc2_valid)
{
    // SMPTE-12M VITC
    int vitc1_tc = pbuffer->timecode.vitc_tc;
    int vitc2_tc = pbuffer->timecode.vitc_tc2;


    int vitc_tc;
    if (vitc1_valid && !vitc2_valid)
    {
        vitc_tc = vitc1_tc;
    }
    else if (vitc2_valid && !vitc1_valid)
    {
        vitc_tc = vitc2_tc;
    }
    else
    {
        // Handle buggy field order (can happen with misconfigured camera)
        // Incorrect field order causes vitc_tc and vitc2 to be swapped.
        // If the high bit is set on vitc use vitc2 instead.
        if ((unsigned)vitc1_tc >= 0x80000000 && (unsigned)vitc2_tc < 0x80000000)
        {
            vitc_tc = vitc2_tc;
            if (loglev > 3) //(verbose)
            {
                logTF("1st vitc value >= 0x80000000 (0x%08x), using 2nd vitc (0x%08x)\n", vitc1_tc, vitc2_tc);
            }
        }
        else
        {
            vitc_tc = vitc1_tc;
        }
    }

    // dvs_tc_to_int will mask off the irrelevant bits in the timecodes.
    return dvs_tc_to_int(vitc_tc);
}

// code copied from ingex/studio/capture/dvs_sdi.c
int Capture::read_fifo_dltc(sv_fifo_buffer *pbuffer)
{
    // "DLTC" timecode from RP188/RP196 ANC data
    int ltc_anc = pbuffer->anctimecode.dltc_tc;


    // Handle buggy field order (can happen with misconfigured camera)
    // Incorrect field order causes vitc_tc and vitc2 to be swapped.
    // If the high bit is set on vitc use vitc2 instead.
    // A similar check must be done for LTC since the field
    // flag is occasionally set when the fifo call returns
    if ((unsigned)ltc_anc >= 0x80000000)
    {
        if (loglev > 3) //(verbose)
        {
            logTF("dltc tc >= 0x80000000 (0x%08x), masking high bit\n", ltc_anc);
        }
        ltc_anc = (unsigned)ltc_anc & 0x7fffffff;
    }

    // dvs_tc_to_int will mask off the irrelevant bits in the timecodes.
    return dvs_tc_to_int(ltc_anc);
}

// code copied from ingex/studio/capture/dvs_sdi.c
int Capture::read_fifo_ltc(sv_fifo_buffer *pbuffer)
{
    // SMPTE-12M LTC
    int ltc_tc = pbuffer->timecode.ltc_tc;


    // Handle buggy field order (can happen with misconfigured camera)
    // Incorrect field order causes vitc_tc and vitc2 to be swapped.
    // If the high bit is set on vitc use vitc2 instead.
    // A similar check must be done for LTC since the field
    // flag is occasionally set when the fifo call returns
    if ((unsigned)ltc_tc >= 0x80000000)
    {
        if (loglev > 3) //(verbose)
        {
            logTF("ltc tc >= 0x80000000 (0x%08x), masking high bit\n", ltc_tc);
        }
        ltc_tc = (unsigned)ltc_tc & 0x7fffffff;
    }

    // dvs_tc_to_int will mask off the irrelevant bits in the timecodes.
    return dvs_tc_to_int(ltc_tc);
}

bool Capture::read_audio_track_ltc(uint8_t *dvs_audio_pair, int channel, int *ltc_as_int, bool *is_prev_frame)
{
    // extract the audio track and convert to 8-bit

    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1
    
    unsigned char buf_8bit[1920];

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    int i;
    for (i = channel_offset; i < 1920*4*2; i += 8) {
        buf_8bit[i/8] = 128 + (char)(dvs_audio_pair[i+3]);
    }


    // process frame of audio data
    
    ltc_decoder.processFrame(buf_8bit, 1920);
    if (!ltc_decoder.havePrevFrameTimecode() && !ltc_decoder.haveThisFrameTimecode())
        return false;
    
    rec::Timecode ltc = ltc_decoder.getTimecode();
    *is_prev_frame = ltc_decoder.havePrevFrameTimecode();
    *ltc_as_int = ltc.hour * 60 * 60 * 25 + ltc.min * 60 * 25 + ltc.sec * 25 + ltc.frame; 
    
    return true;
}

void Capture::update_capture_buffer_pos(int dvs_nbuffers, int dvs_availbuffers, int pos)
{
    pthread_mutex_lock(&m_buffer_state);
    dvs_buffers_empty = (dvs_nbuffers - 1) - dvs_availbuffers;
    num_dvs_buffers = dvs_nbuffers;
    capture_buffer_pos = pos % ring_buffer_size;
    pthread_mutex_unlock(&m_buffer_state);
}

void Capture::update_store_buffer_pos(int pos)
{
    pthread_mutex_lock(&m_buffer_state);
    store_buffer_pos = pos % ring_buffer_size;
    pthread_mutex_unlock(&m_buffer_state);
}

void Capture::update_browse_buffer_pos(int pos)
{
    pthread_mutex_lock(&m_buffer_state);
    browse_buffer_pos = pos % ring_buffer_size;
    pthread_mutex_unlock(&m_buffer_state);
}

extern void *capture_video_thread_wrapper(void *p_obj)
{
    log_thread_id("Capture");
    
    Capture *p = (Capture *)(p_obj);
    p->capture_video_thread();
    
    return NULL;
}

extern void *store_video_thread_wrapper(void *p_obj)
{
    log_thread_id("Store");
    
    Capture *p = (Capture *)(p_obj);
    p->store_video_thread();
    
    return NULL;
}

extern void *store_browse_thread_wrapper(void *p_obj)
{
    log_thread_id("Browse");
    
    Capture *p = (Capture *)(p_obj);
    p->store_browse_thread();
    
    return NULL;
}

extern void *encode_browse_thread_wrapper(void *p_obj)
{
    log_thread_id("Browse encode");
    
    Capture *p = (Capture *)(p_obj);
    p->encode_browse_thread();
    
    return NULL;
}

bool Capture::open_mxf_file_writer(bool page_file)
{
    bool result;
    MXFFile *mxf_file = 0;
    
    if (page_file) {
        MXFPageFile *mxf_page_file;
        if (!mxf_page_file_open_new(mxf_filename, mxf_page_size, &mxf_page_file))
            return false;
        mxf_file = mxf_page_file_get_file(mxf_page_file);
    }

    if (ingest_format == rec::MXF_UNC_8BIT_INGEST_FORMAT ||
        ingest_format == rec::MXF_UNC_10BIT_INGEST_FORMAT)
    {
        rec::ArchiveMXFWriter *mxf_writer = new rec::ArchiveMXFWriter();
        mxf_writer->setComponentDepth(ingest_format == rec::MXF_UNC_8BIT_INGEST_FORMAT ? 8 : 10);
        mxf_writer->setAspectRatio(aspect_ratio);
        mxf_writer->setNumAudioTracks(num_audio_tracks);
        mxf_writer->setIncludeCRC32(include_crc32);
        mxf_writer->setStartPosition(0);
        if (mxf_file)
            result = mxf_writer->createFile(&mxf_file, mxf_filename);
        else
            result = mxf_writer->createFile(mxf_filename);
        if (!result) {
            if (mxf_file)
                mxf_file_close(&mxf_file);
            delete mxf_writer;
            return false;
        }

        frame_writer = new rec::ArchiveMXFFrameWriter(element_size, palff_mode, mxf_writer);

        mxfout = mxf_writer;
    }
    else // ingest_format == rec::MXF_D10_50_INGEST_FORMAT
    {
        rec::D10MXFWriter *mxf_writer = new rec::D10MXFWriter();
        mxf_writer->setAspectRatio(aspect_ratio);
        mxf_writer->setNumAudioTracks(num_audio_tracks);
        mxf_writer->setPrimaryTimecode(primary_timecode);
        mxf_writer->setEventFilename(event_filename);
        if (mxf_file)
            result = mxf_writer->createFile(&mxf_file, mxf_filename);
        else
            result = mxf_writer->createFile(mxf_filename);
        if (!result) {
            if (mxf_file)
                mxf_file_close(&mxf_file);
            delete mxf_writer;
            return false;
        }
        
        frame_writer = new rec::D10MXFFrameWriter(element_size, palff_mode, mxf_writer);

        mxfout = mxf_writer;
    }

    return true;
}

void Capture::close_mxf_file_writer()
{
    delete mxfout;
    mxfout = 0;
    
    delete frame_writer;
    frame_writer = 0;
}

bool Capture::start_record(const char *filename,
                           const char *browseFilename,
                           const char *browseTimecodeFilename,
                           const char *pseReportFilename,
                           const char *eventFilename,
                           const rec::Rational *aspectRatio)
{
    // TODO: code shouldn't rely on all being non-null
    assert(filename && browseFilename && browseTimecodeFilename && pseReportFilename && eventFilename);
    
    logFF("start_record(filename=%s, browseFilename=%s, browseTimecodeFilename=%s, pseReportFilename=%s, eventFilename=%s)\n",
        filename, browseFilename, browseTimecodeFilename, pseReportFilename, eventFilename);
    logFF("browse_enabled=%s; pse_enabled=%s\n", browse_enabled ? "true" : "false", pse_enabled ? "true" : "false"); 

    if (g_record_start) {           // already started recording
        logTF("start_record: FAILED - recording in progress\n");
        return false;
    }

    if (browse_enabled) {
        if (browse_thread_running) {
            logTF("Failed to prepare browse generation whilst existing process is still busy\n");
            return false;
        }
    }
    
    aspect_ratio = *aspectRatio;

    // reset the stop frame after we know the browse thread is not running 
    g_record_stop_frame = -2;
    

    // restart capturing
    CaptureSuspendGuard captureSuspendGuard(&g_suspend_capture);
    int last_frame_captured;
    if (!restart_capture(3000, &last_frame_captured))
    {
        logTF("start_record: failed to start capturing within 3 second of starting\n");
        return false;
    }
    logFF("start_record: last_frame_captured=%d\n", last_frame_captured);
    
    // We don't check for input video status since it is ok to
    // start recording without a good signal, but the recording will
    // fail if we don't get a good signal soon

    // store mxf and browse filenames
    strcpy(mxf_filename, filename);
    strcpy(browse_filename, browseFilename);
    strcpy(browse_timecode_filename, browseTimecodeFilename);
    strcpy(event_filename, eventFilename);

    // make MXF directory if not present
    int res;
    char tmppath[FILENAME_MAX];
    strcpy(tmppath, mxf_filename);
    const char *mxfdir = dirname(tmppath);
    if (! dir_exists(mxfdir)) {
        if ((res = mkdir(mxfdir, 0775)) != 0) {
            logTF("start_record: mkdir(%s) failed, res=%d (%s)\n", mxfdir, res, strerror(errno));
            return false;
        }
    }

    // Open MXF file for writing
    {
        LOCK_SECTION(mxfout_mutex);

        if (!open_mxf_file_writer(false)) {
            logTF("Failed to prepare MXF writer\n");
            return false;
        }

        // get the package UIDs
        g_materialPackageUID = mxfout->getMaterialPackageUID();
        g_filePackageUID = mxfout->getFileSourcePackageUID();
        g_tapePackageUID = mxfout->getTapeSourcePackageUID();
    }


    // Crash record: start from last captured frame
    int first_frame_to_write;
    first_frame_to_write = last_frame_captured;
    update_last_written(first_frame_to_write - 1);
    logFF("start_record: first_frame_to_write=%d\n", first_frame_to_write);

    // Setup new browse encoding
    if (browse_enabled) {

        // Indicate first browse frame to be written (last + 1 = first -> last = first - 1)
        g_last_browse_frame_written = first_frame_to_write - 1;

        // initializations
        int i;
        for (i = 0; i < 2; i++)
        {
            browse_frames[i].readyToRead = false;
            browse_frames[i].isEndOfSequence = false;
            browse_frames[i].isBlackFrame = false;
        }
        last_browse_frame_write = 0;
        last_browse_frame_read = 0;
        

        // create browse store thread and browse encode thread
        
        pthread_t browse_thread;
        if ((res = pthread_create(&browse_thread, NULL, store_browse_thread_wrapper, this)) != 0) {
            logTF("Failed to create browse thread: %s\n", strerror(res));
            return false;
        }

        pthread_t encode_browse_thread;
        if ((res = pthread_create(&encode_browse_thread, NULL, encode_browse_thread_wrapper, this)) != 0) {
            logTF("Failed to create encode browse thread: %s\n", strerror(res));
            return false;
        }
    }
    
    // record start vitc and ltc (for record stats)
    uint8_t *addr = rec_ring_frame(first_frame_to_write);
    memcpy(&start_ltc, addr + ring_ltc_offset(), sizeof(start_ltc));
    memcpy(&start_vitc, addr + ring_vitc_offset(), sizeof(start_vitc));
    logFF("start_ltc=%d start_vitc=%d\n", start_ltc, start_vitc);

    set_recording_status(true);
    g_frames_written = 0;
    g_browse_written = 0;
    g_aborted = false;

    // Open a new PSE analysis
    if (pse_enabled)
    {
        if (pse->is_init())
        {
            pse_report = pseReportFilename;
            if (!pse->open())
            {
                logTF("Failed to open PSE engine\n");
            }
        }
        else
        {
            logTF("Not performing PSE analysis because engine initialization failed\n");
        }
    }

    // Setup digibeta dropout detection
    if (digibeta_dropout_enabled) {
        pthread_mutex_lock(&m_dropout_detector);
        dropout_detector = new DigiBetaDropoutDetector(video_width, video_height,
            digibeta_dropout_lower_threshold, digibeta_dropout_upper_threshold, digibeta_dropout_store_threshold);
        pthread_mutex_unlock(&m_dropout_detector);
    }

    // Clear timecodes list
    timecodes.clear();

    // signal start recording
    g_record_start = true;
    
    captureSuspendGuard.confirmStatus(); // everything went ok, so let capture continue

    return true;
}

bool Capture::stop_record(int duration, InfaxData* infaxData, VTRError *vtrErrors, long numVTRErrors,
    int* pseResult, long* digiBetaDropoutCount)
{
    *pseResult = -1; // only set to 0 or 1 later on if the analysis was completed
    
    if (! g_record_start) {
        logFF("stop_record: FAILED - recording not in progress\n");
        return false;
    }

    // duration of -1 means stop now (this instant)
    if (duration == -1) {
        g_record_stop_frame = last_captured();
    }
    else {
        g_record_stop_frame = g_record_start_frame + duration - 1;
    }

    logFF("stop_record: g_record_stop_frame = %d (duration = %d)\n", g_record_stop_frame, duration);

    // wait until recording stopped before returning
    while (g_record_start) {
        // wake up the store threads
        signal_frame_ready();
        
        sleep_msec(20);
    }

    // suspend the capture thread
    g_suspend_capture = true;

    // Get all PSE results
    long numPSEFailues = 0;
    PSEFailure *pseFailures = 0;
    bool pse_was_open = pse->is_open();
    if (pse_enabled && pse_was_open)
    {
        std::vector<PSEResult> results;
        pse->get_remaining_results(results);
        pse->close();
    
        // Convert PSE results to C array for MXF call
        numPSEFailues = results.size();
        logFF("PSE failures = %ld\n", numPSEFailues);
        pseFailures = new PSEFailure[numPSEFailues];
        for (long i = 0; i < numPSEFailues; i++) {
            pseFailures[i].position = results[i].position;
            pseFailures[i].vitcTimecode = timecodes[pseFailures[i].position].first;
            pseFailures[i].ltcTimecode = timecodes[pseFailures[i].position].second;
            pseFailures[i].redFlash = results[i].red;
            pseFailures[i].luminanceFlash = results[i].flash;
            pseFailures[i].spatialPattern = results[i].spatial;
            pseFailures[i].extendedFailure = results[i].extended;
            logFF("    %4ld: pos=%6"PRIu64" VITC=%02d:%02d:%02d:%02d LTC=%02d:%02d:%02d:%02d red=%4d fls=%4d spt=%4d ext=%d\n", i, pseFailures[i].position,
                    pseFailures[i].vitcTimecode.hour, pseFailures[i].vitcTimecode.min,
                    pseFailures[i].vitcTimecode.sec, pseFailures[i].vitcTimecode.frame,
                    pseFailures[i].ltcTimecode.hour, pseFailures[i].ltcTimecode.min,
                    pseFailures[i].ltcTimecode.sec, pseFailures[i].ltcTimecode.frame,
                    pseFailures[i].redFlash, pseFailures[i].luminanceFlash, pseFailures[i].spatialPattern, pseFailures[i].extendedFailure);
        }
    }
    
    // get digibeta dropouts
    long numDigiBetaDropouts = 0;
    DigiBetaDropout *digiBetaDropouts = 0;
    if (dropout_detector && dropout_detector->getNumDropouts() > 0) {
        digiBetaDropouts = &dropout_detector->getDropouts()[0];
        numDigiBetaDropouts = (long)dropout_detector->getNumDropouts();
    }


    // Complete MXF file
    {
        LOCK_SECTION(mxfout_mutex);
        
        int res = mxfout->complete(infaxData,
            pseFailures, numPSEFailues,
            vtrErrors, numVTRErrors,
            digiBetaDropouts, numDigiBetaDropouts);
        if (!res) {
            logFF("Failed to complete writing Archive MXF file\n");
        }

        close_mxf_file_writer();
    }


    
    // Write PSE resport
    if (pse_enabled && pse_was_open)
    {
        PSEReport* pseReport = PSEReport::open(pse_report);
        if (pseReport == 0)
        {
            logFF("Failed to open %s - no PSE report written\n", pse_report.c_str());
        }
        else
        {
            bool passed;
            bool result = pseReport->write(0, g_frames_written - 1, "", infaxData, pseFailures, numPSEFailues, &passed);
            if (result) 
            {
                logFF("pseReport->write() succeeded. file=%s\n", pse_report.c_str());
                *pseResult = passed ? 0 : 1;
            }
            else 
            {
                logFF("pseReport->write() failed. file=%s\n", pse_report.c_str());
            }

            delete pseReport;
        }

        delete [] pseFailures;
    }
    
    *digiBetaDropoutCount = 0;
    if (dropout_detector)
    {
        *digiBetaDropoutCount = dropout_detector->getNumDropouts();
        
        pthread_mutex_lock(&m_dropout_detector);
        delete dropout_detector;
        dropout_detector = 0;
        pthread_mutex_unlock(&m_dropout_detector);
    }


    logFF("stop_record: finished, g_frames_written=%d\n", g_frames_written);
    return true;
}

bool Capture::start_multi_item_record(const char *filename,
                                      const char *eventFilename,
                                      const rec::Rational *aspectRatio)
{
    // TODO: code shouldn't rely on all being non-null
    assert(filename && eventFilename);
    
    if (strstr(filename, "%d") == 0)
    {
        logTF("start_multi_item_record: invalid page filename '%s'\n", filename);
        return false;
    }
    
    logFF("start_multi_item_record(filename=%s, eventFilename=%s)\n", filename, eventFilename);

    if (g_record_start) {           // already started recording
        logTF("start_multi_item_record: FAILED - recording in progress\n");
        return false;
    }
    
    aspect_ratio = *aspectRatio;

    // reset the stop frame after we know the browse thread is not running 
    g_record_stop_frame = -2;
    
    // restart capturing
    CaptureSuspendGuard captureSuspendGuard(&g_suspend_capture);
    int last_frame_captured;
    if (!restart_capture(3000, &last_frame_captured))
    {
        logTF("start_record: failed to start capturing within 3 second of starting\n");
        return false;
    }
    logFF("start_multi_item_record: last_frame_captured=%d\n", last_frame_captured);
    

    // We don't check for input video status since it is ok to
    // start recording without a good signal, but the recording will
    // fail if we don't get a good signal soon

    // store mxf and browse filenames
    strcpy(mxf_filename, filename);
    strcpy(browse_filename, "");
    strcpy(browse_timecode_filename, "");
    strcpy(event_filename, eventFilename);

    // make MXF directory if not present
    int res;
    char tmppath[FILENAME_MAX];
    strcpy(tmppath, mxf_filename);
    const char *mxfdir = dirname(tmppath);
    if (! dir_exists(mxfdir)) {
        if ((res = mkdir(mxfdir, 0775)) != 0) {
            logTF("start_multi_item_record: mkdir(%s) failed, res=%d (%s)\n", mxfdir, res, strerror(errno));
            return false;
        }
    }

    // Open MXF file for writing
    {
        LOCK_SECTION(mxfout_mutex);
        
        if (!open_mxf_file_writer(true)) {
            logTF("start_multi_item_record: Failed to prepare MXF writer\n");
            return false;
        }
    
        // get the package UIDs
        g_materialPackageUID = mxfout->getMaterialPackageUID();
        g_filePackageUID = mxfout->getFileSourcePackageUID();
        g_tapePackageUID = mxfout->getTapeSourcePackageUID();
    }

    // Crash record: start from last captured frame
    int first_frame_to_write;
    first_frame_to_write = last_frame_captured;
    update_last_written(first_frame_to_write - 1);
    logFF("start_multi_item_record: first_frame_to_write=%d\n", first_frame_to_write);

    // record start vitc and ltc (for record stats)
    uint8_t *addr = rec_ring_frame(first_frame_to_write);
    memcpy(&start_ltc, addr + ring_ltc_offset(), sizeof(start_ltc));
    memcpy(&start_vitc, addr + ring_vitc_offset(), sizeof(start_vitc));
    logFF("start_ltc=%d start_vitc=%d\n", start_ltc, start_vitc);

    set_recording_status(true);
    g_frames_written = 0;
    g_browse_written = 0;
    g_aborted = false;

    // Open a new PSE analysis
    if (pse_enabled)
    {
        if (pse->is_init())
        {
            pse_report = "";
            if (!pse->open())
            {
                logTF("Failed to open PSE engine\n");
            }
        }
        else
        {
            logTF("Not performing PSE analysis because engine initialization failed\n");
        }
    }

    // Setup digibeta dropout detection
    if (digibeta_dropout_enabled) {
        pthread_mutex_lock(&m_dropout_detector);
        dropout_detector = new DigiBetaDropoutDetector(video_width, video_height,
            digibeta_dropout_lower_threshold, digibeta_dropout_upper_threshold, digibeta_dropout_store_threshold);
        pthread_mutex_unlock(&m_dropout_detector);
    }
    
    // Clear timecodes list
    timecodes.clear();

    // signal start recording
    g_record_start = true;
    
    captureSuspendGuard.confirmStatus(); // everything went ok, so let capture continue

    return true;
}

bool Capture::stop_multi_item_record(int duration, VTRError *vtrErrors, long numVTRErrors,
    int* pseResult, long* digiBetaDropoutCount)
{
    InfaxData infaxData;
    memset(&infaxData, 0, sizeof(InfaxData));

    *pseResult = -1; // only set to 0 or 1 later on if the analysis was completed
    
    if (! g_record_start) {
        logFF("stop_multi_item_record: FAILED - recording not in progress\n");
        return false;
    }

    // duration of -1 means stop now (this instant)
    if (duration == -1) {
        g_record_stop_frame = last_captured();
    }
    else {
        g_record_stop_frame = g_record_start_frame + duration - 1;
    }

    logFF("stop_multi_item_record: g_record_stop_frame = %d (duration = %d)\n", g_record_stop_frame, duration);

    // wait until recording stopped before returning
    while (g_record_start) {
        // wake up the store threads
        signal_frame_ready();
        
        sleep_msec(20);
    }

    // suspend the capture thread
    g_suspend_capture = true;

    // Get all PSE result
    long numPSEFailues = 0;
    PSEFailure *pseFailures = 0;
    bool pse_was_open = pse->is_open();
    if (pse_enabled && pse_was_open)
    {
        std::vector<PSEResult> results;
        pse->get_remaining_results(results);
        pse->close();
    
        // Convert PSE results to C array for MXF call
        numPSEFailues = results.size();
        logFF("PSE failures = %ld\n", numPSEFailues);
        pseFailures = new PSEFailure[numPSEFailues];
        for (long i = 0; i < numPSEFailues; i++) {
            pseFailures[i].position = results[i].position;
            pseFailures[i].vitcTimecode = timecodes[pseFailures[i].position].first;
            pseFailures[i].ltcTimecode = timecodes[pseFailures[i].position].second;
            pseFailures[i].redFlash = results[i].red;
            pseFailures[i].luminanceFlash = results[i].flash;
            pseFailures[i].spatialPattern = results[i].spatial;
            pseFailures[i].extendedFailure = results[i].extended;
            logFF("    %4ld: pos=%6"PRIu64" VITC=%02d:%02d:%02d:%02d LTC=%02d:%02d:%02d:%02d red=%4d fls=%4d spt=%4d ext=%d\n", i, pseFailures[i].position,
                    pseFailures[i].vitcTimecode.hour, pseFailures[i].vitcTimecode.min,
                    pseFailures[i].vitcTimecode.sec, pseFailures[i].vitcTimecode.frame,
                    pseFailures[i].ltcTimecode.hour, pseFailures[i].ltcTimecode.min,
                    pseFailures[i].ltcTimecode.sec, pseFailures[i].ltcTimecode.frame,
                    pseFailures[i].redFlash, pseFailures[i].luminanceFlash, pseFailures[i].spatialPattern, pseFailures[i].extendedFailure);
        }
    }

    // get digibeta dropouts
    long numDigiBetaDropouts = 0;
    DigiBetaDropout *digiBetaDropouts = 0;
    if (dropout_detector && dropout_detector->getNumDropouts() > 0) {
        digiBetaDropouts = &dropout_detector->getDropouts()[0];
        numDigiBetaDropouts = (long)dropout_detector->getNumDropouts();
    }

    
    // Complete MXF file
    {
        LOCK_SECTION(mxfout_mutex);

        int res = mxfout->complete(&infaxData,
                                   pseFailures, numPSEFailues,
                                   vtrErrors, numVTRErrors,
                                   digiBetaDropouts, numDigiBetaDropouts);
        if (!res) {
            logFF("stop_multi_item_record: Failed to complete writing Archive MXF file\n");
        }

        close_mxf_file_writer();
    }


    
    // Get the overall PSE result
    if (pse_enabled && pse_was_open)
    {
        bool result = PSEReport::hasPassed(pseFailures, numPSEFailues);
        *pseResult = result ? 0 : 1;
        logFF("get pse result succeeded\n");

        delete [] pseFailures;
    }

    *digiBetaDropoutCount = 0;
    if (dropout_detector)
    {
        *digiBetaDropoutCount = dropout_detector->getNumDropouts();
        
        pthread_mutex_lock(&m_dropout_detector);
        delete dropout_detector;
        dropout_detector = 0;
        pthread_mutex_unlock(&m_dropout_detector);
    }
    

    logFF("stop_multi_item_record: finished, g_frames_written=%d\n", g_frames_written);
    return true;
}

bool Capture::abort_record(void)
{
    logFF("abort_record\n");

    if (g_record_start) {
        // suspend the capture thread
        g_suspend_capture = true;
    
        // signal to store thread to stop
        g_record_stop_frame = last_captured();
    
        // wait until writing thread has finished with current frame
        // (an alternative would be to have a mutux around use of mxfout and
        // to stop the writing thread accessing it after abort_archive_mxf_file())
        while (g_record_start) {
            // wake up the store threads
            signal_frame_ready();
            
            sleep_msec(20);
        }
    }
    else {
        // suspend the capture thread
        g_suspend_capture = true;

        // wake up the browse thread
        // eg. the mxf store buffer overflowed and the sdi signal is bad
        signal_frame_ready();
    }


    // abort MXF writing
    bool res = true;
    {
        LOCK_SECTION(mxfout_mutex);

        if (mxfout)
        {
            res = mxfout->abort();
            logFF("abort_record: abort_archive_mxf_file() returned %d\n", res);
            
            close_mxf_file_writer();
        
            
            // delete file
            if (mxf_page_file_is_page_filename(mxf_filename))
            {
                int status = mxf_page_file_remove(mxf_filename);
                logFF("abort_record: mxf_page_file_remove(%s) returned %d\n", mxf_filename, status);
                if (!status) {
                    logFF("abort_record: mxf_page_file_remove failed\n");
                    res = 0;    // abort failed to remove file
                }
            }
            else
            {
                int status = remove(mxf_filename);
                logFF("abort_record: remove(%s) returned %d\n", mxf_filename, status);
                if (status != 0) {
                    logFF("abort_record: remove failed: %s\n", strerror(status));
                    res = 0;    // abort failed to remove file
                }
            }
        }
    }

    // Close PSE analysis
    if (pse_enabled && pse->is_open())
    {
        pse->close();
    }

    // close digibeta dropout detection
    // Note that we assume the dropout detector is running in the store_video_thread thread which has
    // finished writing
    if (dropout_detector) {
        pthread_mutex_lock(&m_dropout_detector);
        delete dropout_detector;
        dropout_detector = 0;
        pthread_mutex_unlock(&m_dropout_detector);
    }
    
    g_aborted = true;

    logFF("abort_record: returning %d\n", res);
    return res;
}

bool Capture::restart_capture(long timeout_ms, int *last_frame_captured)
{
    int prev_last_frame_captured = last_captured();
    *last_frame_captured = prev_last_frame_captured;
    
    // TODO: use conditional variables to communicate between threads
    // wait for first frame to be captured
    rec::Timer timer;
    timer.start(timeout_ms * MSEC_IN_USEC);
    while ((*last_frame_captured) <= prev_last_frame_captured && timer.timeLeft() > 0)
    {
        rec::sleep_msec(10);
        (*last_frame_captured) = last_captured();
    }
    if ((*last_frame_captured) <= prev_last_frame_captured)
    {
        return false;
    }
    
    return true;
}

BrowseFrame* Capture::get_browse_frame_write()
{
    // wait for browse frame to become available to write (!read) 
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    while (browse_frames[(last_browse_frame_write + 1) % 2].readyToRead)
    {
        pthread_cond_wait(&browse_frame_ready_cond, &m_browse_frame_ready_cond);
    }
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
    
    last_browse_frame_write++;
    return &browse_frames[last_browse_frame_write % 2];
}

BrowseFrame* Capture::get_browse_frame_read()
{
    // wait for browse frame to become available to read 
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    while (!browse_frames[(last_browse_frame_read + 1) % 2].readyToRead)
    {
        pthread_cond_wait(&browse_frame_ready_cond, &m_browse_frame_ready_cond);
    }
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
    
    last_browse_frame_read++;
    return &browse_frames[last_browse_frame_read % 2];
}

void Capture::browse_end_sequence()
{
    BrowseFrame* frame = get_browse_frame_write();

    frame->isBlackFrame = false;
    frame->isEndOfSequence = true;
    
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    frame->readyToRead = true; // frame (end of sequence indicator) available for reading
    pthread_cond_signal(&browse_frame_ready_cond);
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
}

void Capture::browse_add_frame(uint8_t *video, uint8_t *audio, rec::Timecode ltc, rec::Timecode vitc)
{
    BrowseFrame* frame = get_browse_frame_write();
    
    /* convert video to yuv420 */    
    uyvy_to_yuv420(720, 576, 0, video, frame->video);

    /* The first half of the first line is set to black because it contains PAL decode information, and
    this bursty data will cause disturbances in the surrounding video after encoding. The last half of the
    last line is set to black to be consistent */
    
    /* TODO and WARNING: this assumes the UYVY to YUV420 conversion skips every second line of U and V samples to
    get the vertical downconversion */
    
    /* Note: we don't modify the video buffer because it is used by other threads for writing to file and 
    display */
    
    memset(frame->video, 0x10, 360); /* first line, first half Y */
    memset(frame->video + 720 * 576, 0x80, 180); /* first line, first half U */
    memset(frame->video + 720 * 576 + 360 * 288, 0x80, 180); /* first line, first half V */
    memset(frame->video + 720 * 576 - 360, 0x10, 360); /* last line, second half Y */

    /* convert 2 channel 32 bit dvs audio to 16 bit audio */
    dvsaudio32_to_16bitstereo(audio, frame->audio);
    
    /* timecodes */
    frame->ltc = ltc;
    frame->vitc = vitc;
    
    
    frame->isBlackFrame = false;
    frame->isEndOfSequence = false;
    
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    frame->readyToRead = true; // frame available for reading
    pthread_cond_signal(&browse_frame_ready_cond);
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
}

void Capture::browse_add_black_frame(rec::Timecode ltc, rec::Timecode vitc)
{
    BrowseFrame* frame = get_browse_frame_write();
    
    frame->ltc = ltc;
    frame->vitc = vitc;
    
    frame->isBlackFrame = true;
    frame->isEndOfSequence = false;
    
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    frame->readyToRead = true; // frame available for reading
    pthread_cond_signal(&browse_frame_ready_cond);
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
}

void Capture::encode_browse_thread(void)
{
    ThreadRunningGuard guard(&browse_thread_running);
    
    BrowseEncoder* encoder = BrowseEncoder::create(browse_filename, aspect_ratio, browse_kbps, browse_thread_count);
    if (encoder == 0)
    {
        logTF("Failed to create the browse encoder for file '%s'\n", browse_filename);

        logTF("Browse encode thread exited\n");
        pthread_exit(NULL);
    }
    
    // encode browse frames
    while (true)
    {
        BrowseFrame* frame = get_browse_frame_read();
        if (frame->isEndOfSequence)
        {
            // done encoding
            break;
        }

        if (frame->isBlackFrame)
        {
            if (!encoder->encode(black_browse_video, silent_browse_audio, g_browse_written++)) 
            {
                logTF("browse_encoder_encode failed\n");
            }
        }
        else
        {
            if (!encoder->encode(frame->video, frame->audio, g_browse_written++)) 
            {
                logTF("browse_encoder_encode failed\n");
            }
        }

        pthread_mutex_lock(&m_browse_frame_ready_cond);
        frame->readyToRead = false; // frame available for writing
        pthread_cond_signal(&browse_frame_ready_cond);
        pthread_mutex_unlock(&m_browse_frame_ready_cond);
    }

    // Complete browse file
    delete encoder;
    
    logTF("Browse encode thread exited\n");
    pthread_exit(NULL);
}


// Read from ring memory buffer, prepare frame for encoding
void Capture::store_browse_thread(void)
{
    int browse_frames_written = 0;
    int overflow_count = 0;

    FILE* timecodeFile = fopen(browse_timecode_filename, "wb");
    if (timecodeFile == 0)
    {
        logTF("Failed to open browse timecode file '%s': %s\n", browse_timecode_filename, strerror(errno));
        
        browse_end_sequence();

        logTF("Browse store thread exited\n");
        pthread_exit(NULL);
    }
    

    // wait for the start signal
    bool noFrames = false;
    while (true)
    {
        // stop recording and exit if no frames recorded
        if (g_record_stop_frame != -2 &&
            g_last_browse_frame_written >= g_record_stop_frame) 
        {
            noFrames = true;
            break;
        }
        
        // g_record_start signals start of write-to-disk
        if (!g_record_start) 
        {
            wait_for_frame_ready();
            continue;
        }
        
        // started
        break;
    }

    if (noFrames)
    {
        browse_end_sequence();
        fclose(timecodeFile);

        logTF("Browse store (preprocess) thread exited\n");
        pthread_exit(NULL);
    }

    // pre-process frames for encode thread and write timecodes
    while (true)
    {
        int last_frame_captured = last_captured();

        // stop recording and exit if reached last frame
        if (g_record_stop_frame != -2 &&
            g_last_browse_frame_written >= g_record_stop_frame) 
        {
            // end of recording
            break;
        }

        // Handle case when nothing available yet (empty buffer indicated by -1)
        if (last_frame_captured == -1) 
        {
            // TODO: abort if capture has signalled start, but no data ready
            wait_for_frame_ready();
            continue;
        }
        
        // If nothing to save, wait until new frame (or frames) ready
        if (g_last_browse_frame_written >= last_frame_captured) 
        {
            wait_for_frame_ready();
            continue;
        }

        // Pre-check we haven't wrapped around ring buffer (-1 because of the 1 frame capture delay and buffer)
        if (overflow_count == 0 && 
            last_frame_captured - g_last_browse_frame_written >= ring_buffer_size - 1) 
        {
            logTF("Browse failed: ring buffer overflow: last_frame_captured(%d)-last_frame_written(%d) >= ring_buffer_size-1(%d)\n",
                last_frame_captured, g_last_browse_frame_written, ring_buffer_size - 1);
                
            // we don't stop the recording if the browse fails - just report the problem
            report_browse_buffer_overflow();
            
            // skip 3/4 of the frames to try and catch up
            overflow_count += browse_overflow_frames;
            overflow_count = (overflow_count > ring_buffer_size) ? ring_buffer_size : overflow_count;
            logTF("Writing %d black frames to browse file\n", browse_overflow_frames);

            continue;
        }

        if (overflow_count > 0)
        {
            // write black frame
            
            rec::Timecode ltc, vitc, ctc;
            int_to_timecode(0, &ltc);
            int_to_timecode(0, &vitc);
            int_to_timecode(browse_frames_written, &ctc);
            browse_frames_written++;
            
            // Add frame to browse encoding
            browse_add_black_frame(ltc, vitc);
            
            // write timecodes to file
            // control timecode, vertical interval timecode and linear timecode
            write_browse_timecode(timecodeFile, ctc, vitc, ltc);
            
            overflow_count--;
        }
        else
        {
            // next frame which needs to be written
            uint8_t *frame = rec_ring_frame(g_last_browse_frame_written + 1);
            uint8_t *video_8bit = rec_ring_8bit_video_frame(g_last_browse_frame_written + 1);
            uint8_t *active_video = video_8bit;
            if (palff_mode)
                active_video += 16 * 720*2; // VBI is not included in the browse copy
        
            // read ltc and vitc from ring buffer
            int ltc_as_int, vitc_as_int;
            memcpy(&ltc_as_int, frame + ring_ltc_offset(), sizeof(ltc_as_int));
            memcpy(&vitc_as_int, frame + ring_vitc_offset(), sizeof(vitc_as_int));
    
            rec::Timecode ltc, vitc, ctc;
            int_to_timecode(ltc_as_int, &ltc);
            int_to_timecode(vitc_as_int, &vitc);
            int_to_timecode(browse_frames_written, &ctc);
            browse_frames_written++;
            
            // Add frame to browse encoding
            browse_add_frame(active_video, frame + ring_audio_pair_offset(0), ltc, vitc);
            
            // write timecodes to file
            // control timecode, vertical interval timecode and linear timecode
            write_browse_timecode(timecodeFile, ctc, vitc, ltc);
        }

        // Post-check we haven't wrapped around ring buffer (-1 because of the 1 frame capture delay and buffer)
        last_frame_captured = last_captured();
        if (overflow_count == 0 && 
            last_frame_captured - g_last_browse_frame_written >= ring_buffer_size - 1)
        {
            logTF("Browse failed: ring buffer overflow: last_frame_captured(%d)-last_frame_written(%d) >= ring_buffer_size-1(%d)\n",
                last_frame_captured, g_last_browse_frame_written, ring_buffer_size - 1);
                
            // we don't stop the recording if the browse fails - just report the problem
            report_browse_buffer_overflow();
            
            // skip 3/4 of the frames to try and catch up
            overflow_count += browse_overflow_frames;
            overflow_count = (overflow_count > ring_buffer_size) ? ring_buffer_size : overflow_count;
            logTF("Writing %d black frames to browse file\n", browse_overflow_frames);
            
            continue;
        }
        
        g_last_browse_frame_written++;
        
        // update buffer state
        update_browse_buffer_pos(g_last_browse_frame_written);
        
        if (debug_store_thread_buffer)
        {
            logFF("Browse store buffer: position = %d, offset to head = %d\n", 
                g_last_browse_frame_written, last_frame_captured - g_last_browse_frame_written);
        }
    }

    browse_end_sequence();
    fclose(timecodeFile);

    logTF("Browse store thread exited\n");
    pthread_exit(NULL);
}

bool Capture::stop_output_fifo(void)
{
    int res;
    if ((res = sv_fifo_stop(sv, poutput, SV_FIFO_FLAG_FLUSH)) != SV_OK) {
        logTF("sv_fifo_stop(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_stop(poutput)\n");

    if ((res = sv_fifo_free(sv, poutput)) != SV_OK) {
        logTF("sv_fifo_free(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_free(poutput)\n");

    return true;
}

bool Capture::init_start_output_fifo(void)
{
    int res;

    // create FIFO with args bInput=FALSE, bShared(in/out use same mem)=FALSE, bDMA=TRUE
    // fifoflags=0, nframes=0 means use all available card memory
    if ((res = sv_fifo_init(sv, &poutput, FALSE, FALSE, TRUE, 0, 0)) != SV_OK) {
        logTF("sv_fifo_init(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_init(poutput)\n");

    // Start the fifo.  The dropped counter starts counting dropped frames from now.
    if ((res = sv_fifo_start(sv, poutput)) != SV_OK) {
        logTF("sv_fifo_start(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_start(poutput)\n");

    return true;
}

bool Capture::stop_input_fifo(void)
{
    int res;
    if ((res = sv_fifo_stop(sv, pinput, 0)) != SV_OK) {
        logTF("sv_fifo_stop(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_stop(pinput)\n");

    if ((res = sv_fifo_free(sv, pinput)) != SV_OK) {
        logTF("sv_fifo_free(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_free(pinput)\n");

    return true;
}

bool Capture::init_start_input_fifo(void)
{
    int res;

    // create FIFO with args bInput=TRUE, bShared(in/out use same mem)=FALSE, bDMA=TRUE
    // fifoflags=0, nframes=0 means use all available card memory
    if ((res = sv_fifo_init(sv, &pinput, TRUE, FALSE, TRUE, 0, 0)) != SV_OK) {
        logTF("sv_fifo_init(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_init(pinput)\n");

    // Start the fifo.  The dropped counter starts counting dropped frames from now.
    if ((res = sv_fifo_start(sv, pinput)) != SV_OK) {
        logTF("sv_fifo_start(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_start(pinput)\n");

    return true;
}

void Capture::wait_for_good_SDI_signal(int required_good_frames)
{
    int good_frames = 0;

    // Poll until we get a good video signal for at least required_good_frames
    while (!g_suspend_capture) {
        int videoin, audioin;
        unsigned int h_clock, l_clock;
        int current_tick;
        int res1, res2, res3;
        res1 = sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock);
        res2 = sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin);
        res3 = sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin);
        int query_ok = (res1 == SV_OK && res2 == SV_OK && res3 == SV_OK);

        if (loglev > 2) {
            logFF("wait_for_good_SDI_signal: query_ok=%d tick=%8d video=%s audio=%s\n",
                query_ok,
                current_tick,
                videoin == SV_OK ? "OK" : "error",
                audioin == SV_OK ? "OK" : "error"
                );
        }

        if (query_ok && videoin == SV_OK && audioin == SV_OK) {
            good_frames++;
            if (good_frames == required_good_frames) {
                logFF("wait_for_good_SDI_signal: found %d good frames, returning\n", good_frames);
                break;                          // break from while(!g_suspend_capture) loop
            }
        }

        
        sleep_msec(18);        // sleep slightly less than one tick (20,000)
    }

    return;
}

void Capture::poll_sdi_signal_status(void)
{
    int videoin, audioin;
    unsigned int h_clock, l_clock;
    int current_tick;
    int valid_timecode;
    int res1, res2, res3, res4;
    int query_ok;
    
    res1 = sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock);
    res2 = sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin);
    res3 = sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin);
    res4 = sv_query(sv, SV_QUERY_VALIDTIMECODE, 0, &valid_timecode);
    query_ok = (res1 == SV_OK && res2 == SV_OK && res3 == SV_OK && res4 == SV_OK);

    set_input_SDI_status(query_ok && videoin == SV_OK,
                         query_ok && audioin == SV_OK,
                         query_ok && (valid_timecode & SV_VALIDTIMECODE_DLTC),
                         query_ok && (valid_timecode & SV_VALIDTIMECODE_VITC_F1));
}

// Capture frames from sv fifo and write to ring buffer
// Read VITC and burnt-in LTC from VBI when using PALFF mode
void Capture::capture_video_thread(void)
{
    // log the scheduling policy
    int policy;
    struct sched_param param;
    pthread_getschedparam(pthread_self(), &policy, &param);
    logTF("capture scheduling policy='%s' with priority=%d\n",
        policy == SCHED_RR ? "realtime" : "non-realtime", param.sched_priority);
        
    // Boolean controlling whether the sv_fifo_getbuffer flushes the fifo. 
    // DVS support suggests this is only done once the first time sv_fifo_getbuffer is called
    bool first_sv_fifo_getbuffer = true;

    // Loop forever capturing frames into ring buffer
    int wait_good_frame_count = 0;
    int last_res = SV_OK;
    bool start_input_fifo = true;
    bool have_previous_capture_frame = false;
    while (1) {
        
        // wait and poll the SDI status if the capture has been suspended
        if (g_suspend_capture)
        {
            logFF("Suspended capturing\n");
            flushLogFile();
            
            if (!start_input_fifo)
            {
                // have started the fifo - stop it 
                stop_input_fifo();
            }
            
            // poll SDI signal status until capture is no longer suspended
            while (g_suspend_capture)
            {
                poll_sdi_signal_status();
                sleep_msec(18);      // sleep slightly less than one tick (20,000)
            }

            start_input_fifo = true;
        }
        
        // start the fifo if neccessary
        if (start_input_fifo)
        {
            init_start_input_fifo();
            start_input_fifo = false;
            first_sv_fifo_getbuffer = true;
            wait_good_frame_count = WAIT_GOOD_FRAME_COUNT;
            have_previous_capture_frame = false;
            ltc_decoder.reset();
            
            logFF("Starting capturing\n");
            flushLogFile();
        }
        else
        {
            first_sv_fifo_getbuffer = false;
        }
        
        
        int last_frame_captured = last_captured();

        // the index of the next frame to be captured is the reported last frame captured (for consumption by 
        // other threads) + 2, unless there is not previous frame, in which case it is + 1
        int capture_buffer_index = last_frame_captured + 1;
        if (have_previous_capture_frame)
            capture_buffer_index++;

        // Get sv memmory buffer
        sv_fifo_buffer      *pbuffer;
        int                 res;

        // Tell hardware to capture video (into hardware memory)
        // If sv_fifo_getbuffer() fails it typically takes 20ms for each
        // capture attempt in sv_fifo_getbuffer().
        int flags = first_sv_fifo_getbuffer ? SV_FIFO_FLAG_FLUSH : 0;
        if ((res = sv_fifo_getbuffer(sv, pinput, &pbuffer, NULL, flags)) == SV_OK) {
            
            // sdi signal is good
            set_input_SDI_status(true, true, true, true);

            if (res != last_res)
                logFF("capture succeeded with video and audio OK\n");
            last_res = res;
        }
        else {
            logFF("sv_fifo_getbuffer(pinput) failed: %s\n", sv_geterrortext(res));
            logFF("wait for good frame count: %d\n", wait_good_frame_count);

            // sdi signal is bad
            set_input_SDI_status(false, false, false, false);

            // Stop fifo and wait for good signal before continuing
            stop_input_fifo();
            start_input_fifo = true;

            assert(WAIT_GOOD_FRAME_AFTER_ERROR_COUNT - WAIT_GOOD_FRAME_COUNT > 0);
            wait_for_good_SDI_signal(WAIT_GOOD_FRAME_AFTER_ERROR_COUNT - WAIT_GOOD_FRAME_COUNT);
            
            // Video signal should now be good, try capture again
            // or capture has been suspended
            continue;
        }

        // store new frame into next element in ring buffer
        uint8_t *prev_frame;
        uint8_t *frame;
        if (have_previous_capture_frame) {
            prev_frame = rec_ring_frame(capture_buffer_index - 1);
            frame = rec_ring_frame(capture_buffer_index);
        } else {
            prev_frame = 0;
            frame = rec_ring_frame(capture_buffer_index);
        }
        pbuffer->dma.addr = (char *)frame;
        pbuffer->dma.size = ring_element_size();
        if (audio_pair_offset[0] == 0) {
            audio_pair_offset[0] = (unsigned long)pbuffer->audio[0].addr[0];
            audio_pair_offset[1] = (unsigned long)pbuffer->audio[0].addr[1];
            audio_pair_offset[2] = (unsigned long)pbuffer->audio[0].addr[2];
            audio_pair_offset[3] = (unsigned long)pbuffer->audio[0].addr[3];
        }

        // Transfer video to host memory
        if ((res = sv_fifo_putbuffer(sv, pinput, pbuffer, NULL)) != SV_OK) {
            logTF("sv_fifo_putbuffer(pinput) failed: %s\n", sv_geterrortext(res));
            logFF("wait for good frame count: %d\n", wait_good_frame_count);

            // update recording status
            set_recording_status(false);

            // suspend capture
            g_suspend_capture = true;
            
            continue;
        }

        // only start storing frames if we have the number of good frames
        if (wait_good_frame_count > 0) {
            logFF("wait for good frame count: %d\n", wait_good_frame_count);
            wait_good_frame_count--;

            // process audio track with LTC to ensure the LTC decoder has found the sync word and has all samples
            // needed to set the LTC for the first frame
            if (ltc_source == AUDIO_TRACK_LTC_SOURCE) {
                int dummy_ltc_as_int;
                bool dummy_is_prev_frame_ltc;
                read_audio_track_ltc(frame + ring_audio_pair_offset(ltc_audio_track / 2), ltc_audio_track % 2,
                                     &dummy_ltc_as_int, &dummy_is_prev_frame_ltc);
            }

            continue;
        }
        
        // 10- to 8-bit conversion
        uint8_t *video_8bit = rec_ring_8bit_video_frame(capture_buffer_index);
        DitherFrame(video_8bit, frame, video_width * 2, (video_width + 5) / 6 * 16, video_width, video_height);
        

        if (debug_clapper_avsync) {
            int line_size = 720*2;
            int flash;
            if (palff_mode)
                flash = find_red_flash_uyvy(video_8bit + line_size * 16,  line_size);     // skip 16 lines
            else
                flash = find_red_flash_uyvy(video_8bit,  line_size);
            int click1 = 0, click1_off = -1;
            int click2 = 0, click2_off = -1;
            int click3 = 0, click3_off = -1;
            int click4 = 0, click4_off = -1;
            find_audio_click_32bit_stereo(frame + ring_audio_pair_offset(0),
                                                        &click1, &click1_off,
                                                        &click2, &click2_off);
            find_audio_click_32bit_stereo(frame + ring_audio_pair_offset(1),
                                                        &click3, &click3_off,
                                                        &click4, &click4_off);

            if (flash || click1 || click2 || click3 || click4) {
                if (flash)
                    logFF(" %5d  red-flash      \n", capture_buffer_index);
                if (click1)
                    logFF(" %5d  a1off=%d %.1fms\n", capture_buffer_index, click1_off, click1_off / 1920.0 * 40);
                if (click2)
                    logFF(" %5d  a2off=%d %.1fms\n", capture_buffer_index, click2_off, click2_off / 1920.0 * 40);
                if (click3)
                    logFF(" %5d  a3off=%d %.1fms\n", capture_buffer_index, click3_off, click3_off / 1920.0 * 40);
                if (click4)
                    logFF(" %5d  a4off=%d %.1fms\n", capture_buffer_index, click4_off, click4_off / 1920.0 * 40);
            }
        }

        // get status such as dropped frames
        sv_fifo_info fifo_info;
        if (sv_fifo_status(sv, pinput, &fifo_info) == SV_OK) {
            if (fifo_info.dropped > 0)
            {
                logTF("Recording failed: DVS dropped frame: last_frame_captured(%d)\n", last_frame_captured);

                // report the dropped frame
                report_sdi_capture_dropped_frame();
                
                // suspend capture
                g_suspend_capture = true;
                
                // update recording status
                set_recording_status(false);
    
                continue;
            }
        }

        if (loglev > 1)
            logFF("last_frame_captured=%6d %s", last_frame_captured + 1, g_record_start ? "rec " : "");


        // read timecodes
        
        int i;
        int valid_timecode;
        int valid_timecode_result = sv_query(sv, SV_QUERY_VALIDTIMECODE, 0, &valid_timecode);

        // read vitc
        int vitc_as_int = -1;
        switch (vitc_source)
        {
            case ANALOGUE_VITC_SOURCE:
                if (valid_timecode_result == SV_OK &&
                    ((valid_timecode & SV_VALIDTIMECODE_VITC_F1) || (valid_timecode & SV_VALIDTIMECODE_VITC_F2)))
                {
                    vitc_as_int = read_fifo_vitc(pbuffer, (valid_timecode & SV_VALIDTIMECODE_VITC_F1),
                        (valid_timecode & SV_VALIDTIMECODE_VITC_F2));
                }
                break;
            case VBI_VITC_SOURCE:
                assert(palff_mode);
                for (i = 0; i < num_vitc_lines; i++)
                {
                    vitc_as_int = read_timecode(video_8bit, 720 * 2, vitc_lines[i], "VITC");
                    if (vitc_as_int >= 0)
                    {
                        // got it
                        break;
                    }
                }
                break;
            case NO_VITC_SOURCE:
                break;
        }
        memcpy(frame + ring_vitc_offset(), &vitc_as_int, sizeof(vitc_as_int));
        
        // read ltc
        int ltc_as_int = -1;
        bool is_prev_frame_ltc;
        switch (ltc_source)
        {
            case DIGITAL_LTC_SOURCE:
                if (valid_timecode_result == SV_OK && (valid_timecode & SV_VALIDTIMECODE_DLTC))
                {
                    ltc_as_int = read_fifo_dltc(pbuffer);
                }
                break;
            case ANALOGUE_LTC_SOURCE:
                if (valid_timecode_result == SV_OK && (valid_timecode & SV_VALIDTIMECODE_LTC))
                {
                    ltc_as_int = read_fifo_ltc(pbuffer);
                }
                break;
            case VBI_LTC_SOURCE:
                assert(palff_mode);
                for (i = 0; i < num_ltc_lines; i++)
                {
                    ltc_as_int = read_timecode(frame, 720 * 2, ltc_lines[i], "  LTC");
                    if (ltc_as_int >= 0)
                    {
                        // got it
                        break;
                    }
                }
                break;
            case AUDIO_TRACK_LTC_SOURCE:
                if (read_audio_track_ltc(frame + ring_audio_pair_offset(ltc_audio_track / 2), ltc_audio_track % 2,
                                         &ltc_as_int, &is_prev_frame_ltc))
                {
                    if (is_prev_frame_ltc) {
                         if (have_previous_capture_frame)
                             memcpy(prev_frame + ring_ltc_offset(), &ltc_as_int, sizeof(ltc_as_int));
                        ltc_as_int = -1;    // this frame's LTC is unknown at this point
                    }
                }
                break;
            case NO_LTC_SOURCE:
                break;
        }
        memcpy(frame + ring_ltc_offset(), &ltc_as_int, sizeof(ltc_as_int));

        if (loglev > 1) logFFi("\n");

        
        if (have_previous_capture_frame) {
            // the previous frame is ready for consumption
            // last_frame_captured + 1 == capture_buffer_index - 1
            
            // update stats
            memcpy(&current_vitc, prev_frame + ring_vitc_offset(), sizeof(current_vitc));
            memcpy(&current_ltc, prev_frame + ring_ltc_offset(), sizeof(current_ltc));
            
            // update buffer state
            update_capture_buffer_pos(fifo_info.nbuffers, fifo_info.availbuffers, last_frame_captured + 1);
            
            // update shared variable to signal to disk writing thread
            update_last_captured(last_frame_captured + 1);
        } else {
            have_previous_capture_frame = true;
        }
    }
}

// Read from ring memory buffer, write to disk file
void Capture::store_video_thread(void)
{
    while (1)
    {
        int last_frame_captured = last_captured();
        int last_frame_written = last_written();

        // g_record_start signals start of write-to-disk
        if (! g_record_start) {
            wait_for_frame_ready();
            continue;
        }

        // Indicate failed capture if no good video after grace period
        //set_recording_status(false);

        // stop recording if reached last frame
        // g_record_stop_frame == -1 means stop the record before the first
        // frame was captured
        if (g_record_stop_frame != -2 &&
                last_frame_written >= g_record_stop_frame) {
            // Signal recording stopped
            g_record_start = false;
            g_record_start_frame = -1;
            // Note: we don't reset g_record_stop_frame here becuase the browse thread could
            // still be busy


            wait_for_frame_ready();
            continue;
        }

        // Handle case when nothing available yet (empty buffer indicated by -1)
        if (last_frame_captured == -1) {
            wait_for_frame_ready();
            continue;
        }

        // If nothing to save, wait until new frame (or frames) ready
        if (last_frame_written >= last_frame_captured) {
            wait_for_frame_ready();
            continue;
        }


        // Pre-check we haven't wrapped around ring buffer (-1 because of the 1 frame capture delay and buffer)
        if (last_frame_captured - last_frame_written >= ring_buffer_size - 1) {
            logTF("Recording failed: ring buffer overflow: last_frame_captured(%d)-last_frame_written(%d) >= ring_buffer_size-1(%d)\n",
                last_frame_captured, last_frame_written, ring_buffer_size - 1);

            // report the overflow
            report_mxf_buffer_overflow();
            
            // Stop recording
            g_record_stop_frame = last_frame_written;

            // update recording status
            set_recording_status(false);

            continue;
        }

        // next frame which needs to be written
        uint8_t *ring_frame = rec_ring_frame(last_frame_written + 1);
        uint8_t *video_8bit = rec_ring_8bit_video_frame(last_frame_written + 1);
        uint8_t *active_8bit_video = video_8bit;
        if (palff_mode) 
        {
            active_8bit_video += 720 * 2 * 16;
        }
        
        frame_writer->setAudioPairOffset(0, audio_pair_offset[0]);
        frame_writer->setAudioPairOffset(1, audio_pair_offset[1]);
        frame_writer->setAudioPairOffset(2, audio_pair_offset[2]);
        frame_writer->setAudioPairOffset(3, audio_pair_offset[3]);
        frame_writer->setRingFrame(ring_frame, video_8bit);
        
        frame_writer->writeFrame();

    
        // read ltc and vitc from ring buffer
        int ltc_as_int, vitc_as_int;
        memcpy(&ltc_as_int, ring_frame + ring_ltc_offset(), sizeof(ltc_as_int));
        memcpy(&vitc_as_int, ring_frame + ring_vitc_offset(), sizeof(vitc_as_int));

        
        // Record timecodes for later use by PSE report
        ArchiveTimecode ltc, vitc;
        int_to_archive_timecode(ltc_as_int, &ltc);
        int_to_archive_timecode(vitc_as_int, &vitc);
        timecodes.push_back( make_pair(vitc, ltc) );


        // Post-check we haven't wrapped around ring buffer (-1 because of the 1 frame capture delay and buffer)
        last_frame_captured = last_captured();
        if (last_frame_captured - last_frame_written >= ring_buffer_size - 1) {
            logTF("Recording failed: ring buffer overflow: last_frame_captured(%d)-last_frame_written(%d) >= ring_buffer_size-1(%d)\n",
                last_frame_captured, last_frame_written, ring_buffer_size - 1);

            // report the overflow
            report_mxf_buffer_overflow();
            
            // Stop recording
            g_record_stop_frame = last_frame_written;

            // update recording status
            set_recording_status(false);
            
            continue;
        }
        
        g_frames_written++;

        // preserve first frame recorded to calculate duration later
        if (g_record_start_frame == -1)
            g_record_start_frame = last_frame_written + 1;

        update_last_written(last_frame_written + 1);

        if (debug_clapper_avsync) {
            int line_size = 720*2;
            int flash = find_red_flash_uyvy(active_8bit_video, line_size);
            int click1 = 0, click1_off = -1;
            int click2 = 0, click2_off = -1;
            int click3 = 0, click3_off = -1;
            int click4 = 0, click4_off = -1;
            find_audio_click_32bit_stereo(ring_frame + ring_audio_pair_offset(0),
                                                        &click1, &click1_off,
                                                        &click2, &click2_off);
            find_audio_click_32bit_stereo(ring_frame + ring_audio_pair_offset(1),
                                                        &click3, &click3_off,
                                                        &click4, &click4_off);

            if (flash || click1 || click2 || click3 || click4) {
                if (flash)
                    logFF(" store %5d  red-flash      \n", last_frame_captured + 1);
                if (click1)
                    logFF(" store %5d  a1off=%d %.1fms\n", last_frame_captured + 1, click1_off, click1_off / 1920.0 * 40);
                if (click2)
                    logFF(" store %5d  a2off=%d %.1fms\n", last_frame_captured + 1, click2_off, click2_off / 1920.0 * 40);
                if (click3)
                    logFF(" store %5d  a3off=%d %.1fms\n", last_frame_captured + 1, click3_off, click3_off / 1920.0 * 40);
                if (click4)
                    logFF(" store %5d  a4off=%d %.1fms\n", last_frame_captured + 1, click4_off, click4_off / 1920.0 * 40);
            }
        }

        // Add frame to PSE analysis
        if (pse_enabled && pse->is_open())
        {
            pse->analyse_frame(active_8bit_video);
        }
        
        // digibeta dropout detection
        if (dropout_detector)
        {
            pthread_mutex_lock(&m_dropout_detector);
            dropout_detector->processPicture(active_8bit_video, 720 * 576 * 2);
            pthread_mutex_unlock(&m_dropout_detector);
        }

        // update buffer state
        update_store_buffer_pos(last_frame_written + 1);

        if (debug_store_thread_buffer)
        {
            logFF("MXF store buffer: position = %d, offset to head = %d\n", 
                last_frame_written + 1, last_frame_captured - (last_frame_written + 1));
        }
    }
}

Capture::Capture(CaptureListener* listener) : _listener(listener)
{
    rec_ring = NULL;
    ring_buffer_size = 125;
    memset(audio_pair_offset, 0, sizeof(audio_pair_offset));
    rec_8bit_video_ring = 0;

    g_last_frame_captured = -1;
    g_suspend_capture = true;
    
    ingest_format = rec::MXF_UNC_8BIT_INGEST_FORMAT;
    
    ltc_source = NO_LTC_SOURCE;
    ltc_audio_track = -1;
    vitc_source = NO_VITC_SOURCE;
    
    palff_mode = false;

    g_last_frame_written = -1;
    g_last_browse_frame_written = -1;
    g_record_start_frame = -1;
    g_record_start = false;
    g_record_stop_frame = -2; // -2 means not set, -1 means stop before any frame is captured
    g_frames_written = 0;
    g_browse_written = 0;
    g_aborted = false;
    start_vitc = 0;
    start_ltc = 0;
    current_vitc = 0;
    current_ltc = 0;
    last_output_fifo_dropped = 0;
    vitc_lines[0] = 19;
    vitc_lines[1] = 21;
    num_vitc_lines = 2;
    ltc_lines[0] = 15;
    ltc_lines[1] = 17;
    num_ltc_lines = 2;
    num_audio_tracks = 4;
    mxf_page_size = 100 * 1024 * 1024; // 100 MB
    video_ok = false;
    audio_ok = false;
    dltc_ok = false;
    vitc_ok = false;
    recording_ok = true;
    
    dvs_buffers_empty = 0;
    num_dvs_buffers = 0;
    capture_buffer_pos = 0;
    store_buffer_pos = 0;
    browse_buffer_pos = 0;
    
    digibeta_dropout_enabled = false;
    digibeta_dropout_lower_threshold = 100;
    digibeta_dropout_upper_threshold = 400;
    digibeta_dropout_store_threshold = 150;
    dropout_detector = 0;
    
    include_crc32 = false;
    
    primary_timecode = rec::PRIMARY_TIMECODE_AUTO;
    
    strcpy(mxf_filename, "");
    strcpy(browse_filename, "");
    strcpy(browse_timecode_filename, "");
    
    mxfout = 0;
    frame_writer = 0;

    // tmp space for one 720x576x2 YUV420P frame
    video_420 = new uint8_t[720*576*2];
    audio_16bitstereo = new int16_t[1920*2];
    
    // black video and silent audio for browse overflow frames
    silent_browse_audio = new int16_t[1920*2];
    memset(silent_browse_audio, 0, 1920 * 2 * sizeof(int16_t));
    black_browse_video = new uint8_t[720*576*2];
    yuv420p_black_frame(720, 576, black_browse_video);
    

    // Start off with no video, audio or timecode, and no recording error
    set_input_SDI_status(false, false, false, false);
    set_recording_status(true);

    
    // Init PSE engine
    pse_enabled = true;
#ifdef HAVE_PSE_FPA
    pse = new PSE_FPA();
#else
    pse = new PSE_Simple();
#endif
    if (!pse->init())
    {
        logTF("Failed to initialize the PSE engine\n");
    }

    // Browse
    browse_enabled = true;
    browse_kbps = 2700; // video bit rate
    browse_thread_count = 0; // no threads
    browse_overflow_frames = 50;
    int i;
    for (i = 0; i < 2; i++)
    {
        browse_frames[i].video = new uint8_t[720*576*2];
        browse_frames[i].audio = new int16_t[1920*2];
        browse_frames[i].readyToRead = false;
        browse_frames[i].isEndOfSequence = false;
        browse_frames[i].isBlackFrame = false;
    }
    last_browse_frame_write = 0;
    last_browse_frame_read = 0;
    browse_thread_running = false;

    // debug stuff
    
    // redirect libMXF logging if set to defaults
    if (mxf_log == mxf_log_default)
        mxf_log = redirect_mxf_logs;
    if (mxf_vlog == mxf_vlog_default)
        mxf_vlog = redirect_mxf_vlogs;
    
    loglev = 3;
    debug_clapper_avsync = false;
    fp_saved_lines = NULL;
    debug_vitc_failures = false;
    debug_store_thread_buffer = false;
}

Capture::~Capture()
{
    sv_close(sv);

    free(rec_ring);
    delete [] rec_8bit_video_ring;
    delete [] video_420;
    delete [] audio_16bitstereo;
    delete [] black_browse_video;
    delete [] silent_browse_audio;
    delete [] browse_frames[0].video;
    delete [] browse_frames[0].audio;
    delete [] browse_frames[1].video;
    delete [] browse_frames[1].audio;
    
    delete mxfout;
    delete frame_writer;
}

bool Capture::init_capture(int ringBufferSize)
{
    ring_buffer_size = ringBufferSize;

    logTF("Recorder version: date=%s %s\n", __DATE__, __TIME__);

    VideoMode required_videomode = (palff_mode ? PALFF_10BIT : PAL_10BIT);
    
    // get handle to SDI capture hardware: DVS card 0, input only
    if (!open_dvs(true, &sv))
        return false;

    // Check if video mode is set to required mode
    if (! check_videomode(sv, required_videomode)) {
        // Only change video mode if necessary (setting mode can take > 1 sec)
        
        // need to open input and output to set video mode for both
        sv_close(sv);
        if (!open_dvs(false, &sv))
            return false;
        if (! set_videomode(sv, required_videomode)) {
            logTF("Could not set video mode to %s\n", videomode2str(required_videomode));
            return false;
        }

        // back to input only
        sv_close(sv);
        if (!open_dvs(true, &sv))
            return false;
    }
    video_mode = required_videomode;
    logTF("Set video mode '%s'\n", videomode2str(video_mode));


    // get video dimensions
    sv_info status_info;
    sv_status(sv, &status_info);
    video_width = status_info.xsize;
    video_height = status_info.ysize;
    video_8bit_size = video_width * video_height * 2;
    video_size = (video_width + 5) / 6 * 16 * video_height;
    logTF("capture card opened: video raster %dx%d\n", video_width, video_height);

    // set size of one element in ring buffer = video size + audio size
    // each pair of interleaved SDI audio channels consumes 0x4000 bytes
    element_size = video_size + (int)((MAX_CAPTURE_AUDIO + 1) / 2) * 0x4000;

    // allocate ring buffer
    // sv internals need suitably aligned memory, so use valloc not malloc
    rec_ring = (uint8_t*)valloc(element_size * ring_buffer_size);
    if (rec_ring == NULL) {
        logTF("valloc failed\n");
        return false;
    }
    
    rec_8bit_video_ring = new uint8_t[ring_buffer_size * video_8bit_size];
    
    
    // reset the browse buffer overflow if it exceeds ring_buffer_size
    if (browse_overflow_frames > ring_buffer_size)
    {
        browse_overflow_frames = ring_buffer_size;
    }

    // initialise mutexes and condition variables
    pthread_mutex_init(&m_last_frame_captured, NULL);
    pthread_mutex_init(&m_last_frame_written, NULL);
    pthread_mutex_init(&m_frame_ready_cond, NULL);
    pthread_mutex_init(&m_browse_frame_ready_cond, NULL);
    pthread_mutex_init(&m_dropout_detector, NULL);
    pthread_mutex_init(&m_buffer_state, NULL);

    // initialise conditionals to use the monotonic clock
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    int res;
    if ((res = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) != 0)
    {
        logTF("Failed to set monotonic clock for conditional attribute: %s\n", strerror(res));
        return false;
    }
    else
    {
        if ((res = pthread_cond_init(&frame_ready_cond, &attr)) != 0)
        {
            logTF("Failed to initialise conditional: %s\n", strerror(res));
            return false;
        }
    }
    pthread_condattr_destroy(&attr);

    pthread_condattr_init(&attr);
    if ((res = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) != 0)
    {
        logTF("Failed to set monotonic clock for conditional attribute: %s\n", strerror(res));
        return false;
    }
    else
    {
        if ((res = pthread_cond_init(&browse_frame_ready_cond, &attr)) != 0)
        {
            logTF("Failed to initialise conditional: %s\n", strerror(res));
            return false;
        }
    }
    pthread_condattr_destroy(&attr);
    
    
    // Start the capture thread with realtime scheduling policy
    
#if defined(ENABLE_DEBUG)
    logTF("Warning: not setting realtime scheduling policy for capture thread\n");
    
    pthread_t capture_thread;
    res = pthread_create(&capture_thread, NULL, capture_video_thread_wrapper, this);
    if (res != 0) {
        logTF("Failed to create capture thread: %s\n", strerror(res));
        return false;
    }
#else
    pthread_attr_t thread_attr;
    struct sched_param thread_param;
    pthread_t capture_thread;
    uid_t previous_euid = geteuid();
    int min_priority = sched_get_priority_min(SCHED_RR);
    int max_priority = sched_get_priority_max(SCHED_RR);
    
    pthread_attr_init(&thread_attr);
    if (pthread_attr_setschedpolicy(&thread_attr, SCHED_RR) != 0)
    {
        logTF("Failed to set realtime scheduling policy: %s\n", strerror(errno));
        return false;
    }
    thread_param.sched_priority = (min_priority + max_priority) / 2;
    if (pthread_attr_setschedparam(&thread_attr, &thread_param) != 0)
    {
        logTF("Failed set realtime scheduling parameter: %s\n", strerror(errno));
        return false;
    }
    if (pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED) != 0)
    {
        logTF("Failed set explicit inherited scheduling: %s\n", strerror(errno));
        return false;
    }
    
    if (seteuid(0) != 0) // set effective user to root for call to pthread_create
    {
        logTF("Failed to set effective user to root: %s\n", strerror(errno));
        return false;
    }
    res = pthread_create(&capture_thread, &thread_attr, capture_video_thread_wrapper, this);
    if (seteuid(previous_euid) != 0)
    {
        logTF("Failed to restore effective user: %s\n", strerror(errno));
        return false;
    }
    pthread_attr_destroy(&thread_attr);

    if (res != 0) {
        logTF("Failed to create capture thread: %s\n", strerror(res));
        return false;
    }
#endif


    // start the store thread
    pthread_t   store_thread;
    if ((res = pthread_create(&store_thread, NULL, store_video_thread_wrapper, this)) != 0) {
        logTF("Failed to create store thread: %s\n", strerror(res));
        return false;
    }

    return true;
}

// debug methods
void Capture::save_video_line(int line_number, uint8_t *line)
{
    if (fp_saved_lines == NULL) {
        if ((fp_saved_lines = fopen("/video/saved_lines.uyvy", "wb")) == NULL) {
            logFF("fopen /video/saved_lines.uyvy failed");
            return;
        }
    }

    if (fwrite(line, 720*2, 1, fp_saved_lines) != 1) {
        logFF("fwrite line %d to saved_lines.uyvy", line_number);
    }
}

