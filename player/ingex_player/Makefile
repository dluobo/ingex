include ../../common/libmxf.mk

PLAYER_INSTALL_PREFIX ?= /usr/local

COMMON_DIR = ../../common
YUVLIB_DIR = ../../YUVlib

# Use this flag to disable X11 support on systems without X11 installed, e.g.
# headless servers with locked down package management.
#DISABLE_X11_SUPPORT = 1

ifneq ($(MAKECMDGOALS),clean)
	ifneq ($(MAKECMDGOALS),install)
		CHECK_LIB_AVAILABILITIES=1
	endif
endif


ifdef CHECK_LIB_AVAILABILITIES

# dvs_sdk_paths.mk searches for sdk and sets DVS_HARDWARE_INCLUDE and DVS_HARDWARE_LIB
# search is overridden by setting DVSSDK
include $(COMMON_DIR)/dvs_sdk_paths.mk

ifndef DVS_HARDWARE_INCLUDE
  $(warning no DVS available - set the DVSSDK environment variable to build with DVS hardware support)
endif

ifndef DISABLE_FFMPEG
	FFMPEG_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg && echo $$f && break; test -e $$f/libavcodec && echo $$f && break; done)
	ifeq "$(FFMPEG_INCLUDE_PATH)" ""
$(warning no FFMPEG library found - DV stream connector not available)
	endif
endif

ifndef DISABLE_HTTP_ACCESS
	SHTTPD_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/shttpd.h && echo $$f && break; done)
	ifeq "$(SHTTPD_PATH)" ""
$(warning no shttpd library found - http access not available)
	endif
endif

ifndef DISABLE_SHARED_MEM_SOURCE
	TEST_SHM_PATH := $(shell for f in $(COMMON_DIR)/nexus_control.h; do test -e $$f && echo $$f && break; done)
	ifeq "$(TEST_SHM_PATH)" ""
$(warning shared memory source disabled)
	endif
endif

ifndef DISABLE_UDP_SOURCE
	TEST_UDP_PATH := $(shell for f in $(COMMON_DIR)/multicast_video.h; do test -e $$f && echo $$f && break; done)
	ifeq "$(TEST_UDP_PATH)" ""
$(warning udp source disabled)
	endif
endif

PORTAUDIO_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/portaudio.h && echo $$f && break; done)
ifneq "$(PORTAUDIO_INCLUDE_PATH)" ""
HAVE_PORTAUDIO = 1
else
$(warning no PortAudio library found - No PC audio playback available)
endif

ifdef ENABLE_SDL_SINK
    SDL_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/SDL/SDL.h && echo $$f && break; done)
    ifneq "$(SDL_INCLUDE_PATH)" ""
    HAVE_SDL = 1
    else
$(warning no SDL library found - SDL sink not available)
    endif
endif

endif


# -Werror removed since current ffmpeg header give "AVFrac is deprecated"
# warnings when compiling dv_stream_connect.c
CFLAGS = -Wall -W -Wno-unused-parameter -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
OPT = -O3 -mmmx -msse2

INCLUDES = -I. -I/usr/local/include -I/usr/include/freetype2 -I/usr/include/fontconfig \
	$(LIBMXF_INCLUDE) $(ARCHIVEMXFINFO_INCLUDE) $(MXFREADER_INCLUDE)

LIBS = -L. -L/usr/X11R6/lib -L/usr/local/lib -L/usr/X11R6/lib64 -L/usr/local/lib64 \
	$(ARCHIVEMXFINFO_LIB) $(MXFREADER_LIB) $(LIBMXF_LIB) \
	-lpthread -lfreetype -lfontconfig

ifdef DISABLE_QC_LTO_ACCESS
	CFLAGS += -DDISABLE_QC_LTO_ACCESS
else
	INOTIFY_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/sys/inotify.h && echo $$f && break; done)
	ifeq "$(INOTIFY_INCLUDE_PATH)" ""
$(warning sys/inotify.h not found - building QC player without LTO access)
	CFLAGS += -DDISABLE_QC_LTO_ACCESS
	endif
endif

ifdef DVS_HARDWARE_INCLUDE
  INCLUDES += $(DVS_HARDWARE_INCLUDE)
  LIBS += $(DVS_HARDWARE_LIB)
  CFLAGS += -DHAVE_DVS
endif

