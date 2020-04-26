/*
 * NHVD Network Hardware Video Decoder C++ library implementation
 *
 * Copyright 2019-2020 (C) Bartosz Meglicki <meglickib@gmail.com>
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

enum NHVD_COMPILE_TIME_CONSTANTS
{
	NHVD_MAX_DECODERS=3, //!< max number of decoders in multi decoding
};

using namespace std;

static void nhvd_network_decoder_thread(nhvd *n);
static int nhvd_decode_frame(nhvd *n, hvd_packet* packet);
static int nhvd_unproject_depth_frame(nhvd *n, const AVFrame *depth_frame, const AVFrame *texture_frame);
static nhvd *nhvd_close_and_return_null(nhvd *n, const char *msg);
static int NHVD_ERROR_MSG(const char *msg);

struct nhvd
{
	mlsp *network_streamer;

	hvd *hardware_decoder[NHVD_MAX_DECODERS];
	int hardware_decoders_size;

	AVFrame *frame[NHVD_MAX_DECODERS];
	std::mutex mutex; //guards frame and point_cloud

	hdu *hardware_unprojector;
	hdu_point_cloud point_cloud;

	thread network_thread;
	bool keep_working;

	nhvd():
			network_streamer(NULL),
			hardware_decoder(), //zero out
			hardware_decoders_size(0),
			frame(), //zero out
			hardware_unprojector(NULL),
			point_cloud(),
			keep_working(true)
	{}
};

struct nhvd *nhvd_init(
	const nhvd_net_config *net_config,
	const nhvd_hw_config *hw_config, int hw_size,
	const nhvd_depth_config *depth_config)
{
	mlsp_config mlsp_cfg={net_config->ip, net_config->port, net_config->timeout_ms};

	if(hw_size > NHVD_MAX_DECODERS)
		return nhvd_close_and_return_null(NULL, "the maximum number of decoders (compile time) exceeded");

	nhvd *n=new nhvd();

	if(n == NULL)
		return nhvd_close_and_return_null(NULL, "not enough memory for nhvd");

	if( (n->network_streamer = mlsp_init_server(&mlsp_cfg)) == NULL )
		return nhvd_close_and_return_null(n, "failed to initialize network server");

	n->hardware_decoders_size = hw_size;

	for(int i=0;i<hw_size;++i)
	{
		hvd_config hvd_cfg={hw_config[i].hardware, hw_config[i].codec, hw_config[i].device,
		hw_config[i].pixel_format, hw_config[i].width, hw_config[i].height, hw_config[i].profile};

		if( (n->hardware_decoder[i] = hvd_init(&hvd_cfg)) == NULL )
			return nhvd_close_and_return_null(n, "failed to initalize hardware decoder");

		if( (n->frame[i] = av_frame_alloc() ) == NULL)
			return nhvd_close_and_return_null(n, "not enough memory for video frame");

		n->frame[i]->data[0]=NULL;
	}

	if(depth_config)
	{
		const nhvd_depth_config *dc=depth_config;
		if( (n->hardware_unprojector = hdu_init(dc->ppx, dc->ppy, dc->fx, dc->fy, dc->depth_unit)) == NULL )
			return nhvd_close_and_return_null(n, "failed to initialize hardware unprojector");
	}

	n->network_thread = thread(nhvd_network_decoder_thread, n);

	return n;
}

static void nhvd_network_decoder_thread(nhvd *n)
{
	hvd_packet packets[NHVD_MAX_DECODERS] = {0};
	mlsp_frame *streamer_frame;
	int error;

	while(n->keep_working)
	{
		streamer_frame = mlsp_receive(n->network_streamer, &error);

		if(streamer_frame == NULL)
		{
			if(error == MLSP_TIMEOUT)
			{
				cout << "." << endl;
				//accept new streaming sequence
				mlsp_receive_reset(n->network_streamer);
				//flush (drain) the decoder and prepare for new stream
				nhvd_decode_frame(n, NULL);
				continue;
			}
			cerr << "nhvd: error while receiving frame" << endl;
			break;
		}

		for(int i=0;i<n->hardware_decoders_size;++i)
		{
			packets[i].data=streamer_frame->data[i];
			packets[i].size=streamer_frame->size[i];
		}
		if (nhvd_decode_frame(n, packets) != NHVD_OK)
			break;
	}

	//flush the decoder
	cerr << "nhvd: network decoder thread finished" << endl;
}

//NULL packet to flush all hardware decoders
static int nhvd_decode_frame(nhvd *n, hvd_packet *packet)
{
	AVFrame *frame[NHVD_MAX_DECODERS] = {0};
	int error = 0;

	//special NULL packet case with flush request
	for(int i=0;!packet && i < n->hardware_decoders_size;++i)
		if(hvd_send_packet(n->hardware_decoder[i], NULL) != HVD_OK)
			return NHVD_ERROR_MSG("error during decoding (flush)");

	//send data to all hardware decoders
	for(int i=0;packet && i < n->hardware_decoders_size;++i)
	{
		if(!packet[i].size) //silently skip empty subframes
			continue; //(e.g. different framerates/B frames)

		if(hvd_send_packet(n->hardware_decoder[i], &packet[i]) != HVD_OK)
			return NHVD_ERROR_MSG("error during decoding"); //fail actually?
	}

	while(1)
	{
		bool keep_working = false;

		//receive data from all hardware decoders
		for(int i=0;i<n->hardware_decoders_size;++i)
		{
			if(packet && !packet[i].size) //silently skip empty subframes
				continue; //(e.g. different framerates/B frames)

			if( (frame[i] = hvd_receive_frame(n->hardware_decoder[i], &error) ) != NULL )
				keep_working = true;

			if(error != NHVD_OK)
				return NHVD_ERROR_MSG("error after decoding");
		}

		if(!keep_working)
			break;

		//the next calls to hvd_receive_frame will unref the current
		//frames so we have to either consume set of frames or ref it
		std::lock_guard<std::mutex> frame_guard(n->mutex);

		for(int i=0;i<n->hardware_decoders_size;++i)
			if(frame[i])
			{
				av_frame_unref(n->frame[i]);
				av_frame_ref(n->frame[i], frame[i]);
			}

		if(n->hardware_unprojector && frame[0])
			if(nhvd_unproject_depth_frame(n, n->frame[0], n->frame[1]) != NHVD_OK)
				return NHVD_ERROR;
	}

	return NHVD_OK;
}

static int nhvd_unproject_depth_frame(nhvd *n, const AVFrame *depth_frame, const AVFrame *texture_frame)
{
	if(depth_frame->linesize[0] / depth_frame->width != 2 ||
		(depth_frame->format != AV_PIX_FMT_P010LE && depth_frame->format != AV_PIX_FMT_P016LE))
		return NHVD_ERROR_MSG("nhvd_unproject_depth_frame expects uint16 p010le/p016le data");

	if(texture_frame && texture_frame->data[0] &&
		texture_frame->format != AV_PIX_FMT_RGB0 && texture_frame->format != AV_PIX_FMT_RGBA)
		return NHVD_ERROR_MSG("nhvd_unproject_depth_frame expects RGB0/RGBA texture data");

	int size = depth_frame->width * depth_frame->height;
	if(size != n->point_cloud.size)
	{
		delete [] n->point_cloud.data;
		delete [] n->point_cloud.colors;
		n->point_cloud.data = new float3[size];
		n->point_cloud.colors = new color32[size];
		n->point_cloud.size = size;
		n->point_cloud.used = 0;
	}

	uint16_t *depth_data = (uint16_t*)depth_frame->data[0];
	//texture data is optional
	uint32_t *texture_data = texture_frame ? (uint32_t*)texture_frame->data[0] : NULL;
	int texture_linesize = texture_frame ? texture_frame->linesize[0] : 0;

	hdu_depth depth = {depth_data, texture_data, depth_frame->width, depth_frame->height,
		depth_frame->linesize[0], texture_linesize};
	//this could be moved to separate thread
	hdu_unproject(n->hardware_unprojector, &depth, &n->point_cloud);
	//zero out unused point cloud entries
	memset(n->point_cloud.data + n->point_cloud.used, 0, (n->point_cloud.size-n->point_cloud.used)*sizeof(n->point_cloud.data[0]));
	memset(n->point_cloud.colors + n->point_cloud.used, 0, (n->point_cloud.size-n->point_cloud.used)*sizeof(n->point_cloud.colors[0]));

	return NHVD_OK;
}

//NULL if there is no fresh data, non NULL otherwise
int nhvd_get_begin(nhvd *n, nhvd_frame *frame, nhvd_point_cloud *pc)
{
	if(n == NULL)
		return NHVD_ERROR;

	n->mutex.lock();

	bool new_data = false;

	for(int i=0;i<n->hardware_decoders_size;++i)
		if(n->frame[i]->data[0] != NULL)
			new_data = true;

	//for user convinience, return ERROR if there is no new data
	if(!new_data)
		return NHVD_ERROR;

	if(frame)
		for(int i=0;i<n->hardware_decoders_size;++i)
		{
			frame[i].width = n->frame[i]->width;
			frame[i].height = n->frame[i]->height;
			frame[i].format = n->frame[i]->format;

			//copy just a few ints and pointers, not the actual data
			memcpy(frame[i].linesize, n->frame[i]->linesize, sizeof(frame[i].linesize));
			memcpy(frame[i].data, n->frame[i]->data, sizeof(frame[i].data));
		}

	//future -consider only updating if updated frame[0] or other way
	if(pc && n->hardware_unprojector)
	{
		//copy just two pointers and ints
		pc->data = n->point_cloud.data;
		pc->colors = n->point_cloud.colors;
		pc->size = n->point_cloud.size;
		pc->used = n->point_cloud.used;
	}

	return NHVD_OK;
}

//returns HVE_OK on success, HVE_ERROR on fatal error
int nhvd_get_end(struct nhvd *n)
{
	if(n == NULL)
		return NHVD_ERROR;

	for(int i=0;i<n->hardware_decoders_size;++i)
		av_frame_unref(n->frame[i]);

	n->mutex.unlock();

	return NHVD_OK;
}

int nhvd_get_frame_begin(nhvd *n, nhvd_frame *frame)
{
	return nhvd_get_begin(n, frame, NULL);
}

int nhvd_get_frame_end(struct nhvd *n)
{
	return nhvd_get_end(n);
}

int nhvd_get_point_cloud_begin(nhvd *n, nhvd_point_cloud *pc)
{
	return nhvd_get_begin(n, NULL, pc);
}

int nhvd_get_point_cloud_end(nhvd *n)
{
	return nhvd_get_end(n);
}

static nhvd *nhvd_close_and_return_null(nhvd *n, const char *msg)
{
	if(msg)
		cerr << "nhvd: " << msg << endl;

	nhvd_close(n);

	return NULL;
}

void nhvd_close(nhvd *n)
{	//free mutex?
	if(n == NULL)
		return;

	n->keep_working=false;
	if(n->network_thread.joinable())
		n->network_thread.join();

	mlsp_close(n->network_streamer);

	for(int i=0;i<n->hardware_decoders_size;++i)
	{
		hvd_close(n->hardware_decoder[i]);
		av_frame_free(&n->frame[i]);
	}

	hdu_close(n->hardware_unprojector);
	delete [] n->point_cloud.data;
	delete [] n->point_cloud.colors;

	delete n;
}

static int NHVD_ERROR_MSG(const char *msg)
{
	cerr << "nhvd: " << msg << endl;
	return NHVD_ERROR;
}
