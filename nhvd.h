/*
 * NHVD Network Hardware Video Decoder C++ library header
 *
 * Copyright 2019-2020 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef NHVD_H
#define NHVD_H

#include <stdint.h>

/**
 ******************************************************************************
 *
 *  \mainpage NHVD documentation
 *  \see https://github.com/bmegli/network-hardware-video-decoder
 *
 *  \copyright  Copyright (C) 2019-2020 Bartosz Meglicki
 *  \file       nhvd.h
 *  \brief      Library public interface header
 *
 ******************************************************************************
 */

#ifdef __cplusplus
extern "C" {
#endif

//we expose FFmpeg AVFrame type
#include <libavutil/frame.h>

/** \addtogroup interface Public interface
 *  @{
 */

enum NHVD_COMPILE_TIME_CONSTANTS
{
	NHVD_MAX_DECODERS = 3, //!< max number of decoders in multi decoding
};

/**
 * @struct nhvd
 * @brief Internal library data passed around by the user.
 * @see nhvd_init, nhvd_close
 */
struct nhvd;

/**
 * @struct nhvd_net_config
 * @brief Network configuration.
 *
 * For more details see:
 * <a href="https://github.com/bmegli/minimal-latency-streaming-protocol">MLSP</a>
 *
 * @see nhvd_init
 */
struct nhvd_net_config
{
	const char *ip; //!< IP (to listen on) or NULL (listen on any)
	uint16_t port; //!< server port
	int timeout_ms; //!< 0 ar positive number
};

/**
 * @struct nhvd_hw_config
 * @brief Hardware decoder configuration.
 *
 * For more details see:
 * <a href="https://bmegli.github.io/hardware-video-decoder/structhvd__config.html">HVD documentation</a>
 *
 * @see nhvd_init
 */
struct nhvd_hw_config
{
	const char *hardware; //!< hardware type for decoding, e.g. "vaapi"
	const char *codec; //!< codec name, e.g. "h264", "vp8"
	const char *device; //!< NULL/empty string or device, e.g. "/dev/dri/renderD128"
	const char *pixel_format; //!< NULL for default or format, e.g. "rgb0", "bgr0", "nv12", "yuv420p"
	int width; //!< 0 to not specify, needed by some codecs
	int height; //!< 0 to not specify, needed by some codecs
	int profile; //!< 0 to leave as FF_PROFILE_UNKNOWN or profile e.g. FF_PROFILE_HEVC_MAIN, ...
};

/**
 * @struct nhvd_frame
 * @brief Raw received data frame
 *
 * Raw data returned along decoded data (nhvd_receive_all).
 *
 * @see nhvd_receive_all
 */
struct nhvd_frame
{
	uint8_t *data; //!< pointer to encoded data
	int size; //!< size of encoded data
};

/**
  * @brief Constants returned by most of library functions
  */
enum nhvd_retval_enum
{
	NHVD_TIMEOUT=-2, //!< timeout on receive
	NHVD_ERROR=-1, //!< error occured
	NHVD_OK=0, //!< succesfull execution
};

/**
 * @brief Initialize internal library data.
 *
 * Initialize streaming and single or multiple (hw_size > 1) hardware decoders
 * and (aux_size > 1) auxiliary non-video raw data channels.
 *
 * @param net_config network configuration
 * @param hw_config hardware decoders configuration of hw_size size
 * @param hw_size number of supplied hardware decoder configurations
 * @param aux_size number of auxiliary non-video raw data channels
 * @return
 * - pointer to internal library data
 * - NULL on error, errors printed to stderr
 *
 * @see nhvd_net_config, nhvd_hw_config
 */
struct nhvd *nhvd_init(
	const struct nhvd_net_config *net_config,
	const struct nhvd_hw_config *hw_config, int hw_size, int aux_size);

/**
 * @brief Free library resources
 *
 * Cleans and frees library memory.
 *
 * @param n pointer to internal library data
 * @see nhvd_init
 *
 */
void nhvd_close(struct nhvd *n);

/**
 * @brief Receive and decode next set of frames
 *
 * Function blocks until next set of frames is received and decoded or timeout occurs.
 * The number of frames in the set depends on hw_size argument passed to nhvd_init.
 * Typically this is just a single frame (hw_size == 1).
 *
 * Note that this function may return NHVD_OK and NULL for some (or all) frames.
 * This happens when library received frame(s) data but was unable to decode
 * some of the frames yet (e.g. waiting for the keyframe).
 *
 * The ownership of FFmpeg AVFrame* set remains with the library and
 * is valid only to the next call of nhvd_receive so:
 * - consume it immidiately
 * - or reference the data with av_frame_ref
 * - or copy (not recommended)
 *
 * For AVFrame you are mainly interested in its data and linesize arrays.
 *
 * If the function returns NHVD_TIMEOUT you may immidiately proceed with
 * next nhvd_receive. The hardware is flushed and network prepared for new
 * streaming sequence.
 *
 *
 * @param n pointer to internal library data
 * @param frames array of AVFrame* of size matching nhvd_init hw_size
 * @return
 * - NHVD_OK on success
 * - NHVD_ERROR on error
 * - NHVD_TIMEOUT on receive timeout
 *
 * @see nhvd_init
 *
 *
 */
int nhvd_receive(struct nhvd *n, AVFrame *frames[]);

/**
 * @brief Receive next set of frames with both decoded and encoded data
 *
 * Function blocks until next set of frames is received and decoded or timeout occurs.
 * The number of frames in the set depends on hw_size argument passed to nhvd_init.
 * Typically this is just a single frame (hw_size == 1).
 *
 * Note that this function may return NHVD_OK and NULL for some (or all) frames.
 * This happens when library received frame(s) data but was unable to decode
 * some of the frames yet (e.g. waiting for the keyframe).
 *
 * The ownership of FFmpeg AVFrame* set remains with the library and
 * is valid only until next call to nhvd_receive so:
 * - consume it immidiately
 * - or reference the data with av_frame_ref
 * - or copy (not recommended)
 *
 * For AVFrame you are mainly interested in its data and linesize arrays.
 *
 * The number of raws in the set depends on hw_size + aux_size passed to nhvd_init.
 * Auxiliary channels follow video channels.
 * Typically there are no auxiliary channels (aux_size == 0).
 *
 * Note that it is possible to get auxiliary nvhd_frame with 0 size
 * if that is what the sender decided to send (e.g. missing aux data for some frames).
 *
 * The ownership of raws nhvd_frame set data remains with the library and
 * is valid only until next call to nhvd_recive so:
 * - consume it immidiately
 * - or copy if necessary
 *
 * If the function returns NHVD_TIMEOUT you may immidiately proceed with
 * next nhvd_receive. The hardware is flushed and network prepared for new
 * streaming sequence.
 *
 * @param n pointer to internal library data
 * @param frames array of AVFrame* of size matching nhvd_init hw_size
 * @param raws array of nhvd_frame of size matching nhvd_init hw_size + aux_size
 * @return
 * - NHVD_OK on success
 * - NHVD_ERROR on error
 * - NHVD_TIMEOUT on receive timeout
 *
 * @see nhvd_init
 *
 */
int nhvd_receive_all(struct nhvd *n, AVFrame *frames[], struct nhvd_frame *raws);


/** @}*/

#ifdef __cplusplus
}
#endif

#endif
