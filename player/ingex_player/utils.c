/*
 * $Id: utils.c,v 1.7 2010/08/27 17:41:32 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Author: Stuart Cunningham
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>     /* usleep() */
#include <time.h>
#include <math.h>
#include <ctype.h>

#include "utils.h"
#include "logging.h"
#include "macros.h"


long sleep_diff(long delay, const struct timeval* now, const struct timeval* last)
{
    long diff_usec;

    diff_usec = (now->tv_sec - last->tv_sec) * 1000000 + now->tv_usec - last->tv_usec;

    if (diff_usec > 0 && diff_usec < delay)
    {
        usleep(delay - diff_usec);
        return delay - diff_usec;
    }
    else
    {
        //fprintf(stderr, "Missed frame point\n");
        if (diff_usec < 0)
        {
            return 0;
        }
        return delay - diff_usec;
    }
}



void print_timecode(TimecodeType type, TimecodeSubType subType, const Rational* frameRate, const Timecode* timecode)
{
    if (type == CONTROL_TIMECODE_TYPE)
    {
        printf("[C");
    }
    else if (type == SOURCE_TIMECODE_TYPE)
    {
        printf("[S");
    }
    else if (type == SYSTEM_TIMECODE_TYPE)
    {
        printf("[X");
    }
    else
    {
        printf("[?");
    }
    if (subType == VITC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-VITC");
    }
    else if (subType == LTC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-LTC");
    }
    else if (subType == DVITC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-DVITC");
    }
    else if (subType == DLTC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-DLTC");
    }
    printf("] ");
    if (timecode->isDropFrame)
    {
        printf("%02u:%02u:%02u;%02u", timecode->hour, timecode->min, timecode->sec, timecode->frame);
    }
    else
    {
        printf("%02u:%02u:%02u:%02u", timecode->hour, timecode->min, timecode->sec, timecode->frame);
    }
}

void get_timestamp_string(char* timestampStr)
{
    time_t t;
    struct tm* ltm;

    if (time(&t) == ((time_t)-1))
    {
        perror("time");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }
    if ((ltm = localtime(&t)) == NULL)
    {
        fprintf(stderr, "localtime() failed\n");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }

    sprintf(timestampStr, "%d-%02d-%02d %02d:%02d:%02d", ltm->tm_year + 1900,
        ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);

}

