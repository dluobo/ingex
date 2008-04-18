#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MULTICAST_SOURCE_NAME_SIZE 64

#define PACKET_SIZE 1475				// chosen to neatly fit 240x192x3/2 + audio
										// frame without going over common MTU of 1500
#define MULTICAST_DEFAULT_PORT 2000

typedef struct {
	uint32_t	frame_number;
	uint16_t	packets_per_frame;
	uint16_t	audio_size;
	uint16_t	width;
	uint16_t	height;
	uint32_t	vitc;
	uint32_t	ltc;
	uint16_t	framerate_numer;
	uint16_t	framerate_denom;
	uint8_t		signal_ok;
	char		source_name[MULTICAST_SOURCE_NAME_SIZE];	// includes terminating NUL
} IngexNetworkHeader;

extern int connect_to_multicast_address(const char *address, int port);
extern int open_socket_for_streaming(const char *remote, int port);

extern int udp_read_frame_audio_video(int fd, double timeout, IngexNetworkHeader *p_header, uint8_t *video, uint8_t *audio, int *p_total);
extern int udp_read_frame_header(int fd, IngexNetworkHeader *p_header);

extern int send_audio_video(int fd, int width, int height, int audio_channels, const uint8_t *video, const uint8_t *audio, int frame_number, int vitc, int ltc, const char *source_name);

extern void scale_video420_for_multicast(int in_width, int in_height, int out_width, int out_height, uint8_t *video_frame, uint8_t *scaled_frame);
