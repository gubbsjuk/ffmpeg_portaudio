#pragma once
#include <string>
#include "audio_decoder.h"
#include "PacketQueue.h"
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
	void parse_packets(av_data* ad);
	void initialize();
	int open_stream_components(AVFormatContext* fmt_ctx, int stream_index);

	av_data* ad;

	//int init_audio_q();
	//int init_video_q();

public:
	ffmpeg_integration(const char* filePath);
};
