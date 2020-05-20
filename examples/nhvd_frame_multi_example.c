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

//number of hardware decoders
#define HW_DECODERS 2

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
	struct nhvd_hw_config hw_config[HW_DECODERS] =
	{	//those could be completely different decoders using different hardware
		{HARDWARE, CODEC, DEVICE, PIXEL_FORMAT, WIDTH, HEIGHT, PROFILE},
		{HARDWARE, CODEC, DEVICE, PIXEL_FORMAT, WIDTH, HEIGHT, PROFILE}
	};
	struct nhvd_net_config net_config= {IP, PORT, TIMEOUT_MS};

	if(process_user_input(argc, argv, hw_config, &net_config) != 0)
		return 1;

	struct nhvd *network_decoder = nhvd_init(&net_config, hw_config, HW_DECODERS);

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
	AVFrame *frames[HW_DECODERS];
	int status;

	while( (status = nhvd_receive(network_decoder, frames)) != NHVD_ERROR )
	{
		if(status == NHVD_TIMEOUT)
			continue; //keep working

		for(int i=0;i<HW_DECODERS;++i)
		{
			//do something with array of frames:
			// - frames[i]->width
			// - frame[i]->height
			// - frame[i]->format
			// - frame[i]->data
			// - frame[i]->linesize
			//...

			printf("decoded frame %d %dx%d format %d ls[0] %d ls[1] %d ls[2] %d\n", i,
			frames[i]->width, frames[i]->height, frames[i]->format,
			frames[i]->linesize[0], frames[i]->linesize[1], frames[i]->linesize[2]);

			//The AVFrame* set is valid until next loop iteration
			//You should (one of):
			//- consume the data immidiately
			//- reference the frames with FFmpeg av_frame_ref (av_frame_unref)
			//- copy the data (typically not recommended)
		}
	}
	fprintf(stderr, "nhvd_receive failed!\n");
}

int process_user_input(int argc, char **argv, struct nhvd_hw_config *hw_config, struct nhvd_net_config *net_config)
{
	if(argc < 6)
	{
		fprintf(stderr, "Usage: %s <port> <hardware> <codec> <pixel_format1> <pixel_format2> [device] [width] [height] [profile] [profile2]\n\n", argv[0]);
		fprintf(stderr, "examples: \n");
		fprintf(stderr, "%s 9766 vaapi h264 bgr0 bgr0 \n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi h264 nv12 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 vdpau h264 yuv420p yuv420p \n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi h264 bgr0 bgr0 /dev/dri/renderD128\n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi h264 nv12 nv12 /dev/dri/renderD129\n", argv[0]);
		fprintf(stderr, "%s 9766 dxva2 h264 nv12 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 d3d11va h264 nv12 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 videotoolbox h264 nv12 nv12 \n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi hevc nv12 nv12 /dev/dri/renderD128 640 360 1\n", argv[0]);
		fprintf(stderr, "%s 9766 vaapi hevc p010le p010le /dev/dri/renderD128 848 480 2 2\n", argv[0]);
		fprintf(stderr, "%s 9768 vaapi hevc p010le nv12 /dev/dri/renderD128 848 480 2 1\n", argv[0]);

		return 1;
	}

	net_config->port = atoi(argv[1]);

	//first decoder
	hw_config[0].hardware = argv[2];
	hw_config[0].codec = argv[3];
	hw_config[0].pixel_format = argv[4];

	hw_config[0].device = argv[6]; //NULL or device, both are ok

	if(argc > 7) hw_config[0].width = atoi(argv[7]);
	if(argc > 8) hw_config[0].height = atoi(argv[8]);
	if(argc > 9) hw_config[0].profile = atoi(argv[9]);

	//second decoder
	hw_config[1].hardware = argv[2];
	hw_config[1].codec = argv[3];
	hw_config[1].pixel_format = argv[5];

	hw_config[1].device = argv[6]; //NULL or device, both are ok

	if(argc > 7) hw_config[1].width = atoi(argv[7]);
	if(argc > 8) hw_config[1].height = atoi(argv[8]);
	if(argc > 10) hw_config[1].profile = atoi(argv[10]);

	return 0;
}
