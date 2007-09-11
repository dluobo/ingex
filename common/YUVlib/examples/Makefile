TARGETS		:=	dv50_quad \
			raw_I420_quad \
			display_raw \

CFLAGS		+= -O3 -mmmx -msse2 -Wall -g -D_FILE_OFFSET_BITS=64 \
		   -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
CPPFLAGS	+= -I.. \
		   -I/usr/include/freetype2 \
		   -I/usr/include/fontconfig \
		   -I/usr/local/include/ffmpeg
LDFLAGS		+= -L.. -L/usr/X11R6/lib
LDLIBS		+= -lYUVlib -lXv -lXext -lX11 \
			-lfreetype -lfontconfig -lavcodec -lavformat

.PHONY : all clean test test2

all : $(TARGETS)

clean :
	rm -f $(TARGETS) *.o core core.*

TEST_OPTS	:= -alltext -interlace -hfil -vfil

#DV50_SEQ	:= /dp_videoedit/nymans10_dv50.dv
DV50_SEQ	:= /dp_videoedit/bear_fcp_dvcpro50.dv

RAW_SEQ		:= /dp_videoedit/bamzooki/boxing/iso.yuv

test : dv50_quad
	./dv50_quad \
		-tl -input $(DV50_SEQ) -title "MAIN" \
		-bl -input $(DV50_SEQ) -title "ISO1" -skip -50 \
		-br -input $(DV50_SEQ) -title "ISO2" -skip -75 \
		-tr -timecode $$[25 * 3600 * 10] \
		-label "Simple test\nof new quad split" \
		$(TEST_OPTS) | \
	ffmpeg -s 720x576 -pix_fmt yuv422p -f rawvideo -i - \
		-target dv50 - | \
	ffplay -f dv -

test2 : raw_I420_quad
	./raw_I420_quad \
		-tl -input $(RAW_SEQ) -title "MAIN" -skip 200 \
		-bl -input $(RAW_SEQ) -title "ISO1" -skip 175 \
		-br -input $(RAW_SEQ) -title "ISO2" -skip 125 \
		-tr -timecode $$[25 * 3600 * 10] \
		-label "Simple test\nof new quad split" \
		$(TEST_OPTS) | \
	ffmpeg -s 720x576 -pix_fmt yuv420p -f rawvideo -i - \
		-target dv50 - | \
	ffplay -f dv -

dv50_quad : dv50_quad.o planar_YUV_io.o

raw_I420_quad : raw_I420_quad.o planar_YUV_io.o
