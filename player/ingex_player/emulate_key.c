/* originally code and ideas were used from XFakeKey, produced by Adam Pierce (http://www.doctort.org/adam/) */ 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "emulate_key.h"
#include "logging.h"
#include "macros.h"


struct EmulateKey
{
    Display* display;
    Window rootWin;
};


static int send_key(EmulateKey* emu, int down, int keysym, int modifiers)
{
    XKeyEvent event;
    Window winFocus;
    int revert;
    
    /* get window to send event to */
    XGetInputFocus(emu->display, &winFocus, &revert);
    
    /* create event */
    memset(&event, 0, sizeof(XKeyEvent));
    event.type = (down) ? KeyPress : KeyRelease;
    /* event.serial set by XSendEvent */
    /* event.send_event set by XSendEvent */
    event.display = emu->display;
    event.window = winFocus;
    event.root = emu->rootWin;
    event.subwindow = None;
    event.time = CurrentTime;
    event.x = 1;
    event.y = 1;
    event.x_root = 1;
    event.y_root = 1;
    event.state = modifiers;
    event.keycode = XKeysymToKeycode(emu->display, keysym);
    event.same_screen = True;
    
    /* send event */
    return XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent*)&event) != 0;
}



int create_emu(EmulateKey** emu)
{
    EmulateKey* newEmu;
    
    if ((newEmu = (EmulateKey*)malloc(sizeof(EmulateKey))) == NULL)
    {
        fprintf(stderr, "Failed to allocate memory\n");
        return 0;
    }
    memset(newEmu, 0, sizeof(EmulateKey));
    
    
    if ((newEmu->display = XOpenDisplay(0)) == NULL)
    {
        fprintf(stderr, "Failed to open display\n");
        SAFE_FREE(&newEmu);
        return 0;
    }
    newEmu->rootWin = XDefaultRootWindow(newEmu->display);
    
    *emu = newEmu;
    return 1;
}

int emu_key_down(EmulateKey* emu, int keysym, int modifier)
{
    return send_key(emu, 1, keysym, modifier);
}

int emu_key_up(EmulateKey* emu, int keysym, int modifier)
{
    return send_key(emu, 0, keysym, modifier);
}

int emu_key(EmulateKey* emu, int keysym, int modifier)
{
    return emu_key_down(emu, keysym, modifier) && emu_key_up(emu, keysym, modifier); 
}

void free_emu(EmulateKey** emu)
{
    if (*emu == NULL)
    {
        return;
    }

    if ((*emu)->display != NULL)
    {
        XCloseDisplay((*emu)->display);
    }
    
    SAFE_FREE(emu);
}