ifdef FFMPEG_INCLUDE_PATH

	CFLAGS += -DHAVE_FFMPEG

	# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
	FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
	ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
		CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
	endif

	# Check for swscale
	FFMPEG_SWSCALE_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/swscale.h && echo YES && break; test -e $$f/libswscale/swscale.h && echo YES && break; done)
	ifeq "$(FFMPEG_SWSCALE_INCLUDE_PATH)" "YES"
		CFLAGS += -DHAVE_FFMPEG_SWSCALE

		FFMPEG_SWSCALE_LIB := $(shell for f in /usr/local/lib /usr/lib /usr/local/lib64 /usr/lib64; do test -e $$f/libswscale.a && echo YES && break; done)
		ifeq "$(FFMPEG_SWSCALE_LIB)" "YES"
			LIBS += -lswscale
		endif
	endif

	LIBS += -lavformat -lavcodec -lavutil -lm -lz -lbz2

	# Add additional library if we find support for mp3lame
	FFMPEG_HAS_MP3LAME := $(shell ffmpeg 2>&1 | grep -q enable-libmp3lame && echo YES)
	ifeq "$(FFMPEG_HAS_MP3LAME)" "YES"
		LIBS += -lmp3lame
	endif
	
	# Add additional library if we find support for h264 in ffmpeg
	FFMPEG_HAS_H264 := $(shell ffmpeg 2>&1 | grep -q enable-libx264 && echo YES)
	ifeq "$(FFMPEG_HAS_H264)" "YES"
		LIBS += -lx264
	endif
	
	# Add additional libraries if we find support for AAC in ffmpeg
	FFMPEG_HAS_AAC := $(shell ffmpeg 2>&1 | grep -q enable-libfaac && echo YES)
	ifeq "$(FFMPEG_HAS_AAC)" "YES"
		LIBS += -lfaac -lfaad
	endif
	
	# Add additional libraries and set flag if ffmpeg supports DNxHD
	FFMPEG_HAS_DNXHD := $(shell test `ffmpeg -formats 2>/dev/null | grep DNxHD | wc -l` -gt 1 && echo YES)
	ifeq "$(FFMPEG_HAS_DNXHD)" "YES"
		CFLAGS += -DHAVE_DNXHD
	endif
else
	CFLAGS += -Werror
endif

ifdef SHTTPD_PATH
	INCLUDES += -I$(SHTTPD_PATH)/include
	LIBS += -L$(SHTTPD_PATH)/lib -lshttpd
	CFLAGS += -DHAVE_SHTTPD
else
	ifdef SHTTPD_INSTALL
		INCLUDES += -I$(SHTTPD_INSTALL)/src
		LIBS += -L$(SHTTPD_INSTALL)/src -lshttpd
		CFLAGS += -DHAVE_SHTTPD
	endif
endif

ifndef TEST_SHM_PATH
	CFLAGS += -DDISABLE_SHARED_MEM_SOURCE
endif

ifndef TEST_UDP_PATH
	CFLAGS += -DDISABLE_UDP_SOURCE
else
	INCLUDES += -I$(COMMON_DIR) -I$(YUVLIB_DIR)
	LIBS += -L$(COMMON_DIR) -L$(YUVLIB_DIR) -lcommon -lYUVlib
endif

ifdef HAVE_PORTAUDIO
	INCLUDES += -I$(PORTAUDIO_INCLUDE_PATH)
	LIBS += -lportaudio
	CFLAGS += -DHAVE_PORTAUDIO
endif

ifdef HAVE_SDL
	LIBS += -lSDL
	CFLAGS += -DHAVE_SDL
endif

ifdef DISABLE_X11_SUPPORT
	CFLAGS += -DDISABLE_X11_SUPPORT
else
	LIBS += -lX11 -lXv
endif


CC = g++ -g $(CFLAGS) $(OPT)
AR = ar cr

SOURCES = audio_level_sink.c \
	audio_sink.c \
	audio_switch_sink.c \
	blank_source.c \
	bouncing_ball_source.c \
	buffered_media_sink.c \
	buffered_media_source.c \
	clapper_source.c \
	clapper_source_data.c \
	clip_source.c \
	connection_matrix.c \
	console_monitor.c \
	dnxhd_stream_connect.c \
	dvs_sink.c \
	dv_stream_connect.c \
	ffmpeg_source.c \
	frame_info.c \
	frame_sequence_sink.c \
	half_split_sink.c \
	http_access.c \
	http_access_resources.c \
	keyboard_input.c \
	keyboard_input_connect.c \
	logging.c \
	media_control.c \
	media_player.c \
	media_player_marks.c \
	media_sink.c \
	media_sink_frame.c \
	media_source.c \
	mjpeg_stream_connect.c \
	mouse_input.c \
	mouse_input_connect.c \
	mov_helper.c \
	mpegi_stream_connect.c \
	multiple_sources.c \
	mxf_source.c \
	null_sink.c \
	on_screen_display.c \
	osd_symbols.c \
	overlay.c \
	picture_scale_sink.c \
	progress_bar_input.c \
	progress_bar_input_connect.c \
	qc_http_access.c \
	qc_lto_access.c \
	qc_lto_extract.c \
	qc_session.c \
	raw_dv_source.c \
	raw_file_sink.c \
	raw_file_source.c \
	sdl_sink.c \
	shared_mem_source.c \
	shuttle_input.c \
	shuttle_input_connect.c \
	source_event.c \
	stream_connect.c \
	system_timecode_source.c \
	term_keyboard_input.c \
	udp_source.c \
	utils.c \
	video_conversion.c \
	video_switch_database.c \
	video_switch_sink.c \
	vitc_reader_sink_source.c

