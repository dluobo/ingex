#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include "progress_bar_input_connect.h"
#include "logging.h"
#include "macros.h"


struct ProgressBarConnect
{
    MediaControl* control;
    
    ProgressBarInput* input;
    ProgressBarInputListener listener;
};


static void pic_position_set(void* data, float position)
{
    ProgressBarConnect* connect = (ProgressBarConnect*)data;
    
    mc_seek(connect->control, (int64_t)(position * 1000.0), SEEK_SET, PERCENTAGE_PLAY_UNIT);
    mc_pause(connect->control);
}



int pic_create_progress_bar_connect(MediaControl* control, ProgressBarInput* input, 
    ProgressBarConnect** connect)
{
    ProgressBarConnect* newConnect;

    CALLOC_ORET(newConnect, ProgressBarConnect, 1);
    
    newConnect->control = control;
    newConnect->input = input;

    newConnect->listener.data = newConnect;
    newConnect->listener.position_set = pic_position_set;
    pip_set_listener(input, &newConnect->listener);
    
    
    *connect = newConnect;
    return 1;
}

void pic_free_progress_bar_connect(ProgressBarConnect** connect)
{
    if (*connect == NULL)
    {
        return;
    }
    
    pip_unset_listener((*connect)->input);
    
    SAFE_FREE(connect);
}


