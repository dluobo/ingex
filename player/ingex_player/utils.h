#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/time.h>

#ifdef __cplusplus
extern "C" 
{
#endif


#include "frame_info.h"


#define MAX_TIMESTAMP_STRING_SIZE   20


/* returns number of usecs slept. value can be negative */
long sleep_diff(long delay, const struct timeval* now, const struct timeval* last);

void print_timecode(TimecodeType type, TimecodeSubType subType, const Timecode* timecode);

void get_timestamp_string(char* timestampStr);
void get_short_timestamp_string(char* timestampStr);
int64_t get_timecode_now();


int init_mutex(pthread_mutex_t* mutex);
int init_cond_var(pthread_cond_t* cond);

void destroy_mutex(pthread_mutex_t* mutex);
void destroy_cond_var(pthread_cond_t* cond);

int create_joinable_thread(pthread_t* thread, void* (*start_func)(void*), void* arg);
void cancel_and_join_thread(pthread_t* thread, void* data, void (*stop_func)(void*));
void join_thread(pthread_t* thread, void* data, void (*stop_func)(void*));


void writeVITC(unsigned hh, unsigned mm, unsigned ss, unsigned ff, unsigned char *line);

double calc_audio_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power);
double calc_audio_peak_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power);

void strcat_separator(char* path);

char* get_host_name(char* buffer, size_t bufferSize);
char* get_user_name(char* buffer, size_t bufferSize);


#ifdef __cplusplus
}
#endif


#endif



