/*
 * NHVD Network Hardware Video Decoder C++ library header
 *
 * Copyright 2019 (C) Bartosz Meglicki <meglickib@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */
 
#ifndef NHVD_H
#define NHVD_H

#include <stdint.h>

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

//internal library data
struct nhvd;
	
struct nhvd_hw_config
{
	const char *hardware; //!< hardware type for decoding, e.g. "vaapi"
	const char *codec; //!< codec name, e.g. "h264", "vp8"
	const char *device; //!< NULL/empty string or device, e.g. "/dev/dri/renderD128"
	const char *pixel_format; //!< NULL for default or format, e.g. "rgb0", "bgr0", "nv12", "yuv420p"
};

struct nhvd_net_config
{
	const char *ip; // IP (to listen on) or NULL (listen on any)
	uint16_t port; // server port
	int timeout_ms; //0 ar positive number
};

enum nhvd_retval_enum
{
	NHVD_ERROR=-1, //!< error occured 
	NHVD_OK=0, //!< succesfull execution
};

NHVD_EXPORT NHVD_API struct nhvd *nhvd_init(const nhvd_net_config *net_config, const nhvd_hw_config *hw_config);
NHVD_EXPORT NHVD_API void nhvd_close(nhvd *n);

NHVD_EXPORT NHVD_API uint8_t *nhvd_get_frame_begin(nhvd *n, int *w, int *h, int *s);

//returns HVE_OK on success, HVE_ERROR on fatal error
NHVD_EXPORT NHVD_API int nhvd_get_frame_end(nhvd *n);

}

#endif