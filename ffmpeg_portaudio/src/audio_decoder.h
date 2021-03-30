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
	PaStream* stream;
	av_data* m_ad;

	void decode_thread(av_data* ad);

	/*
	Returns 0 on success.
	Returns -1 on EAGAIN
	returns <-1 on more serious error.

	TODO: Better way of cleaning this up?
	*/
	int receive_frame(AVCodecContext* audio_ctx, AVFrame* audio_frame);

	int convert_buffer(av_data* ad, AVFrame* audio_frame, uint8_t** dstBuffer);

	int init_audio_q(av_data* ad);

	PaError init_port_audio(av_data* ad);
	int select_portaudio_device();

	static int pa_audio_callback(const void* inputBuffer, void* outputbuffer, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);

	void handle_pa_error(PaError err);

	void handle_av_error(int av_error);

public:
	audio_decoder(av_data* av);
	~audio_decoder();
};
