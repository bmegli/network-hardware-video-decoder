/*
 * NHVD Network Hardware Video Decoder example
 *
 * Copyright 2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "../nhvd.h"

#include <stdio.h>

void main_loop(struct nhvd *network_decoder);
int process_user_input(int argc, char **argv, struct nhvd_hw_config *hw_config, struct nhvd_net_config *net_config);

//decoder configuration
const char *HARDWARE=NULL; //input through CLI, e.g. "vaapi"
const char *CODEC=NULL;  //input through CLI, e.g. "h264"
const char *DEVICE=NULL; //optionally input through CLI, e.g. "/dev/dri/renderD128"
const char *PIXEL_FORMAT=NULL; //input through CLI, NULL for default (NV12) or pixel format e.g. "rgb0"
//the pixel format that you want to receive data in
//this has to be supported by hardware
const int WIDTH=0; //0 to not specify, needed by some codecs
const int HEIGHT=0; //0 to not specify, needed by some codecs
const int PROFILE=0; //0 to leave as FF_PROFILE_UNKNOWN
//for list of profiles see:
//https://ffmpeg.org/doxygen/3.4/avcodec_8h.html#ab424d258655424e4b1690e2ab6fcfc66

//network configuration
const char *IP=NULL; //listen on or NULL (listen on any)
const uint16_t PORT=9766; //to be input through CLI
const int TIMEOUT_MS=500; //timeout, accept new streaming sequence by receiver

int main(int argc, char **argv)
{
	struct nhvd_hw_config hw_config= {HARDWARE, CODEC, DEVICE, PIXEL_FORMAT, WIDTH, HEIGHT, PROFILE};
	struct nhvd_net_config net_config= {IP, PORT, TIMEOUT_MS};

	if(process_user_input(argc, argv, &hw_config, &net_config) != 0)
		return 1;

	struct nhvd *network_decoder = nhvd_init(&net_config, &hw_config, 1, 0);

	if(!network_decoder)
	{
		fprintf(stderr, "failed to initalize nhvd\n");
		return 2;
	}

	main_loop(network_decoder);

	nhvd_close(network_decoder);
	return 0;
}

void main_loop(struct nhvd *network_decoder)
{
	AVFrame *frame;
	int status;

	while( (status = nhvd_receive(network_decoder, &frame)) != NHVD_ERROR )
	{
		if(status == NHVD_TIMEOUT || frame == NULL)
			continue; //keep working

		//do something with the:
		// - frame->width
		// - frame->height
		// - frame->format
		// - frame->data
		// - frame->linesize
		// - ...

		printf("decoded frame %dx%d format %d ls[0] %d ls[1] %d ls[2] %d\n",
		frame->width, frame->height, frame->format,
		frame->linesize[0], frame->linesize[1], frame->linesize[2]);

		//The AVFrame* is valid until next loop iteration
		//You should (one of):
		//- consume the data immidiately
		//- reference the frames with FFmpeg av_frame_ref (av_frame_unref)
		//- copy the data (typically not recommended)
	}

	fprintf(stderr, "nhvd_receive failed!\n");
}

int process_user_input(int argc, char **argv, struct nhvd_hw_config *hw_config, struct nhvd_net_config *net_config)
{
	if(argc < 5)
	{
		fprintf(stderr, "Usage: %s <port> <hardware> <codec> <pixel format> [device] [width] [height] [profile]\n\n", argv[0]);
		fprintf(stderr, "examples: \n");
		fprintf(stderr, "%s 9766 vaapi h264 bgr0 \n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi h264 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 vdpau h264 yuv420p \n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi h264 bgr0 /dev/dri/renderD128\n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi h264 nv12 /dev/dri/renderD129\n", argv[0]);
		fprintf(stderr, "%s 9766 dxva2 h264 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 d3d11va h264 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 videotoolbox h264 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi hevc nv12 /dev/dri/renderD128 640 360 1\n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi hevc p010le /dev/dri/renderD128 848 480 2\n", argv[0]);

		return 1;
	}

	net_config->port = atoi(argv[1]);
	hw_config->hardware = argv[2];
	hw_config->codec = argv[3];
	hw_config->pixel_format = argv[4];
	hw_config->device = argv[5]; //NULL or device, both are ok

	if(argc > 6) hw_config->width = atoi(argv[6]);
	if(argc > 7) hw_config->height = atoi(argv[7]);
	if(argc > 8) hw_config->profile = atoi(argv[8]);

	return 0;
}
