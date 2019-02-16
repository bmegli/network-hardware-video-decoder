# NHVD Network Hardware Video Decoder C++ library

Library for hardware video decoding over custom network protocol.

The intent behind library:
- minimize video latency
- minimize CPU usage (hardware decoding and color conversions)
- create simple interface (originally for interfacing from Unity)

## Using

```C++
	nhvd_hw_config hw_config= {"vaapi", "h264", "/dev/dri/renderD128", "bgr0"};
	nhvd_net_config net_config= {NULL, 9765, 500};

	nhvd *network_decoder=nhvd_init(&net_config, &hw_config);
		
	uint8_t *data;
	int width, height, linesize;
	
	while(keep_working)
	{
		data = nhvd_get_frame_begin(network_decoder, &width, &height, &linesize);
		
		if(data != NULL)
		{
			//...
			//do something with the data, be quick - we are holding the mutex
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


