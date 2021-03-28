#pragma once

#include "portaudio.h"
#include "pa_util/pa_ringbuffer.h"
#include "PacketQueue.h"
#include "av_data.h"
#include <vector>
#include <fstream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
};

class audio_decoder
{
	std::ofstream myfile;
	PaStream* stream;

	void decode_thread(av_data* ad);

	int convert_buffer(av_data* ad, AVFrame* audio_frame, uint8_t** dstBuffer);

	int init_audio_q(av_data* ad);

	PaError init_port_audio(av_data* ad);
	void handle_error(PaError err);

	static int pa_audio_callback(const void* inputBuffer, void* outputbuffer, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);

public:
	audio_decoder(av_data* av);
};
