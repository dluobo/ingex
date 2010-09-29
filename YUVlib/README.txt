Brief notes on YUVlib
=====================


This directory contains software to do stuff to YUV video frames in a variety
of formats: 4:2:2, 4:2:0 or 4:1:1 chrominance subsampling, planar or
multiplexed data and so on. (But 8-bit values only!)

The routines use a simple structure to hold parameters for each YUV component.
The general idea is that you don't copy the data from its "native" buffer
(DirectShow, ffmpeg or whatever) but just set pointers to the first sample of
each YUV component. This might not be as efficient as software that is specific
to the particular data layout you are using, but it's convenient to have
something more general.

Return values:
Many of the routines return an int to indicate the success or otherwise of the
operation. Unlike many other C language routines, failure is indicated by a
negative value and success by zero or a positive value. This allows some
routines to return a "count", for example the number of characters rendered in
the permitted width. The enumerated type YUV_error holds some likely error
codes.
