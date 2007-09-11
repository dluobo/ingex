#ifndef __HTTP_ACCESS_RESOURCES_H__
#define __HTTP_ACCESS_RESOURCES_H__


typedef struct
{
    char name[64];

    char contentType[64];
    const unsigned char* data;
    unsigned int dataSize;
    
    unsigned char* dynData;
} HTTPAccessResource;


typedef struct HTTPAccessResources HTTPAccessResources;

int har_create_resources(HTTPAccessResources** resources);
void har_free_resources(HTTPAccessResources** resources);

int har_add_resource(HTTPAccessResources* resources, HTTPAccessResource* resource);
const HTTPAccessResource* har_get_resource(HTTPAccessResources* resources, const char* name);


#endif


