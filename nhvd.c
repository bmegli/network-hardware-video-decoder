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

#include <stdio.h>

static int nhvd_decode_frame(struct nhvd *n, struct hvd_packet* packet);
static struct nhvd *nhvd_close_and_return_null(struct nhvd *n, const char *msg);
static int NHVD_ERROR_MSG(const char *msg);

struct nhvd
{
	struct mlsp *network_streamer;

	struct hvd *hardware_decoder[NHVD_MAX_DECODERS];
	int hardware_decoders_size;
	int auxiliary_channels_size;

	AVFrame *frame[NHVD_MAX_DECODERS];
};

struct nhvd *nhvd_init(
	const struct nhvd_net_config *net_config,
	const struct nhvd_hw_config *hw_config, int hw_size, int aux_size)
{
	struct nhvd *n, zero_nhvd = {0};
	struct mlsp_config mlsp_cfg={net_config->ip, net_config->port, net_config->timeout_ms, hw_size + aux_size};

	if(hw_size > NHVD_MAX_DECODERS)
		return nhvd_close_and_return_null(NULL, "the maximum number of decoders (compile time) exceeded");

	if( ( n = (struct nhvd*)malloc(sizeof(struct nhvd))) == NULL )
		return nhvd_close_and_return_null(NULL, "not enough memory for nhvd");

	*n = zero_nhvd;

	if( (n->network_streamer = mlsp_init_server(&mlsp_cfg)) == NULL )
		return nhvd_close_and_return_null(n, "failed to initialize network server");

	n->hardware_decoders_size = hw_size;
	n->auxiliary_channels_size = aux_size;

	for(int i=0;i<hw_size;++i)
	{
		struct hvd_config hvd_cfg={hw_config[i].hardware, hw_config[i].codec, hw_config[i].device,
		hw_config[i].pixel_format, hw_config[i].width, hw_config[i].height, hw_config[i].profile};

		if( (n->hardware_decoder[i] = hvd_init(&hvd_cfg)) == NULL )
			return nhvd_close_and_return_null(n, "failed to initalize hardware decoder");
	}

	return n;
}

void nhvd_close(struct nhvd *n)
{
	if(n == NULL)
		return;

	mlsp_close(n->network_streamer);

	for(int i=0;i<n->hardware_decoders_size;++i)
		hvd_close(n->hardware_decoder[i]);

	free(n);
}

int nhvd_receive(struct nhvd *n, AVFrame *frames[])
{
	return nhvd_receive_all(n, frames, NULL);
}

int nhvd_receive_all(struct nhvd *n, AVFrame *frames[], struct nhvd_frame *raws)
{
	struct hvd_packet packets[NHVD_MAX_DECODERS] = {0};
	const struct mlsp_frame *streamer_frame;
	int error;

	if( (streamer_frame = mlsp_receive(n->network_streamer, &error)) == NULL)
	{
		if(error == MLSP_TIMEOUT)
		{
			fprintf(stderr, ".");
			nhvd_decode_frame(n, NULL);
			return NHVD_TIMEOUT;
		}
		return NHVD_ERROR_MSG("error while receiving frame");
	}

	for(int i=0;i<n->hardware_decoders_size;++i)
	{
		packets[i].data = streamer_frame[i].data;
		packets[i].size = streamer_frame[i].size;
	}

	if (nhvd_decode_frame(n, packets) != NHVD_OK)
		return NHVD_ERROR;

	for(int i=0;i<n->hardware_decoders_size;++i)
		frames[i] = n->frame[i];

	if(raws)
		for(int i=0;i < n->hardware_decoders_size + n->auxiliary_channels_size;++i)
		{
			raws[i].data = streamer_frame[i].data;
			raws[i].size = streamer_frame[i].size;
		}

	return NHVD_OK;
}

//NULL packet to flush all hardware decoders
static int nhvd_decode_frame(struct nhvd *n, struct hvd_packet *packet)
{
	int error = 0;

	for(int i=0;i<n->hardware_decoders_size;++i)
		n->frame[i] = NULL;

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
			return NHVD_ERROR_MSG("error during decoding");
	}

	//receive data from all hardware decoders
	for(int i=0;i<n->hardware_decoders_size;++i)
	{
		if(packet && !packet[i].size) //silently skip empty subframes
			continue; //(e.g. different framerates/B frames)

		//non NULL packet - get single frame
		//NULL packet - flush the decoder, work until hardware is flushed
		do
			n->frame[i] = hvd_receive_frame(n->hardware_decoder[i], &error);
		while(!packet && n->frame[i]);

		if(error != NHVD_OK)
			return NHVD_ERROR_MSG("error after decoding");
	}

	return NHVD_OK;
}

static struct nhvd *nhvd_close_and_return_null(struct nhvd *n, const char *msg)
{
	if(msg)
		fprintf(stderr, "nhvd: %s\n", msg);

	nhvd_close(n);

	return NULL;
}

static int NHVD_ERROR_MSG(const char *msg)
{
	fprintf(stderr, "nhvd: %s\n", msg);
	return NHVD_ERROR;
}
