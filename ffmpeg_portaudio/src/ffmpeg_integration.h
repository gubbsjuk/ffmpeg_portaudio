#pragma once
#include "audio_decoder.h"
#include "av_data.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <time.h>
}

class ffmpeg_integration
{
	/*
	Thread for parsing packets
	*/
	void parse_packets(av_data* ad);

	/*
	Allocates ffmpeg format context and opens input.
	If pre-processor statement DEBUG is defined dumps format.
	*/
	void initialize_ffmpeg(av_data* ad);

	/*
	Loop reading inn all frames of file and adding them to their respective packet queues.
	*/
	void read_frames(av_data* ad);
	
	/*
	Generates decoding thread for each stream component and initializes the respective PacketQueues.
	Returns -1 on error 0 on success.
	*/
	int open_stream_components(AVFormatContext* fmt_ctx, int stream_index);

	av_data* ad;
	audio_decoder* audio_dec = nullptr;

public:
	ffmpeg_integration(const char* filePath);
	~ffmpeg_integration();
};
