#ifndef __SOURCE_EVENT_H__
#define __SOURCE_EVENT_H__



typedef enum
{
    NULL_EVENT_TYPE = 0,
    SOURCE_NAME_UPDATE_EVENT_TYPE
} SourceEventType;


typedef struct
{
    char name[64];
} SourceNameUpdateEvent;


typedef struct
{
    SourceEventType type;
    union
    {
        SourceNameUpdateEvent nameUpdate;
    } value;
} SourceEvent;


int svt_get_buffer_size(int numEvents);

void svt_write_num_events(unsigned char* buffer, int numEvents);
int svt_read_num_events(const unsigned char* buffer);

void svt_write_event(unsigned char* buffer, int index, const SourceEvent* event);
void svt_read_event(const unsigned char* buffer, int index, SourceEvent* event);


void svt_set_name_update_event(SourceEvent* event, char* name);


#endif


