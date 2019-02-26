# NHVD Network Hardware Video Decoder C++ library

Library for hardware video decoding from custom [MLSP](https://github.com/bmegli/minimal-latency-streaming-protocol) network protocol.
See also twin [NHVE](https://github.com/bmegli/network-hardware-video-encoder) network encoder.

The intent behind library:
- minimize video latency
- minimize CPU usage (hardware decoding and color conversions)
- simple interface for interfacing from Unity

Unity calls `Update` function once per frame, preparing data for rendering.
If you have other workflow it may be difficult for you to use the library directly.

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

Works with system FFmpeg on Ubuntu 18.04 and doesn't on 16.04 (outdated FFmpeg).

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

# finally build the shared library
cd network-hardware-video-decoder
mkdir build
cd build
cmake ..
make
```

## Using

See [HVD](https://github.com/bmegli/hardware-video-decoder) docs for details about hardware configuration.

```C++
	nhvd_hw_config hw_config= {"vaapi", "h264", "/dev/dri/renderD128", "bgr0"};
	nhvd_net_config net_config= {NULL, 9765, 500};

	nhvd *network_decoder=nhvd_init(&net_config, &hw_config);

	//this is where we will get the decoded data	
	nhvd_frame frame;
	
	while(keep_working)
	{
		if( nhvd_get_frame(network_decoder, &frame) == NHVD_OK )
		{
			//...
			//do something with the:
			// - frame.width
			// - frame.height
			// - frame.format
			// - frame.data
			// - frame.linesize
			// be quick - we are holding the mutex
			// Examples:
			// - fill the texture
			// - copy for later use if you can't be quick
			//...
		}

		if( nhvd_get_frame_end(network_decoder) != NHVD_OK )
			break; //error occured

		//this should spin once per frame rendering
		//so wait until we are after rendering
	}
	 	
	nhvd_close(network_decoder);
```

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

Hardware decode & render in Unity from H.264 network stream [unity-network-hardware-video-decoder](https://github.com/bmegli/unity-network-hardware-video-decoder)

### Streaming data

You may stream Realsense camera data (sending side) from [realsense-network-hardware-video-encoder
](https://github.com/bmegli/realsense-network-hardware-video-encoder)

