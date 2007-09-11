#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "progress_bar_input.h"


void pil_position_set(ProgressBarInputListener* listener, float position)
{
    if (listener && listener->position_set)
    {
        listener->position_set(listener->data, position);
    }
}

void pip_set_listener(ProgressBarInput* input, ProgressBarInputListener* listener)
{
    if (input && input->set_listener)
    {
        input->set_listener(input->data, listener);
    }
}

void pip_unset_listener(ProgressBarInput* input)
{
    if (input && input->unset_listener)
    {
        input->unset_listener(input->data);
    }
}

void pip_close(ProgressBarInput* input)
{
    if (input && input->close)
    {
        input->close(input->data);
    }
}





