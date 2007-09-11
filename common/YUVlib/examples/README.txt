Brief notes on Quad Split software
==================================


This directory contains software to make a "quad split" - up to four
quarter size pictures with some graphics on top. There are also some
auxiliary programs to display video files in a variety of formats.

The software started off using DV25 video files, decoded to YUY2 format.
It later evolved to use raw I420 files, at which point it aquired the
ability to superimpose text and timecode.

In its latest version it reads dv50 files and writes uncompressed planar
4:2:2 sampled YUV

The software uses routines from "YUVlib", a collection of routines to do common
image tasks on YUV data stored in planar or multiplexed form.


Source code
===========

xvdisplay_dv.c
	Sample code to display dv25 files.

display_quad.c
	First quad split program. Takes up to 4 dv25 files and displays
	result on screen.

planar_YUV_io.c planar_YUV_io.h
	Simple IO of frames of planar YUV data.

display_raw.c
	Takes uncompressed input (in YV12, IYUV/I420 or YUY2 format) and
	displays on computer monitor.

raw_I420_quad.c
	Third quad split program. Takes up to 4 uncompressed I420 inputs
	and writes uncompressed I420 result to stdout. This is the
	all-singing, all-dancing version with lots of options. Its usage
	is not entirely straightforward.

dv50_quad.c
	Fourth quad split program.
	Usage is similar to raw_I420_quad.c, but inputs are dv50 and
	output is uncompressed 4:2:2 YUV in planar (as opposed to
	multiplexed) format. This is variously called yuv422p or YV16.

raw_I420_quad and dv50_quad usage
=================================

raw_I420_quad [options] | display_raw

Options are:
-help		: Lists options
-interlace	: Treats input as interlaced. Default is to treat as
		  "film" motion, which looks better on a computer
		  display.
-hfiltered	: Horizontally filter video before subsampling.
		  Desirable.
-vfiltered	: Vertically filter video before subsampling.
		  Desirable.
-tl		: Selects "top left" quadrant.
-tr		: Selects "top right" quadrant.
-bl		: Selects "bottom left" quadrant.
-br		: Selects "bottom right" quadrant.
		  The selected quadrant affects the operation of all the
		  following options. This allows you to set the position
		  of each input and to choose the location of text
		  overlays.
-input name	: Gets video input from file "name".
-title text	: Sets quadrant's title to "text". E.g. MAIN or ISO1.
-skip n		: Skips first n frames of this input. Can be negative.
-count n	: Only process n frames of this input.
-timecode n	: Display timecode in this quadrant, starting at frame n.
-label text	: Display label in this quadrant. The label is set to
		  "text" which can be multiple line text. Word wrap is
		  automatic, but "\n" can be used to force a new line.
-alltext	: Put all text (titles, timecode and label) in this
		  quadrant, which must have no video input.