void get_short_timestamp_string(char* timestampStr)
{
    time_t t;
    struct tm* ltm;

    if (time(&t) == ((time_t)-1))
    {
        perror("time");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }
    if ((ltm = localtime(&t)) == NULL)
    {
        fprintf(stderr, "localtime() failed\n");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }

    sprintf(timestampStr, "%d%02d%02d_%02d%02d%02d", ltm->tm_year + 1900,
        ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
}

int64_t get_timecode_now(int roundedFrameRate)
{
    time_t t;
    struct tm* ltm;

    if (time(&t) == ((time_t)-1))
    {
        return 0;
    }
    if ((ltm = localtime(&t)) == NULL)
    {
        return 0;
    }

    return ltm->tm_hour * 60 * 60 * roundedFrameRate + ltm->tm_min * 60 * roundedFrameRate + ltm->tm_sec * roundedFrameRate;
}

int init_mutex(pthread_mutex_t* mutex)
{
    int err;

    if ((err = pthread_mutex_init(mutex, NULL)) != 0)
    {
        ml_log_error("Failed to initialise mutex: %s\n", strerror(err));
        return 0;
    }
    return 1;
}

int init_cond_var(pthread_cond_t* cond)
{
    int err;

    if ((err = pthread_cond_init(cond, NULL)) != 0)
    {
        ml_log_error("Failed to initialise conditional variable: %s\n", strerror(err));
        return 0;
    }
    return 1;
}

void destroy_mutex(pthread_mutex_t* mutex)
{
    int err;

    if (mutex == NULL)
    {
        return;
    }

	if ((err = pthread_mutex_destroy(mutex)) != 0)
    {
        ml_log_warn("Failed to destroy mutex: %s\n", strerror(err));
    }
}

void destroy_cond_var(pthread_cond_t* cond)
{
    int err;

    if (cond == NULL)
    {
        return;
    }

    if ((err = pthread_cond_destroy(cond) != 0))
    {
        ml_log_warn("Failed to destroy conditional variable: %s\n", strerror(err));
    }
}


int create_joinable_thread(pthread_t* thread, void* (*start_func)(void*), void* arg)
{
    int result;
    pthread_attr_t attr;
    pthread_t newThread;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    result = pthread_create(&newThread, &attr, start_func, arg);
    if (result == 0)
    {
        *thread = newThread;
    }
    else
    {
        ml_log_error("Failed to create joinable thread: %s\n", strerror(result));
    }

    pthread_attr_destroy(&attr);

    return result == 0;
}

void cancel_and_join_thread(pthread_t* thread, void* data, void (*stop_func)(void*))
{
    int result;
    void* status;

    if (*thread != 0)
    {
        if (stop_func != NULL)
        {
            stop_func(data);
        }

        if ((result = pthread_cancel(*thread)) != 0)
        {
            ml_log_warn("Failed to cancel thread: %s\n", strerror(result));
        }

        if ((result = pthread_join(*thread, (void **)&status)) != 0)
        {
            ml_log_warn("Failed to join thread: %s\n", strerror(result));
        }
        *thread = 0;
    }
}

void join_thread(pthread_t* thread, void* data, void (*stop_func)(void*))
{
    int result;
    void* status;

    if (*thread != 0)
    {
        if (stop_func != NULL)
        {
            stop_func(data);
        }

        if ((result = pthread_join(*thread, (void **)&status)) != 0)
        {
            ml_log_warn("Failed to join thread: %s\n", strerror(result));
        }
        *thread = 0;
    }
}


double calc_audio_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power)
{
    int i;
    double squareSum = 0.0;
    double max_audio_level = 1.0;
    int8_t sample8;
    int16_t sample16;
    int32_t sample32;
    double power;

    switch (byte_alignment)
    {
        case 1:
            for (i = 0; i < num_samples; i++)
            {
                sample8 = (int8_t)p_samples[i];

                squareSum += (double)sample8 * (double)sample8;
            }
            max_audio_level = 128; /* max signed 8-bit */
            break;
        case 2:
            for (i = 0; i < num_samples; i++)
            {
                sample16 = (int16_t)(
                    (uint16_t)p_samples[i * 2] |
                    (((uint16_t)p_samples[i * 2 + 1]) << 8));

                squareSum += (double)sample16 * (double)sample16;
            }
            max_audio_level = 32768; /* max signed 16-bit */
            break;
        case 3:
            for (i = 0; i < num_samples; i++)
            {
                sample32 = (int32_t)(
                    (((uint32_t)p_samples[i * 3]) << 8) |
                    (((uint32_t)p_samples[i * 3 + 1]) << 16) |
                    (((uint32_t)p_samples[i * 3 + 2]) << 24));

                squareSum += (double)sample32 * (double)sample32;
            }
            max_audio_level = 2.0*1024*1024*1024; /* max signed 32-bit */
            break;
        case 4:
            for (i = 0; i < num_samples; i++)
            {
                sample32 = (int32_t)(
                    (uint32_t)p_samples[i * 4] |
                    (((uint32_t)p_samples[i * 4 + 1]) << 8) |
                    (((uint32_t)p_samples[i * 4 + 2]) << 16) |
                    (((uint32_t)p_samples[i * 4 + 3]) << 24));

                squareSum += (double)sample32 * (double)sample32;
            }
            max_audio_level = 2.0*1024*1024*1024; /* max signed 32-bit */
            break;
        default:
            /* not supported */
            squareSum = 0.0;
            max_audio_level = 1.0;
            break;
    }

    /* return min_power - 1 if no audio is present */
    if (squareSum <= 0.0)
    {
        return min_power - 1;
    }

    /* dBFS - decibel full scale */
    /* 20 * log10(sqrt(squareSum / num_samples) / max_audio_level) dBFS */

    power = 10 * log10(squareSum / ((double)num_samples)) - 20 * log10(max_audio_level);

    /* always return min_power if there is audio */
    if (power < min_power)
    {
        return min_power;
    }

    return power;
}

