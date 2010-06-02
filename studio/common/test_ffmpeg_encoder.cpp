#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "VideoRaster.h"
#include "integer_types.h"
#include "ffmpeg_encoder.h"

static int usage(char *argv[])
{
    fprintf(stderr, "Usage: %s [-r res] input.yuv output.raw\n\n", argv[0]);
    fprintf(stderr, "    -b n_frames    read one frame then loop n_frames\n");
    fprintf(stderr, "    -r <res>       encoder resolution (DV25,DV50,IMX30,IMX40,IMX50,\n");
    fprintf(stderr, "                   DNX36p,DNX120p,DNX185p,DNX120i,DNX185i,\n");
    fprintf(stderr, "                   DV100_1080i50,DV100_720p50)\n");
    return 1;
}

static double tv_diff_now_us(const struct timeval* last)
{
    struct timeval *now, now_buf;
    now = &now_buf;
    gettimeofday(now, NULL);

    uint64_t diff = (now->tv_sec - last->tv_sec) * 1000000 + now->tv_usec - last->tv_usec;
    return (double)diff;
}

extern int main(int argc, char *argv[])
{
    FILE            *input_fp = NULL;
    FILE            *output_fp = NULL;
    uint8_t            *in, *out;
    Ingex::VideoRaster::EnumType raster = Ingex::VideoRaster::PAL;
    int                benchmark_encode = 0;
    int                n;
    MaterialResolution::EnumType res = MaterialResolution::NONE;

    n = 1;
    while (n < argc)
    {
        if (strcmp(argv[n], "-b") == 0)
        {
            benchmark_encode = atoi(argv[n+1]);
            if (benchmark_encode < 1)
                return usage(argv);
            n += 2;
        }
        /*
        Note that for some resoutions we use MXF_ATOM code because
        there isn't a RAW code.  We still produce a raw file.
        */
        if (strcmp(argv[n], "-r") == 0)
        {
            if (strcmp(argv[n+1], "DV25") == 0)
            {
                res = MaterialResolution::DV25_RAW;
                raster = Ingex::VideoRaster::PAL_B;
            }
            else if (strcmp(argv[n+1], "DV50") == 0)
            {
                res = MaterialResolution::DV50_RAW;
                raster = Ingex::VideoRaster::PAL_B;
            }
            else if (strcmp(argv[n+1], "IMX30") == 0)
            {
                res = MaterialResolution::IMX30_MXF_ATOM;
            }
            else if (strcmp(argv[n+1], "IMX40") == 0)
            {
                res = MaterialResolution::IMX40_MXF_ATOM;
            }
            else if (strcmp(argv[n+1], "IMX50") == 0)
            {
                res = MaterialResolution::IMX50_MXF_ATOM;
            }
            else if (strcmp(argv[n+1], "DNX36p") == 0)
            {
                res = MaterialResolution::DNX36P_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(argv[n+1], "DNX120p") == 0)
            {
                res = MaterialResolution::DNX120P_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(argv[n+1], "DNX185p") == 0)
            {
                res = MaterialResolution::DNX185P_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(argv[n+1], "DNX120i") == 0)
            {
                res = MaterialResolution::DNX120I_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(argv[n+1], "DNX185i") == 0)
            {
                res = MaterialResolution::DNX185I_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(argv[n+1], "DV100_1080i50") == 0)
            {
                res = MaterialResolution::DV100_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE274_25I;
            }
            else if (strcmp(argv[n+1], "DV100_720p50") == 0)
            {
                res = MaterialResolution::DV100_MXF_ATOM;
                raster = Ingex::VideoRaster::SMPTE296_50P;
            }
            n += 2;
            continue;
        }
        break;
    }

    if (benchmark_encode)
    {
        if ((argc - n) != 1)
        {
            return usage(argv);
        }
    }
    else
    {
        if ((argc - n) != 2)
        {
            return usage(argv);
        }
    }

    if (res == MaterialResolution::NONE)
    {
        return usage(argv);
    }

    // Get raster parameters
    int width;
    int height;
    int fps_num;
    int fps_den;
    Ingex::Interlace::EnumType interlace;
    Ingex::VideoRaster::GetInfo(raster, width, height, fps_num, fps_den, interlace);

    // Open input file
    if ( (input_fp = fopen(argv[n], "rb")) == NULL)
    {
        perror(argv[n]);
        return 1;
    }

    // Initialise ffmpeg encoder
    ffmpeg_encoder_t * ffmpeg_encoder = ffmpeg_encoder_init(res, raster, THREADS_USE_BUILTIN_TUNING);
    if (!ffmpeg_encoder)
    {
        fprintf(stderr, "ffmpeg encoder init failed\n");
        return 1;
    }

    int unc_frame_size = width*height*2;
    switch (res)
    {
    case MaterialResolution::DV25_RAW:
    case MaterialResolution::DV25_MXF_ATOM:
        unc_frame_size = width * height * 3 / 2;
        break;
    default:
        unc_frame_size = width * height * 2;
        break;
    }

    in = (uint8_t *)malloc(unc_frame_size);

    // Open output file
    if (! benchmark_encode)
    {
        if ( (output_fp = fopen(argv[n+1], "wb")) == NULL)
        {
            perror(argv[n+1]);
            return 1;
        }
    }

    int frames_encoded = 0;
    uint64_t total_size = 0;
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    while (1)
    {
        if (benchmark_encode && frames_encoded != 0)
        {
            if (frames_encoded == benchmark_encode)
            {
                break;
            }
        }
        else
        {
            if ( fread(in, unc_frame_size, 1, input_fp) != 1 )
            {
                if (feof(input_fp))
                {
                    break;
                }
    
                perror(argv[n]);
                return(1);
            }
        }

        int compressed_size = ffmpeg_encoder_encode(ffmpeg_encoder, in, &out);
        if (compressed_size < 0)
        {
            fprintf(stderr, "ffmpeg_encoder_encode() failed, frames_encoded = %d\n", frames_encoded);
            return(1);
        }

        if (! benchmark_encode)
        {
            if ( fwrite(out, compressed_size, 1, output_fp) != 1 )
            {
                perror(argv[n+1]);
                return(1);
            }
        }
        frames_encoded++;
        total_size += compressed_size;
    }

    printf("frames encoded = %d (%.2f sec), compressed size = %"PRIu64" (%.2fMbps), time = %.2f\n",
            frames_encoded, frames_encoded / 25.0,
            total_size, total_size / frames_encoded * 25.0 * 8 / 1000,
            tv_diff_now_us(&start_time) / 1000000.0);

    fclose(input_fp);
    if (! benchmark_encode)
    {
        fclose(output_fp);
    }

    free(in);
    ffmpeg_encoder_close(ffmpeg_encoder);

    return 0;
}

