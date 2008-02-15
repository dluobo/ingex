#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "source_event.h"
#include "logging.h"
#include "macros.h"


int svt_get_buffer_size(int numEvents)
{
    return sizeof(int) + numEvents * sizeof(SourceEvent);
}

void svt_write_num_events(unsigned char* buffer, int numEvents)
{
    memcpy(buffer, (const unsigned char*)&numEvents, sizeof(int));
}

int svt_read_num_events(const unsigned char* buffer)
{
    return *((const int*)buffer);
}

void svt_write_event(unsigned char* buffer, int index, const SourceEvent* event)
{
    memcpy(&buffer[sizeof(int) + index * sizeof(SourceEvent)], (const unsigned char*)event, sizeof(SourceEvent));
}

void svt_read_event(const unsigned char* buffer, int index, SourceEvent* event)
{
    memcpy((unsigned char*)event, &buffer[sizeof(int) + index * sizeof(SourceEvent)], sizeof(SourceEvent));
}

void svt_set_name_update_event(SourceEvent* event, char* name)
{
    memset(event, 0, sizeof(SourceEvent));
    
    event->type = SOURCE_NAME_UPDATE_EVENT_TYPE;
    strncpy(event->value.nameUpdate.name, name, sizeof(event->value.nameUpdate.name) - 1);
}

