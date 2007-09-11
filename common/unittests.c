#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_test_signals.h"
#include "video_conversion_10bits.h"

static void usage_exit(void)
{
	fprintf(stderr, "unittests\n");
	fprintf(stderr, "\n");
	exit(1);
}

static void read_sample(const char *input_name, size_t frame_size, uint8_t *frame)
{
	FILE *fp_input = NULL;

	if ((fp_input = fopen(input_name, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", input_name);
		exit(1);
	}

	size_t r;
	if ((r = fread(frame, 1, frame_size, fp_input)) != frame_size) {
		fprintf(stderr, "Expected %u, read %u\n", frame_size, r);
		perror("fread");
		exit(1);
	}

	fclose(fp_input);
}

static void write_sample(const char *output_name, size_t frame_size, const uint8_t *frame)
{
	FILE *fp_output = NULL;

	if ((fp_output = fopen(output_name, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", output_name);
		exit(1);
	}

	if (fwrite(frame, 1, frame_size, fp_output) != frame_size) {
		perror("fwrite");
		exit(1);
	}

	fclose(fp_output);
}

int main(int argc, char *argv[])
{
	int width = 720;
	int height = 576;
	size_t frame_size = width * height * 2;
	size_t frame_size_10bit = frame_size * 4 / 3;
	char *output_name = "test.uyvy";
	char *input_name = "/dp_videoedit/sport10bit";

	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			usage_exit();
		}
		fprintf(stderr, "Unrecognied argument %s\n", argv[i]);
		usage_exit();
	}

	uint8_t *frame = (uint8_t*)malloc(frame_size);
	uint8_t *frame10bit = (uint8_t*)malloc(frame_size_10bit);

	// Test creation of "NO VIDEO" uyvy caption frame
	uyvy_no_video_frame(width, height, frame);
	write_sample(output_name, frame_size, frame);

	// Test 10bit conversions
	read_sample(input_name, frame_size_10bit, frame10bit);

	ConvertFrame10to8(frame, frame10bit,
						width, width*4/3, width, height*2 - 1);
	write_sample(output_name, frame_size, frame);

	free(frame);
	free(frame10bit);

	return 0;
}
