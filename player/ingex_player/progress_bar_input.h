#ifndef __PROGRESS_BAR_INPUT_H__
#define __PROGRESS_BAR_INPUT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


typedef struct
{
    void* data; /* passed to functions */

    /* position is a value >= 0.0 and < 100.0 */ 
    void (*position_set)(void* data, float position);
} ProgressBarInputListener;

typedef struct
{
    void* data; /* passed to functions */
    
    void (*set_listener)(void* data, ProgressBarInputListener* listener);
    void (*unset_listener)(void* data);
    void (*close)(void* data);
} ProgressBarInput;


/* utility functions for calling ProgressBarInputListener functions */

void pil_position_set(ProgressBarInputListener* listener, float position);


/* utility functions for calling ProgressBarInput functions */

void pip_set_listener(ProgressBarInput* input, ProgressBarInputListener* listener);
void pip_unset_listener(ProgressBarInput* input);
void pip_close(ProgressBarInput* input);




#ifdef __cplusplus
}
#endif



#endif