double calc_audio_peak_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power)
{
    int i;
    int32_t sample;
    int32_t maxSample = 0;
    double power;

    switch (byte_alignment)
    {
        case 1:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)((uint32_t)p_samples[i] << 24);

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 2:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)(
                    (((uint32_t)p_samples[i * 2]) << 16) |
                    (((uint32_t)p_samples[i * 2 + 1]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 3:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)(
                    (((uint32_t)p_samples[i * 3]) << 8) |
                    (((uint32_t)p_samples[i * 3 + 1]) << 16) |
                    (((uint32_t)p_samples[i * 3 + 2]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 4:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)(
                    (uint32_t)p_samples[i * 4] |
                    (((uint32_t)p_samples[i * 4 + 1]) << 8) |
                    (((uint32_t)p_samples[i * 4 + 2]) << 16) |
                    (((uint32_t)p_samples[i * 4 + 3]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        default:
            /* not supported */
            break;
    }

    /* return min_power - 1 if no audio is present */
    if (maxSample == 0)
    {
        return min_power - 1;
    }

    /* dBFS - decibel full scale */
    power = 20 * log10(maxSample / (2.0*1024*1024*1024));

    /* always return min_power if there is audio */
    if (power < min_power)
    {
        return min_power;
    }

    return power;
}


void strcat_separator(char* path)
{
    size_t len = strlen(path);

    if (len == 0 || path[len - 1] != '/')
    {
        path[len] = '/';
        path[len + 1] = '\0';
    }
}

char* get_host_name(char* buffer, size_t bufferSize)
{
    if (gethostname(buffer, bufferSize - 1) != 0)
    {
        // failed
        buffer[0] = '\0';
        return buffer;
    }

    buffer[bufferSize - 1] = '\0';
    return buffer;
}

char* get_user_name(char* buffer, size_t bufferSize)
{
    FILE* cmdStdout = popen("whoami", "r");
    if (cmdStdout == 0)
    {
        // failed
        buffer[0] = '\0';
        return buffer;
    }

    size_t numRead = fread(buffer, 1, bufferSize - 1, cmdStdout);
    if (numRead <= 0)
    {
        // failed
        buffer[0] = '\0';
    }
    else
    {
        buffer[numRead] = '\0';

        // trim end
        int i;
        for (i = numRead - 1; i >= 0; i--)
        {
            if (!isspace(buffer[i]))
            {
                break;
            }

            buffer[i] = '\0';
        }
    }

    pclose(cmdStdout);

    return buffer;
}


int64_t convert_length(int64_t length, const Rational* oldFrameRate, const Rational* newFrameRate)
{
    double factor;
    int64_t newLength;

    if (length < 0 || memcmp(oldFrameRate, newFrameRate, sizeof(*oldFrameRate)) == 0 ||
        oldFrameRate->num < 1 || oldFrameRate->den < 1 || newFrameRate->num < 1 || newFrameRate->den < 1)
    {
        return length;
    }

    factor = newFrameRate->num * oldFrameRate->den / (double)(newFrameRate->den * oldFrameRate->num);
    newLength = (int64_t)(length * factor + 0.5);

    if (newLength < 0)
    {
        /* e.g. if length was the max value and the new rate is faster */
        return length;
    }

    return newLength;
}

int64_t convert_non_drop_timecode(int64_t timecode, const Rational* oldFrameRate, const Rational* newFrameRate)
{
    double factor;
    int64_t newTimecode;

    if (timecode < 0 || memcmp(oldFrameRate, newFrameRate, sizeof(*oldFrameRate)) == 0 ||
        oldFrameRate->num < 1 || oldFrameRate->den < 1 || newFrameRate->num < 1 || newFrameRate->den < 1)
    {
        return timecode;
    }

    factor = get_rounded_frame_rate(newFrameRate) / (double)get_rounded_frame_rate(oldFrameRate);
    newTimecode = (int64_t)(timecode * factor + 0.5);

    if (newTimecode < 0)
    {
        /* e.g. if timecode was the max value and the new rate is faster */
        return timecode;
    }

    return newTimecode;
}