ifndef DISABLE_X11_SUPPORT
	SOURCES += dual_sink.c \
		emulate_key.c \
		x11_common.c \
		x11_display_sink.c \
		x11_xv_display_sink.c
endif


OBJECTS = $(patsubst %.c,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.c,.deps/%.d,$(SOURCES))


.PHONY: all
all: libingexplayer.a player qc_player


libingexplayer.a: $(OBJECTS)
	$(AR) libingexplayer.a $(OBJECTS)


.deps/%.d : %.c
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.c
	@mkdir -p .objs
	$(CC) -c $(INCLUDES) $< -o $@



.deps/player.d : player.c
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

player: .objs/player.o libingexplayer.a
	$(CC) -c $(INCLUDES) version.c -o .objs/version.o
	$(CC) .objs/player.o .objs/version.o libingexplayer.a $(LIBS) -o $@

.deps/qc_player.d : qc_player.c
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

qc_player: .objs/qc_player.o libingexplayer.a
	$(CC) -c $(INCLUDES) version.c -o .objs/version.o
	$(CC) .objs/qc_player.o .objs/version.o libingexplayer.a $(LIBS) -o $@



TESTAPPS += test_shuttle_input
test_shuttle_input: .objs/test_shuttle_input.o .objs/shuttle_input.o .objs/logging.o
	$(CC) $(INCLUDES) .objs/test_shuttle_input.o .objs/shuttle_input.o .objs/logging.o -o test_shuttle_input

TESTAPPS += test_emulate_key
test_emulate_key: .objs/test_emulate_key.o .objs/emulate_key.o .objs/logging.o
	$(CC) $(INCLUDES) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/test_emulate_key.o .objs/emulate_key.o .objs/logging.o -o test_emulate_key

TESTAPPS += test_media_control
test_media_control: .objs/test_media_control.o .objs/media_control.o .objs/shuttle_input_connect.o .objs/shuttle_input.o .objs/x11_keyboard_input.o .objs/term_keyboard_input.o .objs/logging.o
	$(CC) $(INCLUDES) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/test_media_control.o .objs/media_control.o .objs/shuttle_input_connect.o .objs/shuttle_input.o .objs/x11_keyboard_input.o .objs/term_keyboard_input.o .objs/logging.o -o test_media_control

TESTAPPS += ffmpeg_shuttle_emu
ffmpeg_shuttle_emu: .objs/ffmpeg_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o
	$(CC) $(INCLUDES) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/ffmpeg_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o -o ffmpeg_shuttle_emu

TESTAPPS += recorder_shuttle_emu
recorder_shuttle_emu: .objs/recorder_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o .objs/utils.o
	$(CC) $(INCLUDES) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/recorder_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o .objs/utils.o -lm -lXtst -lpthread -o recorder_shuttle_emu

TESTAPPS += test_video_switch_database
test_video_switch_database: .objs/test_video_switch_database.o test_video_switch_database.c
	$(CC) $(INCLUDES) .objs/test_video_switch_database.o .objs/video_switch_database.o .objs/logging.o -o $@



.PHONY: install
install: player qc_player
	cp player $(PLAYER_INSTALL_PREFIX)/bin
	cp qc_player $(PLAYER_INSTALL_PREFIX)/bin

.PHONY: uninstall
uninstall:
	@rm -f $(PLAYER_INSTALL_PREFIX)/bin/player
	@rm -f $(PLAYER_INSTALL_PREFIX)/bin/qc_player



.PHONY: clean
clean:
	@rm -f *~ *.a player qc_player $(TESTAPPS)
	@rm -Rf .objs
	@rm -Rf .deps


ifneq ($(MAKECMDGOALS),clean)
-include $(DEPENDENCIES)
-include .deps/player.d
-include .deps/qc_player.d
endif

