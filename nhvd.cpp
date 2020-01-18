/*
 * NHVD Network Hardware Video Decoder C++ library implementation
 *
 * Copyright 2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "nhvd.h"

// Minimal Latency Streaming Protocol library
#include "mlsp.h"
// Hardware Video Decoder library
#include "hvd.h"
// Hardware Depth Unprojector library
#include "hdu.h"

#include <thread>
#include <mutex>
#include <fstream>
#include <iostream>
#include <string.h> //memset

using namespace std;

static void nhvd_network_decoder_thread(nhvd *n);
static int nhvd_decode_frame(nhvd *n, hvd_packet* packet);
static nhvd *nhvd_close_and_return_null(nhvd *n);

const float PPX=421.353;
const float PPY=240.93;
const float FX=426.768;
const float FY=426.768;
const float DEPTH_UNIT=0.0001;

struct nhvd
{
	mlsp *network_streamer;

	hvd *hardware_decoder;
	AVFrame *frame;
	std::mutex frame_mutex;

	hdu *hardware_unprojector;
	hdu_point_cloud point_cloud;

	thread network_thread;
	bool keep_working;

	nhvd(): network_streamer(NULL),
			hardware_decoder(NULL),
			frame(NULL),
			hardware_unprojector(NULL),
			keep_working(true)
	{}
};

struct nhvd *nhvd_init(const nhvd_net_config *net_config,const nhvd_hw_config *hw_config)
{
	mlsp_config mlsp_cfg={net_config->ip, net_config->port, net_config->timeout_ms};
	hvd_config hvd_cfg={hw_config->hardware, hw_config->codec, hw_config->device,
		hw_config->pixel_format, hw_config->width, hw_config->height, hw_config->profile};

	nhvd *n=new nhvd();

	if(n == NULL)
	{
		cerr << "nhvd: not enough memory for nhvd" << endl;
		return NULL;
	}
	if( (n->network_streamer = mlsp_init_server(&mlsp_cfg)) == NULL )
	{
		cerr << "nhvd: failed to initialize network server" << endl;
		return nhvd_close_and_return_null(n);
	}
	if( (n->hardware_decoder = hvd_init(&hvd_cfg)) == NULL )
	{
		cerr << "nhvd: failed to initalize hardware decoder" << endl;
		return nhvd_close_and_return_null(n);
	}
	if( (n->frame = av_frame_alloc() ) == NULL)
	{
		cerr << "nhvd: not enough memory for video frame" << endl;
		return nhvd_close_and_return_null(n);
	}
	n->frame->data[0]=NULL;

	if( (n->hardware_unprojector = hdu_init(PPX, PPY, FX, FY, DEPTH_UNIT)) == NULL )
	{
		cerr << "nhvd: failed to initialize hardware unprojector" << endl;
		return nhvd_close_and_return_null(n);
	}

	n->point_cloud.data = NULL, n->point_cloud.size = 0, n->point_cloud.used = 0;

	n->network_thread = thread(nhvd_network_decoder_thread, n);

	return n;
}

static void nhvd_network_decoder_thread(nhvd *n)
{
	hvd_packet packet={0};
	mlsp_frame *streamer_frame;
	int error;

	while(n->keep_working)
	{
		streamer_frame = mlsp_receive(n->network_streamer, &error);

		if(streamer_frame == NULL)
		{
			if(error==MLSP_TIMEOUT)
			{
				cout << "." << endl;
				mlsp_receive_reset(n->network_streamer);
				continue;
			}
			cerr << "nhvd: error while receiving frame" << endl;
			break;
		}

		cout << "collected frame " << streamer_frame->framenumber;
		packet.data=streamer_frame->data;
		packet.size=streamer_frame->size;

		if (nhvd_decode_frame(n, &packet) != NHVD_OK)
			break;
	}

	//flush the decoder
	cerr << "nhvd: network decoder thread finished" << endl;
}

static int nhvd_decode_frame(nhvd *n, hvd_packet* packet)
{
	AVFrame *frame = NULL;
	int error = 0;

	if(hvd_send_packet(n->hardware_decoder, packet) != HVD_OK)
	{
		cerr << "hvd: error during decoding" << endl;
		return NHVD_ERROR; //fail actually?
	}

	while( (frame = hvd_receive_frame(n->hardware_decoder, &error) ) != NULL )
	{
//		std::lock_guard<std::mutex> frame_guard(n->frame_mutex);
//		av_frame_unref(n->frame);
//		av_frame_ref(n->frame, frame);
		av_frame_unref(n->frame);
		av_frame_ref(n->frame, frame);

		//guard the point cloud, not the frame
		std::lock_guard<std::mutex> frame_guard(n->frame_mutex);
		//unproject
		int size = n->frame->width * n->frame->height;
		if(size != n->point_cloud.size)
		{
			delete [] n->point_cloud.data;
			n->point_cloud.data = new float3[size];
			n->point_cloud.size = size;
			n->point_cloud.used = 0;
		}

		hdu_depth depth = {(uint16_t*)n->frame->data[0], n->frame->width, n->frame->height, n->frame->linesize[0]};
		//this could be moved to separate thread
		hdu_unproject(n->hardware_unprojector, &depth, &n->point_cloud);
		//temp
		memset(n->point_cloud.data + n->point_cloud.used, 0, (n->point_cloud.size-n->point_cloud.used)*sizeof(n->point_cloud.data[0]));
	}

	if(error != HVD_OK)
	{
		cerr << "nhvd: error after decoding"<< endl;
		return NHVD_ERROR;
	}

	return NHVD_OK;
}

int nhvd_get_frame_begin(nhvd *n, nhvd_frame *frame)
{
	if(n == NULL)
		return NHVD_ERROR;

	n->frame_mutex.lock();

	//for user convinience, return ERROR if there is no data
	if(n->frame->data[0] == NULL)
		return NHVD_ERROR;

	frame->width = n->frame->width;
	frame->height = n->frame->height;
	frame->format = n->frame->format;

	//copy just a few ints and pointers, not the actual data
	memcpy(frame->linesize, n->frame->linesize, sizeof(frame->linesize));
	memcpy(frame->data, n->frame->data, sizeof(frame->data));

	return NHVD_OK;
}

int nhvd_get_frame_end(struct nhvd *n)
{
	if(n == NULL)
		return NHVD_ERROR;

	av_frame_unref(n->frame);

	n->frame_mutex.unlock();
	return NHVD_OK;
}

//NULL if there is no fresh data, non NULL otherwise
int nhvd_get_point_cloud_begin(nhvd *n, nhvd_point_cloud *pc)
{
	if(n == NULL)
		return NHVD_ERROR;

	n->frame_mutex.lock();

	//for user convinience, return ERROR if there is no data
	if(n->point_cloud.data == NULL)
		return NHVD_ERROR;

	pc->data = n->point_cloud.data;
	pc->size = n->point_cloud.size;
	pc->used = n->point_cloud.used;

	return NHVD_OK;

}
//returns HVE_OK on success, HVE_ERROR on fatal error
int nhvd_get_point_cloud_end(nhvd *n)
{
	if(n == NULL)
		return NHVD_ERROR;

	n->frame_mutex.unlock();

	return NHVD_OK;
}

static nhvd *nhvd_close_and_return_null(nhvd *n)
{
	nhvd_close(n);
	return NULL;
}

void nhvd_close(nhvd *n)
{ //free mutex?
	if(n == NULL)
		return;

	n->keep_working=false;
	if(n->network_thread.joinable())
		n->network_thread.join();

	mlsp_close(n->network_streamer);
	hvd_close(n->hardware_decoder);
	hdu_close(n->hardware_unprojector);
	delete [] n->point_cloud.data;

	av_frame_free(&n->frame);

	delete n;
}
