#ifndef __PROGRESS_BAR_INPUT_CONNECT_H__
#define __PROGRESS_BAR_INPUT_CONNECT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_control.h"
#include "progress_bar_input.h"


typedef struct ProgressBarConnect ProgressBarConnect;


int pic_create_progress_bar_connect(MediaControl* control, ProgressBarInput* input, 
    ProgressBarConnect** connect);
void pic_free_progress_bar_connect(ProgressBarConnect** connect);


#ifdef __cplusplus
}
#endif



#endif


