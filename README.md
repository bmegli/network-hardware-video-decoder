# NHVD Network Hardware Video Decoder C library

Library for hardware video decoding and streaming over custom [MLSP](https://github.com/bmegli/minimal-latency-streaming-protocol) protocol.

See also twin [NHVE](https://github.com/bmegli/network-hardware-video-encoder) network encoder.

See [hardware-video-streaming](https://github.com/bmegli/hardware-video-streaming) for other related projects.

The intent behind library:
- minimize video latency
- minimize CPU usage (hardware decoding and color conversions)
- multi-frame streaming (e.g. depth + texture)
- auxiliary data channels (e.g. IMU, odometry, metadata)
- simple user interface

## Platforms 

Unix-like operating systems (e.g. Linux).
The dependency is through [MLSP](https://github.com/bmegli/minimal-latency-streaming-protocol) socket use (easily portable).

Tested on Ubuntu 18.04.

## Hardware

Tested with:
- Intel VAAPI compatible hardware decoders ([Quick Sync Video](https://ark.intel.com/Search/FeatureFilter?productType=processors&QuickSyncVideo=true))
- AMD/ATI VAAPI compatible hardware decoders
- VDPAU compatible hardware decoders (e.g. Nvidia GPU) 

Also implemented (but not tested):
- DirectX 9 Video Acceleration (dxva2)
- DirectX 11 Video Acceleration (d3d11va)
- VideoToolbox

## Dependencies

Library depends on:
- [HVD Hardware Video Decoder](https://github.com/bmegli/hardware-video-decoder)
	- FFmpeg `avcodec` and `avutil` (at least 3.4 version)
- [MLSP Minimal Latency Streaming Protocol](https://github.com/bmegli/minimal-latency-streaming-protocol)

HVD and MLSP are included as submodules so you only need to satifisy HVD dependencies.

Works with system FFmpeg on Ubuntu 18.04 and doesn't on 16.04 (outdated FFmpeg and VAAPI ecosystem).

## Building Instructions

Tested on Ubuntu 18.04.

``` bash
# update package repositories
sudo apt-get update 
# get avcodec and avutil
sudo apt-get install ffmpeg libavcodec-dev libavutil-dev
# get compilers and make 
sudo apt-get install build-essential
# get cmake - we need to specify libcurl4 for Ubuntu 18.04 dependencies problem
sudo apt-get install libcurl4 cmake
# get git
sudo apt-get install git
# clone the repository with *RECURSIVE* for submodules
git clone --recursive https://github.com/bmegli/network-hardware-video-decoder.git

# finally build the library
cd network-hardware-video-decoder
mkdir build
cd build
cmake ..
make
```

## Testing

Assuming:
- you are using VAAPI device "/dev/dri/renderD128"
- port is 9766
- codec is h264
- pixel format is nv12

### Receiving side

```bash
./nhvd-frame-example 9766 vaapi h264 nv12 /dev/dri/renderD128
# for decoding and dumping raw (encoded) data to file
#./nhvd-frame-raw-example 9766 vaapi h264 nv12 /dev/dri/renderD128
# for multi-frame streaming
#./nhvd-frame-multi-example 9766 vaapi h264 nv12 nv12 /dev/dri/renderD128
# for video + auxiliary channel (non-video)
#./nhvd-frame-aux-example 9766 vaapi h264 nv12 /dev/dri/renderD128
```

### Sending side

For a quick test you may use [NHVE](https://github.com/bmegli/network-hardware-video-encoder) procedurally generated H.264 video (recommended).

```bash
# assuming you build NHVE, in build directory
./nhve-stream-h264 127.0.0.1 9766 10 /dev/dri/renderD128
# for multi frame streaming
#./nhve-stream-multi 127.0.0.1 9766 10 /dev/dri/renderD128
# for video + auxiliary channel (non-video)
#./nhve-stream-h264-aux 127.0.0.1 9766 10 /dev/dri/renderD128
```

If you have Realsense camera you may use [realsense-network-hardware-video-encoder](https://github.com/bmegli/realsense-network-hardware-video-encoder).

```bash
# assuming you build RNHVE, in build directory
./realsense-nhve-h264 127.0.0.1 9766 infrared 640 360 30 10 /dev/dri/renderD128
```

If everything went well you will:
- see hardware initialized
- collected frames info
- decoded frames size

If you have multiple vaapi devices you may have to specify correct one e.g. "/dev/dri/renderD129"

## Using

See [HVD](https://github.com/bmegli/hardware-video-decoder) docs for details about hardware configuration.

See examples directory for more complete examples.

```C
	struct nhvd_hw_config hw_config = {"vaapi", "h264", "/dev/dri/renderD128", "bgr0"};
	struct nhvd_net_config net_config = {NULL, 9765, 500};

	struct nhvd *network_decoder = nhvd_init(&net_config, &hw_config, 1, 0);

	//this is where we will get the decoded data	
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
		
		//immidiatelly consume, reference or copy frame data
	}

	nhvd_close(network_decoder);
```

That's it!

The same interface works for multi-frame streaming with:
- array of hardware configurations in `nhvd_init`
- array of frames in `nhvd_receive`

With `nhvd_receive_all` you may:
- get both decoded and encoded data.
- use `nhvd_init` with `aux_size > 0` for non-video data channels

## License

Library and my dependencies are licensed under Mozilla Public License, v. 2.0

This is similiar to LGPL but more permissive:
- you can use it as LGPL in prioprietrary software
- unlike LGPL you may compile it statically with your code

Like in LGPL, if you modify this library, you have to make your changes available.
Making a github fork of the library with your changes satisfies those requirements perfectly.

Since you are linking to FFmpeg libraries consider also `avcodec` and `avutil` licensing.

## Additional information

### Library uses

Hardware decode & render in Unity from H.264/HEVC/HEVC Main10 network stream [unity-network-hardware-video-decoder](https://github.com/bmegli/unity-network-hardware-video-decoder)

### Streaming data

You may stream Realsense camera data (sending side) from [realsense-network-hardware-video-encoder
](https://github.com/bmegli/realsense-network-hardware-video-encoder)
