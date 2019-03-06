/*
 * NHVD Network Hardware Video Decoder C++ library implementation
 *
 * Copyright 2019 (C) Bartosz Meglicki <meglickib@gmail.com>
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

#include <thread>
#include <unistd.h> // TEMP usleep


#include <mutex>
#include <fstream>
#include <iostream>

using namespace std;

static void nhvd_network_decoder_thread(nhvd *n);
static int nhvd_decode_frame(nhvd *n, hvd_packet* packet);
static nhvd *nhvd_close_and_return_null(nhvd *n);

struct nhvd
{
	mlsp *network_streamer;

	hvd *hardware_decoder;
	AVFrame *frame;
	std::mutex frame_mutex;

	thread network_thread;
	bool keep_working;

	nhvd(): network_streamer(NULL), hardware_decoder(NULL), frame(NULL), keep_working(true) {}
};

struct nhvd *nhvd_init(const nhvd_net_config *net_config,const nhvd_hw_config *hw_config)
{
	mlsp_config mlsp_cfg={net_config->ip, net_config->port, net_config->timeout_ms};
	hvd_config hvd_cfg={hw_config->hardware, hw_config->codec, hw_config->device, hw_config->pixel_format};

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

	n->network_thread = thread(nhvd_network_decoder_thread, n);

	return n;
}

static void nhvd_network_decoder_thread(nhvd *n)
{
	hvd_packet packet={0};
	mlsp_frame *streamer_frame;
	int error;

	//dummy
	int FRAMERATE=48;
	//const useconds_t useconds_per_frame = 1000000/FRAMERATE;

	
	const useconds_t useconds_per_frame =  10 * 1000;

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
		std::lock_guard<std::mutex> frame_guard(n->frame_mutex);
		av_frame_unref(n->frame);
		av_frame_ref(n->frame, frame);
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

	av_frame_free(&n->frame);

	delete n;
}
