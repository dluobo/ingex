#ifndef __KEYBOARD_INPUT_H__
#define __KEYBOARD_INPUT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


typedef struct
{
    void* data; /* passed to functions */
    
    void (*key_pressed)(void* data, int key);
    void (*key_released)(void* data, int key);
} KeyboardInputListener;

typedef struct
{
    void* data; /* passed to functions */
    
    void (*set_listener)(void* data, KeyboardInputListener* listener);
    void (*unset_listener)(void* data);
    void (*close)(void* data);
} KeyboardInput;


/* utility functions for calling KeyboardInputListener functions */

void kil_key_pressed(KeyboardInputListener* listener, int key);
void kil_key_released(KeyboardInputListener* listener, int key);


/* utility functions for calling KeyboardInput functions */

void kip_set_listener(KeyboardInput* input, KeyboardInputListener* listener);
void kip_unset_listener(KeyboardInput* input);
void kip_close(KeyboardInput* input);




#ifdef __cplusplus
}
#endif



#endif


