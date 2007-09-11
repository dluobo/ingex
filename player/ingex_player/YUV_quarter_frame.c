#include "YUV_frame.h"
#include "YUV_quarter_frame.h"

// Sub sample a line with no filtering
static void half_line_alias(BYTE* srcLine, BYTE* dstLine,
                            const int inStride, const int outStride, const int w)
{
    int		i;

    for (i = 0; i < w / 2; i++)
    {
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inStride * 2;
    }
}

// Sub sample a line with (1,2,1)/4 filtering
static void half_line_121(BYTE* srcLine, BYTE* dstLine,
                          const int inStride, const int outStride, const int w)
{
    BYTE*	src0;
    BYTE*	src1;
    BYTE*	dst;
    int		i;
    BYTE	held;

    // Work backwards to allow for awkward end of line stuff
    src1 = srcLine + ((w - 1) * inStride);
    dst = dstLine + (((w / 2) - 1) * outStride);
    held = *src1;	// pre-load with edge value
    for (i = 0; i < w / 2; i++)
    {
        src0 = src1 - inStride;
        *dst = (*src0 + (*src1 * 2) + held) / 4;
        dst -= outStride;
        held = *src0;
        src1 = src0 - inStride;
    }
}

// Sub sample a line with (1,1)/2 filtering
static void half_line_11(BYTE* srcLine, BYTE* dstLine,
                         const int inStride, const int outStride, const int w)
{
    BYTE*	src0;
    BYTE*	src1;
    BYTE*	dst;
    int		i;

    src0 = srcLine;
    dst = dstLine;
    for (i = 0; i < w / 2; i++)
    {
        src1 = src0 + inStride;
        *dst = (*src0 + *src1) / 2;
        dst += outStride;
        src0 = src1 + inStride;
    }
}

// Do (1,2,1)/4 vertical filtering
static void v_fil_121(BYTE* inBuffA, BYTE* inBuffB,
                      BYTE* inBuffC, BYTE* outBuff,
                      const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + (*inBuffB++ * 2) + *inBuffC++) / 4;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + (*inBuffB * 2) + *inBuffC) / 4;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
            inBuffC += inStride;
        }
}

// Do (1,1)/2 vertical filtering
static void v_fil_11(BYTE* inBuffA, BYTE* inBuffB, BYTE* outBuff,
                     const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + *inBuffB++) / 2;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + *inBuffB) / 2;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
        }
}

typedef void half_line_proc(BYTE*, BYTE*, const int, const int, const int);

// Make a quarter size copy of inBuff in outBuff, top left corner at x, y
void quarter_121(BYTE* inBuff, BYTE* outBuff,
                 component* inFrame, component* outFrame,
                 const int v_fil, const int step,
                 half_line_proc* half_line,
                 BYTE** work)
{
    int		j;
    int		Nm1, N, Np1;

    Nm1 = 0;	// use first line twice (edge padding)
    N   = 0;
    Np1 = 1;
    for (j = 0; j < inFrame->h / (step * 2); j++)
    {
        if (v_fil)
        {
            half_line(inBuff, work[N], inFrame->pixelStride, 1, inFrame->w);
            inBuff += inFrame->lineStride * step;
            half_line(inBuff, work[Np1], inFrame->pixelStride, 1, inFrame->w);
            inBuff += inFrame->lineStride * step;
            v_fil_121(work[Nm1], work[N], work[Np1], outBuff,
                      1, outFrame->pixelStride, inFrame->w / 2);
            Nm1 =  Np1;
            N   = (Nm1 + 1) % 3;
            Np1 = (N   + 1) % 3;
        }
        else	// no v_fil
        {
            half_line(inBuff, outBuff,
                      inFrame->pixelStride, outFrame->pixelStride, inFrame->w);
            inBuff += inFrame->lineStride * step * 2;
        }
        outBuff += outFrame->lineStride * step;
    }
}

