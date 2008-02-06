#include "multicast_video.h"
#include "YUV_scale_pic.h"

extern int connect_to_multicast_address(const char *address, int port)
{
	int fd;

	if ((fd = socket( AF_INET, SOCK_DGRAM, 0 )) == -1) {	// SOCK_DGRAM means UDP socket
		perror("cannot create socket");
		return -1;
	}

	/* Make sure bind will work if socket already in use */
	int i_opt = 1;
	if (setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (void *) &i_opt, sizeof( i_opt ) ) == -1 )
	{
		perror("cannot configure socket (SO_REUSEADDR)");
		return -1;
	}

	/* Attempt to increase buffer size */
	i_opt = 0x80000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure socket (SO_RCVBUF)");
	}
	i_opt = 0;
	unsigned i_opt_size = sizeof(i_opt);
	getsockopt(fd, 1, 8, (void*) &i_opt, &i_opt_size);
	//printf("socket buffer set to 0x%x (%d)\n", i_opt, i_opt);

	/* setup address for bind */
	struct sockaddr_in sock;
	memset(&sock, 0, sizeof(struct sockaddr_in));
	sock.sin_family = AF_INET;
	sock.sin_port = htons(port);
	sock.sin_addr.s_addr = inet_addr(address);		// convert e.g. "224.1.0.20" string

	if (bind(fd, (struct sockaddr *)&sock, sizeof( sock )) < 0) {
		perror("cannot bind socket");
		return -1;
	}

	/* Join multicast group if multicast address */
	if ( ((((in_addr_t)(ntohl(sock.sin_addr.s_addr))) & 0xf0000000) == 0xe0000000) ) {
		struct ip_mreq imr;

		imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
		imr.imr_interface.s_addr = INADDR_ANY;

		if (setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
						(char*)&imr, sizeof(struct ip_mreq) ) == -1) {
			perror("failed to join IP multicast group");
			return -1;
		}
	}
	printf("Joined multicast group for %s:%d\n", address, port);
	return fd;
}

