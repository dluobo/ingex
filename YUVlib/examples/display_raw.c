#include <stdlib.h>
#include <stdio.h>
#include <string.h>     /* strcmp() */
#include <sys/shm.h>    /* shmget(), shmctl() */
#include <unistd.h>     /* usleep() */
#include <sys/time.h>   /* gettimeofday() */
#include <getopt.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

//#include <libdv/dv_types.h>

// little-endian specific
#define uint32_t unsigned int
#define VLC_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )

#define X11_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )
#define VLC2X11_FOURCC( i ) \
        X11_FOURCC( ((char *)&i)[0], ((char *)&i)[1], ((char *)&i)[2], \
                    ((char *)&i)[3] )
#define X112VLC_FOURCC( i ) \
        VLC_FOURCC( i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff, \
                    (i >> 24) & 0xff )


static int XVideoGetPort( Display *p_display,
                          int32_t i_chroma,
                          int i_requested_adaptor)      // -1 for autoscan
{
    XvAdaptorInfo *p_adaptor;
    unsigned int i;
    unsigned int i_adaptor, i_num_adaptors;
    int i_selected_port;

    switch( XvQueryExtension( p_display, &i, &i, &i, &i, &i ) )
    {
        case Success:
            break;

        case XvBadExtension:
            fprintf(stderr, "XvBadExtension\n" );
            return -1;

        case XvBadAlloc:
            fprintf(stderr, "XvBadAlloc\n" );
            return -1;

        default:
            fprintf(stderr, "XvQueryExtension failed\n" );
            return -1;
    }

    switch( XvQueryAdaptors( p_display,
                             DefaultRootWindow( p_display ),
                             &i_num_adaptors, &p_adaptor ) )
    {
        case Success:
            break;

        case XvBadExtension:
            fprintf(stderr, "XvBadExtension for XvQueryAdaptors\n" );
            return -1;

        case XvBadAlloc:
            fprintf(stderr, "XvBadAlloc for XvQueryAdaptors\n" );
            return -1;

        default:
            fprintf(stderr, "XvQueryAdaptors failed\n" );
            return -1;
    }

    i_selected_port = -1;

    for( i_adaptor = 0; i_adaptor < i_num_adaptors; ++i_adaptor )
    {
        XvImageFormatValues *p_formats;
        int i_format, i_num_formats;
        int i_port;

        /* If we requested an adaptor and it's not this one, we aren't
         * interested */
        if( i_requested_adaptor != -1 && i_adaptor != i_requested_adaptor )
        {
            continue;
        }

        /* If the adaptor doesn't have the required properties, skip it */
        if( !( p_adaptor[ i_adaptor ].type & XvInputMask ) ||
            !( p_adaptor[ i_adaptor ].type & XvImageMask ) )
        {
            continue;
        }

        /* Check that adaptor supports our requested format... */
        p_formats = XvListImageFormats( p_display,
                                        p_adaptor[i_adaptor].base_id,
                                        &i_num_formats );

        for( i_format = 0;
             i_format < i_num_formats && ( i_selected_port == -1 );
             i_format++ )
        {
            XvAttribute     *p_attr;
            int             i_attr, i_num_attributes;

            /* Matching chroma? */
            if( p_formats[ i_format ].id != i_chroma )
            {
                continue;
            }

            /* Look for the first available port supporting this format */
            for( i_port = p_adaptor[i_adaptor].base_id;
                 ( i_port < (int)(p_adaptor[i_adaptor].base_id
                                   + p_adaptor[i_adaptor].num_ports) )
                   && ( i_selected_port == -1 );
                 i_port++ )
            {
                if( XvGrabPort( p_display, i_port, CurrentTime )
                     == Success )
                {
                    i_selected_port = i_port;
                }
            }

            /* If no free port was found, forget it */
            if( i_selected_port == -1 )
            {
                continue;
            }

            /* If we found a port, print information about it */
            fprintf( stderr, "adaptor %i, port %i, format 0x%x (%4.4s) %s\n",
                     i_adaptor, i_selected_port, p_formats[ i_format ].id,
                     (char *)&p_formats[ i_format ].id,
                     ( p_formats[ i_format ].format == XvPacked ) ?
                         "packed" : "planar" );

            /* Make sure XV_AUTOPAINT_COLORKEY is set */
            p_attr = XvQueryPortAttributes( p_display,
                                            i_selected_port,
                                            &i_num_attributes );

            for( i_attr = 0; i_attr < i_num_attributes; i_attr++ )
            {
                if( !strcmp( p_attr[i_attr].name, "XV_AUTOPAINT_COLORKEY" ) )
                {
                    const Atom autopaint =
                        XInternAtom( p_display,
                                     "XV_AUTOPAINT_COLORKEY", False );
                    XvSetPortAttribute( p_display,
                                        i_selected_port, autopaint, 1 );
                    break;
                }
            }

            if( p_attr != NULL )
            {
                XFree( p_attr );
            }
        }

        if( p_formats != NULL )
        {
            XFree( p_formats );
        }

    }

    if( i_num_adaptors > 0 )
    {
        XvFreeAdaptorInfo( p_adaptor );
    }

    if( i_selected_port == -1 )
    {
        int i_chroma_tmp = X112VLC_FOURCC( i_chroma );
        if( i_requested_adaptor == -1 )
        {
            fprintf(stderr, "no free XVideo port found for format\n"
                      "0x%.8x (%4.4s)", i_chroma_tmp, (char*)&i_chroma_tmp );
        }
        else
        {
            fprintf(stderr, "XVideo adaptor %i does not have a free\n"
                      "XVideo port for format 0x%.8x (%4.4s)",
                      i_requested_adaptor, i_chroma_tmp, (char*)&i_chroma_tmp );
        }
    }

    return i_selected_port;
}