void quarter_11(BYTE* inBuff, BYTE* outBuff,
                component* inFrame, component* outFrame,
                const int v_fil, const int step,
                half_line_proc* half_line, BYTE** work)
{
    int			j;

    for (j = 0; j < inFrame->h / (step * 2); j++)
    {
        if (v_fil)
        {
            half_line(inBuff, work[0], inFrame->pixelStride, 1, inFrame->w);
            inBuff += inFrame->lineStride * step;
            half_line(inBuff, work[1], inFrame->pixelStride, 1, inFrame->w);
            inBuff += inFrame->lineStride * step;
            v_fil_11(work[0], work[1], outBuff,
                     1, outFrame->pixelStride, inFrame->w / 2);
        }
        else	// no v_fil
        {
            half_line(inBuff, outBuff,
                      inFrame->pixelStride, outFrame->pixelStride, inFrame->w);
            inBuff += inFrame->lineStride * step * 2;
        }
        outBuff += outFrame->lineStride * step;
    }
}

void quarter_frame(YUV_frame* in_frame, YUV_frame* out_frame,
                   int x, int y, int intlc, int hfil, int vfil,
                   void* workSpace)
{
    half_line_proc*	half_line_Y;
    half_line_proc*	half_line_UV;
    BYTE*		work[3];
    int			ssx, ssy;

    work[0] = workSpace;
    work[1] = work[0] + in_frame->Y.w;
    work[2] = work[1] + in_frame->Y.w;
    if (hfil)
    {
        half_line_Y = &half_line_121;
        half_line_UV = &half_line_11;
    }
    else
    {
        half_line_Y = &half_line_alias;
        half_line_UV = &half_line_alias;
    }
    ssx = in_frame->Y.w / in_frame->U.w;
    ssy = in_frame->Y.h / in_frame->U.h;
    if (intlc)
    {
        // "first" field i.e. "even" lines"
        quarter_121(in_frame->Y.buff,
                    out_frame->Y.buff + (y * out_frame->Y.lineStride) +
                                        (x * out_frame->Y.pixelStride),
                    &in_frame->Y, &out_frame->Y, vfil, 2, half_line_Y, work);
        quarter_121(in_frame->U.buff,
                    out_frame->U.buff + ((y/ssy) * out_frame->U.lineStride) +
                                        ((x/ssx) * out_frame->U.pixelStride),
                    &in_frame->U, &out_frame->U, vfil, 2, half_line_UV, work);
        quarter_121(in_frame->V.buff,
                    out_frame->V.buff + ((y/ssy) * out_frame->V.lineStride) +
                                        ((x/ssx) * out_frame->V.pixelStride),
                    &in_frame->V, &out_frame->V, vfil, 2, half_line_UV, work);
        // "second" field i.e. "odd" lines"
        quarter_11(in_frame->Y.buff + in_frame->Y.lineStride,
                   out_frame->Y.buff + ((y+1) * out_frame->Y.lineStride) +
                                           (x * out_frame->Y.pixelStride),
                   &in_frame->Y, &out_frame->Y, vfil, 2, half_line_Y, work);
        quarter_11(in_frame->U.buff + (in_frame->U.lineStride),
                   out_frame->U.buff + (((y/ssy)+1) * out_frame->U.lineStride) +
                                           ((x/ssx) * out_frame->U.pixelStride),
                   &in_frame->U, &out_frame->U, vfil, 2, half_line_UV, work);
        quarter_11(in_frame->V.buff + (in_frame->V.lineStride),
                   out_frame->V.buff + (((y/ssy)+1) * out_frame->V.lineStride) +
                                           ((x/ssx) * out_frame->V.pixelStride),
                   &in_frame->V, &out_frame->V, vfil, 2, half_line_UV, work);
    }
    else
    {
        quarter_121(in_frame->Y.buff,
                    out_frame->Y.buff + (y * out_frame->Y.lineStride) +
                                        (x * out_frame->Y.pixelStride),
                    &in_frame->Y, &out_frame->Y, vfil, 1, half_line_Y, work);
        quarter_121(in_frame->U.buff,
                    out_frame->U.buff + ((y/ssy) * out_frame->U.lineStride) +
                                        ((x/ssx) * out_frame->U.pixelStride),
                    &in_frame->U, &out_frame->U, vfil, 1, half_line_UV, work);
        quarter_121(in_frame->V.buff,
                    out_frame->V.buff + ((y/ssy) * out_frame->V.lineStride) +
                                        ((x/ssx) * out_frame->V.pixelStride),
                    &in_frame->V, &out_frame->V, vfil, 1, half_line_UV, work);
    }
}
