/*
 * NHVD Network Hardware Video Decoder example
 *
 * Copyright 2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * Note that NHVD was written for integration with Unity
 * - it assumes that engine checks just before rendering if new frame has arrived
 * - this example simulates such behaviour be sleeping frame time
 * - for those reason NHVD may not fit your workflow
 */

#include "../nhvd.h"

#include <iostream>
#include <unistd.h> //usleep, note that this is not portable

using namespace std;

void main_loop(nhvd *network_decoder);
int process_user_input(int argc, char **argv, nhvd_hw_config *hw_config, nhvd_net_config *net_config);

//network configuration
const char *IP=NULL; //listen on or NULL (listen on any)
const uint16_t PORT=9768; //to be input through CLI
const int TIMEOUT_MS=500; //timeout, accept new streaming sequence by receiver

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

//depth unprojection configuration
const float PPX=421.353;
const float PPY=240.93;
const float FX=426.768;
const float FY=426.768;
const float DEPTH_UNIT=0.0001;

//we simpulate application rendering at framerate
const int FRAMERATE = 30;

int main(int argc, char **argv)
{
	nhvd_net_config net_config = {IP, PORT, TIMEOUT_MS};
	nhvd_hw_config hw_config = {HARDWARE, CODEC, DEVICE, PIXEL_FORMAT, WIDTH, HEIGHT, PROFILE};
	nhvd_depth_config depth_config = {PPX, PPY, FX, FY, DEPTH_UNIT};

	if(process_user_input(argc, argv, &hw_config, &net_config) != 0)
		return 1;

	nhvd *network_decoder = nhvd_init(&net_config, &hw_config, &depth_config);

	if(!network_decoder)
	{
		cerr << "failed to initalize nhvd" << endl;
		return 2;
	}

	main_loop(network_decoder);

	nhvd_close(network_decoder);
	return 0;
}

void main_loop(nhvd *network_decoder)
{
	const int SLEEP_US = 1000000/FRAMERATE;

	//this is where we will get the decoded data
	nhvd_frame frame;
	nhvd_point_cloud cloud;

	bool keep_working=true;

	while(keep_working)
	{
		if( nhvd_get_point_cloud_begin(network_decoder, &cloud) == NHVD_OK )
		{
			//do something with:
			// - cloud.data
			// - cloud.colors
			// - cloud.size
			// - cloud.used
			cout << "Decoded cloud with " << cloud.used << " points" << endl;
		}

		if( nhvd_get_point_cloud_end(network_decoder) != NHVD_OK )
			break; //error occured

		//this should spin once per frame rendering
		//so wait until we are after rendering
		usleep(SLEEP_US);
	}
}

int process_user_input(int argc, char **argv, nhvd_hw_config *hw_config, nhvd_net_config *net_config)
{
	if(argc < 5)
	{
		fprintf(stderr, "Usage: %s <port> <hardware> <codec> <pixel format> [device] [width] [height] [profile]\n\n", argv[0]);
		fprintf(stderr, "examples: \n");
		fprintf(stderr, "%s 9768 vaapi hevc p010le /dev/dri/renderD128 848 480 2\n", argv[0]);

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