static void XVideoReleasePort( Display *p_display, int i_port )
{
    XvUngrabPort( p_display, i_port, CurrentTime );
}

static int  average_sleep = 0;
static void sleep_diff(long delay, const struct timeval *now, const struct timeval *last)
{
    long diff_usec;

    diff_usec = (now->tv_sec - last->tv_sec) * 1000000 +
                now->tv_usec - last->tv_usec;
    average_sleep += ((delay - diff_usec) - average_sleep) / 100;

    if (diff_usec < delay)          // E.g. 0.040 seconds == 25 fps
    {
        usleep( delay - diff_usec );
    }
}

int main(int argc, char *argv[])
{
    int             xvport;
    Display*            display;
    Window          window;
    XSetWindowAttributes    x_attr;
    GC              gc;
    XEvent          event;
    int             pic_width = 720, pic_height = 576;
    int             dsply_height = pic_height;
    int             dsply_width = dsply_height * 16 / 9;
    XvImage*        yuv_image = NULL;
    char*           frame_buff = NULL;
    int             frame_size;
    int32_t         frame_format;
    struct timeval  now_time, last_frame_time;
    char*           in_name = "stdin";
    FILE*           in_fd = stdin;

    frame_format = X11_FOURCC('I','4','2','0');
    // get command line options
    {
        int c;
        struct option long_options[] =
        {
            {"help",      0, NULL, '?'},
            {"YV12",      0, NULL, '0'},
            {"IYUV",      0, NULL, '1'},
            {"I420",      0, NULL, '1'},
            {"YUY2",      0, NULL, '2'},
            {0, 0, 0, 0}
        };
        while (1)
        {
            c = getopt_long_only(argc, argv, "", long_options, NULL);
            if (c == -1)
                break;
            switch (c)
            {
                case '0':
                    frame_format = X11_FOURCC('Y','V','1','2');
                    break;
                case '1':
                    frame_format = X11_FOURCC('I','4','2','0');
                    break;
                case '2':
                    frame_format = X11_FOURCC('Y','U','Y','2');
                    break;
                case '?':
                    fprintf(stderr, "usage: %s", argv[0]);
                    int i;
                    for (i = 1; long_options[i].name != 0; i++)
                        fprintf(stderr, " [-%s]", long_options[i].name);
                    fprintf(stderr, " [raw_file]\n");
                    exit(1);
                default:
                    printf ("?? getopt returned character code 0%o ??\n", c);
            }
        }
    }

    if (optind == argc - 1)
    {
        in_name = argv[optind];
        in_fd = fopen(in_name, "rb");
        if (in_fd == NULL)
            perror("fopen");
    }

    if ((display = XOpenDisplay(NULL)) == NULL)
    {
        fprintf(stderr, "Cannot open Display.\n");
        return 1;
    }

    /* Check that we have access to an XVideo port providing this chroma    */
    /* Commonly supported chromas: YV12, I420, YUY2, YUY2                   */
    xvport = XVideoGetPort( display, frame_format, -1);
    if ( xvport < 0 )
    {
        fprintf(stderr, "Cannot find an xv port for requested video format\n");
        return 1;
    }

    /* Setup X parameters */
    x_attr.backing_store = Always;
    x_attr.background_pixel = 0;
    x_attr.event_mask = ExposureMask | StructureNotifyMask;

    window = XCreateWindow(display, DefaultRootWindow(display),
                0, 0, dsply_width, dsply_height,
                0, 0, InputOutput, 0,
                CWBackingStore | CWBackPixel | CWEventMask, &x_attr);
    XStoreName(display, window, in_name);
    XSelectInput(display, window, StructureNotifyMask);
    XMapWindow(display, window);

    do {
        XNextEvent(display, &event);
    } while (event.type != MapNotify || event.xmap.event != window);

    gc = XCreateGC(display, window, 0, 0);

    // Allocate video frame
    switch (frame_format)
    {
        case X11_FOURCC('Y','U','Y','2'):
            frame_size = pic_width * pic_height * 2;
            break;
        case X11_FOURCC('Y','V','1','2'):
        case X11_FOURCC('I','4','2','0'):
        case X11_FOURCC('I','Y','U','V'):
            frame_size = (pic_width * pic_height) +
                         (pic_width * pic_height / 2);
            break;
        default:
            frame_size = pic_width * pic_height * 2;
    }
    frame_buff = malloc(frame_size);
    if (frame_buff == NULL)
    {
        perror("malloc");
        exit(1);
    }

    // Create display image linked to video frame
    yuv_image = XvCreateImage(display, xvport, frame_format,
                              frame_buff, pic_width, pic_height);

    gettimeofday(&last_frame_time, NULL);

    while (1)
    {
        gettimeofday(&now_time, NULL);
        // Read an input frame
        if (fread(frame_buff, frame_size, 1, in_fd) != 1)
        {
            if (feof(in_fd))
            {
                fprintf(stderr, "end of input\n");
                break;
            }
            else
            {
                perror("reading input");
                return 1;
            }
        }

        // Display on xv port
        XvPutImage(display, xvport, window, gc, yuv_image,
            0, 0, pic_width, pic_height,    // source w,h
            0, 0, dsply_width, dsply_height);   // scaled to w.h
        XSync(display, False);

        // Display frame 0.040 seconds after last frame displayed
        sleep_diff(40*1000, &now_time, &last_frame_time);   // 0.040 seconds
        last_frame_time = now_time;
    }

    XVideoReleasePort(display, xvport);
    fprintf(stderr, "Average sleep = %d\n", average_sleep);
    return 0;
}