extern int open_socket_for_streaming(const char *remote, int port)
{
	int fd, i_opt;
	unsigned			i_opt_size;
	struct sockaddr_in	sock;

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		perror("cannot create socket");
		return -1;
	}

	/* Make sure bind will work if socket already in use */
	i_opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure socket (SO_REUSEADDR)");
		return -1;
	}

	/* TODO: do we need to make this non blocking? */
	/* TODO: fcntl64(8, F_SETFL, O_RDWR|O_NONBLOCK) */

	/* Attempt to increase buffer size */
	i_opt = 0x80000;
	if (setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure socket (SO_RCVBUF)");
	}
	if (setsockopt( fd, SOL_SOCKET, SO_SNDBUF, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure socket (SO_SNDBUF)");
	}
	i_opt = 0;
	i_opt_size = sizeof( i_opt );
	getsockopt( fd, 1, 8, (void*) &i_opt, &i_opt_size );
	//if (i_opt != 0x80000) {				// debug to test kernel tuning
	//	printf("socket buffer set to %d bytes\n", i_opt);
	//}

	/* Set multicast time-to-live */
	i_opt = 12;
	if (setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot set multicast hop limit");
	}

	/* setup address for connect */
	memset(&sock, 0, sizeof( struct sockaddr_in ) );
	sock.sin_family = AF_INET;
	sock.sin_port = htons( port );
	sock.sin_addr.s_addr = inet_addr( remote );		// convert "224.1.0.20" string

	if (connect(fd, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
		perror("cannot connect socket");
		return -1;
	}

	return fd;
}

extern int udp_read_frame_audio_video(int fd, IngexNetworkHeader *p_header, uint8_t *video, uint8_t *audio, int *p_total)
{
	struct timeval timeout = {2, 0};		// timeout of 2 seconds used to read a packet

	int packets_read = 0;
	int total_bytes = 0;
	int last_packet_num = -1;
	int video_size = 0;
	int audio_size = 1920*2;
	int remaining_audio = audio_size;
	int found_first_packet = 0;
	unsigned short packet_num;

	uint8_t buf[PACKET_SIZE];

    /* Read a frame from the network */
	while (1)
    {
		fd_set	readfds;
		int		n;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		n = select(fd + 1, &readfds, NULL, NULL, &timeout);
		if (n == 0)
			return -1;		// timeout

		if (! FD_ISSET(fd, &readfds))
			continue;		// not readable - try again

		ssize_t bytes_read = recv(fd, buf, PACKET_SIZE, 0);

		if ((bytes_read != PACKET_SIZE) || (buf[0] != 'I'))
			return -1;		// bad packet

		total_bytes += bytes_read;

		// Packet reading code must cope with packets not arriving in order or at all.
		// Since the video is intended for monitoring only, it doesn't matter if
		// we mix data from different frames.

		memcpy(&packet_num, &buf[2], 2);

		// keeping trying until we read a packet number 0
		if (!found_first_packet && packet_num != 0) {
			continue;
		}

		packets_read++;

		if (packet_num == 0) {
			found_first_packet = 1;

			// fill out header
			memcpy(p_header, &buf[4], sizeof(IngexNetworkHeader));
			video_size = p_header->width * p_header->height * 3 / 2;

			// copy audio
			int audio_chunk_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			memcpy(audio, &buf[4 + sizeof(IngexNetworkHeader)], audio_chunk_size);
			audio += audio_chunk_size;
			remaining_audio -= audio_chunk_size;
		}
		else if (packet_num == 1 || packet_num == 2) {
			// copy audio
			int audio_chunk_size = PACKET_SIZE - 4;
			if (remaining_audio > 0) {
				if (remaining_audio < audio_chunk_size)
					audio_chunk_size = remaining_audio;
				memcpy(audio, &buf[4], audio_chunk_size);
				audio += audio_chunk_size;
				remaining_audio -= audio_chunk_size;
			}
		}
		else if (packet_num >= 3) {
			// video packet
			int video_chunk_size = PACKET_SIZE - 4;

			// store video
			// calculate position in video frame for data based on packet_num
			int video_pos = (packet_num - 3) * video_chunk_size;
			if (video_pos + video_chunk_size > video_size)
				video_chunk_size = video_size - video_pos;
			memcpy(video + video_pos, &buf[4], video_chunk_size);
		}

		if (p_header->packets_per_frame > 0) {
			// If we got the last packet or we've read the right number of packets, stop
			if (packet_num == p_header->packets_per_frame - 1 ||
				(found_first_packet && packets_read == p_header->packets_per_frame))
			break;
		}

		last_packet_num = packet_num;
    }
	*p_total = total_bytes;
	return 0;
}

extern int udp_read_frame_header(int fd, IngexNetworkHeader *p_header)
{
	uint8_t buf[PACKET_SIZE];

	printf("Reading video parameters from multicast stream...");
	fflush(stdout);

    /* Read a frame from the network */
	while (1)
    {
		fd_set	readfds;
		int		n;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		n = select(fd + 1, &readfds, NULL, NULL, NULL);		// last arg NULL means no timeout
		if (n == 0)
			return -1;		// select failed for some reason

		if (! FD_ISSET(fd, &readfds))
			continue;		// not readable - try again

		ssize_t bytes_read = recv(fd, buf, PACKET_SIZE, 0);

		if ((bytes_read != PACKET_SIZE) || (buf[0] != 'I'))
			return -1;		// bad packet

		// The header is always in packet number 0
		unsigned short packet_num;
		memcpy(&packet_num, &buf[2], 2);

		if (packet_num != 0)
			continue;

		// fill out header
		memcpy(p_header, &buf[4], sizeof(IngexNetworkHeader));

		break;
    }
	printf(" width=%d height=%d packets_per_frame=%d source_name=\"%s\"\n",
			p_header->width, p_header->height, p_header->packets_per_frame, p_header->source_name);
	return 0;
}


// write 50 UDP packets with video data
// each packet is 1475 bytes
extern int send_audio_video(int fd, int width, int height, const uint8_t *video, const uint8_t *audio,
						int frame_number, int vitc, int ltc)
{
	// FIXME: change this to a stack variable after testing with valgrind
	unsigned char *buf = malloc(PACKET_SIZE);
	int total_bytes_written = 0;

	int video_size = width * height * 3/2;
	int audio_size = 1920 * 2;
	int header_size = sizeof(IngexNetworkHeader);
	IngexNetworkHeader header;

	int packets_per_frame = (int)( (header_size + audio_size + video_size) / (double)(PACKET_SIZE-4) + 0.999999 );

	// fill out header
	memset(&header, 0, sizeof(header));
	header.packets_per_frame = packets_per_frame;
	header.signal_ok = 1;
	header.framerate_numer = 1;
	header.framerate_denom = 25;
	header.width = width;
	header.height = height;
	header.frame_number = frame_number;
	header.vitc = vitc;
	header.ltc = ltc;
	strcpy(header.source_name, "TEST_SOURCE");

	int video_chunk_size = PACKET_SIZE - 4;
	const uint8_t *p_video_chunk = video;
	int remaining_audio = audio_size;

	unsigned short packet_num = 0;
	while (1) {
		// packet header is 4 bytes starting with 'I' sync byte
		buf[0] = 'I';		// sync byte 'I' for ingex
		buf[1] = 0;			// flags
		memcpy(&buf[2], &packet_num, 2);

		if (packet_num == 0) {
			// write header followed by audio data
			buf[1] |= 0x80;	// set high bit to indicate start-of-frame
			memcpy(&buf[4], &header, sizeof(IngexNetworkHeader));

			// write as much audio as will fit
			int audio_chunk_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			memcpy(&buf[4 + sizeof(IngexNetworkHeader)], audio, audio_chunk_size);
			audio += audio_chunk_size;
			remaining_audio -= audio_chunk_size;
		}
		else if (packet_num == 1 || packet_num == 2) {
			// second & third parts of audio
			int audio_chunk_size = PACKET_SIZE - 4;
			if (remaining_audio < audio_chunk_size)
				audio_chunk_size = remaining_audio;
			memcpy(&buf[4], audio, audio_chunk_size);
			audio += audio_chunk_size;
			remaining_audio -= audio_chunk_size;
		}
		else {
			// video
			if (p_video_chunk - video + video_chunk_size > video_size)
				video_chunk_size = video_size - (p_video_chunk - video);
			memcpy(&buf[4], p_video_chunk, video_chunk_size);
			p_video_chunk += video_chunk_size;
		}

		ssize_t bytes_written = send(fd, buf, PACKET_SIZE, 0);
		if (bytes_written == -1) {
			perror("send");
			return 0;
		}
		total_bytes_written += bytes_written;
		if (p_video_chunk - video >= video_size)
			break;

		packet_num++;
	}
	free(buf);
	return total_bytes_written;
}

extern void scale_video420_for_multicast(int in_width, int in_height, int out_width, int out_height, uint8_t *video_frame, uint8_t *scaled_frame)
{
	YUV_frame input_frame;
	YUV_frame output_frame;
	uint8_t workspace[2*in_width*4];

	input_frame.Y.w = in_width;
	input_frame.Y.h = in_height;
	input_frame.Y.lineStride = in_width;
	input_frame.Y.pixelStride = 1;
	input_frame.Y.buff = video_frame;
	input_frame.U.w = in_width/2;
	input_frame.U.h = in_height/2;
	input_frame.U.lineStride = in_width/2;
	input_frame.U.pixelStride = 1;
	input_frame.U.buff = video_frame + in_width*in_height;
	input_frame.V.w = in_width/2;
	input_frame.V.h = in_height/2;
	input_frame.V.lineStride = in_width/2;
	input_frame.V.pixelStride = 1;
	input_frame.V.buff = video_frame + in_width*in_height * 5/4;
	
	output_frame.Y.w = out_width;
	output_frame.Y.h = out_height;
	output_frame.Y.lineStride = out_width;
	output_frame.Y.pixelStride = 1;
	output_frame.Y.buff = scaled_frame;
	output_frame.U.w = out_width/2;
	output_frame.U.h = out_height/2;
	output_frame.U.lineStride = out_width/2;
	output_frame.U.pixelStride = 1;
	output_frame.U.buff = scaled_frame + out_width*out_height;
	output_frame.V.w = out_width/2;
	output_frame.V.h = out_height/2;
	output_frame.V.lineStride = out_width/2;
	output_frame.V.pixelStride = 1;
	output_frame.V.buff = scaled_frame + out_width*out_height * 5/4;

	scale_pic(	&input_frame, &output_frame,
				0, 0, output_frame.Y.w, output_frame.Y.h,
				0,				// turn off interlace since this is for monitoring
				1, 1,			// hfil, vfil
				workspace);
}
