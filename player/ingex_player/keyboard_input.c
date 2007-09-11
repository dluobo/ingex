#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "keyboard_input.h"


void kil_key_pressed(KeyboardInputListener* listener, int key)
{
    if (listener && listener->key_pressed)
    {
        listener->key_pressed(listener->data, key);
    }
}

void kil_key_released(KeyboardInputListener* listener, int key)
{
    if (listener && listener->key_released)
    {
        listener->key_released(listener->data, key);
    }
}

void kip_set_listener(KeyboardInput* input, KeyboardInputListener* listener)
{
    if (input && input->set_listener)
    {
        input->set_listener(input->data, listener);
    }
}

void kip_unset_listener(KeyboardInput* input)
{
    if (input && input->unset_listener)
    {
        input->unset_listener(input->data);
    }
}

void kip_close(KeyboardInput* input)
{
    if (input && input->close)
    {
        input->close(input->data);
    }
}





