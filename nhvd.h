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

// API compatible with C99 on various platorms
// Compatible with Unity Native Plugins
#if defined(__CYGWIN32__)
    #define NHVD_API __stdcall
    #define NHVD_EXPORT __declspec(dllexport)
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WINAPI_FAMILY)
    #define NHVD_API __stdcall
    #define NHVD_EXPORT __declspec(dllexport)
#elif defined(__MACH__) || defined(__ANDROID__) || defined(__linux__)
    #define NHVD_API
    #define NHVD_EXPORT
#else
    #define NHVD_API
    #define NHVD_EXPORT
#endif

extern "C"{

/** \addtogroup interface Public interface
 *  @{
 */

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
 * @struct nhvd_depth_config
 * @brief Depth unprojection configuration.
 *
 * For more details see:
 * <a href="https://github.com/bmegli/hardware-depth-unprojector">HDU</a>
 *
 * @see nhvd_init
 */
struct nhvd_depth_config
{
	float ppx; //!< principal point x pixel coordinates (center of projection)
	float ppy; //!< principal point y pixel coordinates (center of projection)
	float fx; //!< focal length in pixel width unit
	float fy; //!< focal length in pixel height unit
	float depth_unit; //!< multiplier for raw depth data;
};

enum {NHVD_NUM_DATA_POINTERS = 3};

/**
 * @struct nhvd_frame
 * @brief Video frame abstraction.
 *
 * Note that video data is often processed in multiplanar formats.
 * This may mean for example separate planes (arrays) for:
 * - luminance data
 * - color data
 *
 * @see nhvd_get_frame_begin, nhvd_get_frame_end, nhvd_get_begin, nhvd_get_end
 */
struct nhvd_frame
{
	int width; //!< width of frame in pixels
	int height; //!< height of frame in pixels
	int format; //!< FFmpeg pixel format
	uint8_t *data[NHVD_NUM_DATA_POINTERS]; //!< array of pointers to frame planes (e.g. Y plane and UV plane)
	int linesize[NHVD_NUM_DATA_POINTERS]; //!< array of strides of frame planes (row length including padding)
};

/**
  * @brief Vertex data (x, y, z)
  */
typedef float float3[3];

/**
  * @brief Vertex color data (rgba), currently only a as greyscale
  */
typedef uint32_t color32;

/**
 * @struct nhvd_point_cloud
 * @brief Point cloud abstraction.
 *
 * Array of float3 points and color32 colors. Only used points are non zero.
 *
 * @see nhvd_get_point_cloud_begin, nhvd_get_point_cloud_end, nhvd_get_begin, nhvd_get_end
 */
struct nhvd_point_cloud
{
	float3 *data; //!< array of point coordinates
	color32 *colors; //!< array of point colors
	int size; //!< size of array
	int used; //!< number of elements used in array
};

/**
  * @brief Constants returned by most of library functions
  */
enum nhvd_retval_enum
{
	NHVD_ERROR=-1, //!< error occured
	NHVD_OK=0, //!< succesfull execution
};

/**
 * @brief Initialize internal library data.
 *
 * For video streaming the argument depth_config should be NULL.
 * Non NULL depth_config enables depth unprojection (point cloud streaming).
 *
 * @param net_config network configuration
 * @param hw_config hardware decoder configuration
 * @param depth_config unprojection configuration (may be NULL)
 * @return
 * - pointer to internal library data
 * - NULL on error, errors printed to stderr
 *
 * @see nhvd_net_config, nhvd_hw_config, nhvd_depth_config
 */
NHVD_EXPORT NHVD_API struct nhvd *nhvd_init(
	const nhvd_net_config *net_config,
	const nhvd_hw_config *hw_config,
	const nhvd_depth_config *depth_config);

/**
 * @brief Free library resources
 *
 * Cleans and frees library memory.
 *
 * @param n pointer to internal library data
 * @see nhvd_init
 *
 */
NHVD_EXPORT NHVD_API void nhvd_close(nhvd *n);


/** @name Data retrieval functions
 *
 *  nhvd_xxx_begin functions should be always followed by corresponding nhvd_xxx_end calls.
 *  A mutex is held between begin and end function so be as fast as possible.
 *
 *  The ownership of the data remains with the library. You should consume the data immidiately
 *  (e.g. fill the texture, fill the vertex buffer). The data is valid only until call to corresponding end
 *  function.
 *
 *  Functions will calculate point clouds from depth maps only if non NULL ::nhvd_depth_config was passed to ::nhvd_init
 *
 * @param n pointer to internal library data
 * @param frame pointer to frame description data
 * @param pc pointer to point cloud description data
 * @return
 * - begin functions
 * 	- NHVD_OK sucessfully returned new data
 * 	- NHVD_ERROR no new data
 * - end functions
 *		- NHVD_OK sucessfully finished begin/end block
 *		- NHVD_ERROR fatal error occured
 *
 * @see nhvd_frame, nhvd_point_cloud
 *
 */
///@{
/** @brief Retrieve depth frame and point cloud.
 *
 * Point cloud data may be only retrieved if non NULL ::nhvd_depth_config was passed to ::nhvd_init.
 * This function may retrieve both depth frame and unprojected point cloud at the same time
 */
NHVD_EXPORT NHVD_API int nhvd_get_begin(nhvd *n, nhvd_frame *frame, nhvd_point_cloud *pc);
/** @brief Finish retrieval. */
NHVD_EXPORT NHVD_API int nhvd_get_end(nhvd *n);
/** @brief Retrieve video frame. */
NHVD_EXPORT NHVD_API int nhvd_get_frame_begin(nhvd *n, nhvd_frame *frame);
/** @brief Finish retrieval. */
NHVD_EXPORT NHVD_API int nhvd_get_frame_end(nhvd *n);
/** @brief Retrieve point cloud. */
NHVD_EXPORT NHVD_API int nhvd_get_point_cloud_begin(nhvd *n, nhvd_point_cloud *pc);
/** @brief Finish retrieval. */
NHVD_EXPORT NHVD_API int nhvd_get_point_cloud_end(nhvd *n);
///@}

/** @}*/
}

#endif
