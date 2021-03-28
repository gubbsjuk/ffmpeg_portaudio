#include "audio_decoder.h"
#include <malloc.h>
#include <stdint.h>
#include <iostream>

#define RING_BUF_SIZE (1024*2*4)
//#define RING_BUF_SIZE (262144)

audio_decoder::audio_decoder(av_data* ad)
{
	init_port_audio(ad);
	init_audio_q(ad);

	ad->audio_decoder_pid = new std::thread(&audio_decoder::decode_thread, this, ad);
}

void audio_decoder::decode_thread(av_data* ad)
{
	AVPacket packet, * pkt = &packet; // TODO: Bedre måte?
	AVFrame* audio_frame = av_frame_alloc();
	int ret = 0;
	int sampleCount;

	// main decode loop
	while (1)
	{
		while (ad->audio_q.get_nb_packets() < 1)
			std::this_thread::sleep_for(std::chrono::milliseconds(10)); //replace with condition?

		ad->audio_q.packet_queue_get(pkt);

		ret = avcodec_send_packet(ad->audio_ctx, pkt);

		ret = avcodec_receive_frame(ad->audio_ctx, audio_frame);

		if (ret == AVERROR(EAGAIN)) {
			std::cout << "EAGAIN";
			continue;
		}
		else if (ret == AVERROR_EOF) {
			std::cout << "EOF";
			break;
		}
		else if (ret == AVERROR(EINVAL))
			std::cout << "codec not opened";
		else if (ret < 0)
			std::cout << "legitimate decoding error.";

		sampleCount = audio_frame->nb_samples;
		//Convert here if needed
		bool convert = false;
		uint8_t** dstBuffer = (uint8_t**)av_malloc(sizeof(uint8_t) * 2048);
		if (convert)
		{
			sampleCount = convert_buffer(ad, audio_frame, dstBuffer);
			if (sampleCount < 0)
				std::cout << "error while converting samples." << std::endl;
		}

		// Conversion done
		while (PaUtil_GetRingBufferWriteAvailable(&ad->audio_buf) < sampleCount * audio_frame->channels)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		
		if (convert)
			ret = PaUtil_WriteRingBuffer(&ad->audio_buf, *dstBuffer, sampleCount * audio_frame->channels);
		else
			ret = PaUtil_WriteRingBuffer(&ad->audio_buf, *audio_frame->extended_data, sampleCount * audio_frame->channels);

		std::cout << "Wrote " << ret << " samples to ringbuffer. Read available: " << PaUtil_GetRingBufferReadAvailable(&ad->audio_buf) << std::endl;

		av_frame_unref(audio_frame);
		//av_freep(dstBuffer);
	}
}

int audio_decoder::convert_buffer(av_data* ad, AVFrame* audio_frame, uint8_t** dstBuffer)
{
	int ret;
	int dst_linesize;

	/*
	* Get channel layout.
	* If not specified in ctx, get default for ch count.
	*/
	int64_t chLayout = ad->audio_ctx->channel_layout;
	if (chLayout == 0)
		chLayout = av_get_default_channel_layout(ad->audio_ctx->channels);

	// Allocate conversion context and set options.
	SwrContext* swr = swr_alloc_set_opts(NULL, chLayout, AV_SAMPLE_FMT_FLTP, ad->audio_ctx->sample_rate, chLayout, ad->audio_ctx->sample_fmt, ad->audio_ctx->sample_rate, 0, NULL);
	if (!swr)
		std::cout << "Error creating conversion context." << std::endl;
	
	// Initialize conversion context.
	ret = swr_init(swr);
	char errtxt[64];
	if (ret < 0)
		std::cout << av_make_error_string(errtxt, 64, ret);

	// Get samplecount after conversion. (Should be equal when not doing samplerate conversion?)
	const int outSampleCount = swr_get_out_samples(swr, audio_frame->nb_samples);
	if (outSampleCount < 0)
		std::cout << "Error calculating out-sample-count" << std::endl;
	
	// Allocated outputbuffer. Try with av_samples_alloc aswell.
	ret = av_samples_alloc(dstBuffer, &dst_linesize, ad->audio_ctx->channels, outSampleCount, AV_SAMPLE_FMT_FLTP, 1);
	if (ret < 0)
		std::cout << "Could not allocate destination samples." << std::endl;

	// Convert current frame.
	ret = swr_convert(swr, dstBuffer, outSampleCount, (const uint8_t**)audio_frame->extended_data, audio_frame->nb_samples);
	if (ret < 0)
		std::cout << av_make_error_string(errtxt, 64, ret);

	return ret;
}

int audio_decoder::init_audio_q(av_data* ad)
{
	//PaUtilRingBuffer* audio_buf = (PaUtilRingBuffer*)malloc(sizeof(uint8_t) * RING_BUF_SIZE);
	int16_t* bufloc = new int16_t[RING_BUF_SIZE];

	PaUtil_InitializeRingBuffer(&ad->audio_buf, sizeof(int16_t), RING_BUF_SIZE, bufloc);
	return 1;
}

PaError audio_decoder::init_port_audio(av_data* ad)
{
	PaError err;
	PaStream* stream;

	err = Pa_Initialize();
	if (err != paNoError)	handle_error(err);

	int devcount = Pa_GetDeviceCount();
	const PaDeviceInfo* devInfo;
	const PaHostApiInfo* apiInfo;
	int apicount = Pa_GetHostApiCount();

	int defaultDevice = Pa_GetDefaultOutputDevice();
	devInfo = Pa_GetDeviceInfo(defaultDevice);
	std::cout << "Default output-device is ID: " << defaultDevice << " " << devInfo->name << std::endl;

	std::cout << "LISTING " << apicount << " APIs:" << std::endl;
	for (int i = 0; i < apicount; i++)
	{
		apiInfo = Pa_GetHostApiInfo(i);
		int apiDevices = apiInfo->deviceCount;
		std::cout << "Api ID: " << i << " " << apiInfo->name << " num devices: " << apiDevices << std::endl;
		for (int j = 0; j < apiDevices; j++)
		{
			int devIndex = Pa_HostApiDeviceIndexToDeviceIndex(i, j);
			devInfo = Pa_GetDeviceInfo(devIndex);
			std::cout << "\tDevice ID: " << devIndex << " Device Name: " << devInfo->name << std::endl;
		}
	}

	int selectedDevice;
	std::cout << "Select a device ID: ";
	std::cin >> selectedDevice;

	PaStreamParameters outputParameters;
	//outputParameters.channelCount = ad->audio_ctx->channels;
	outputParameters.channelCount = 2;
	outputParameters.device = selectedDevice;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	outputParameters.sampleFormat = paInt16;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(selectedDevice)->defaultLowOutputLatency;

	err = Pa_OpenStream(&stream, NULL, &outputParameters, ad->audio_ctx->sample_rate,paFramesPerBufferUnspecified, paNoFlag, pa_audio_callback, ad);

	//err = Pa_OpenDefaultStream(&stream, 0, 1, paInt16, ad->audio_ctx->sample_rate, 1024, pa_audio_callback, &m_audio_buf);
	if (err != paNoError)	handle_error(err);
	err = Pa_StartStream(stream);
	if (err != paNoError)	handle_error(err);
	std::cout << "PortAudio stream started." << std::endl;

	return 1;
}

int audio_decoder::pa_audio_callback(const void* inputBuffer, void* outputBuffer, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData)
{
	av_data* ad = (av_data*)userData;
	uint8_t* out = (uint8_t*)outputBuffer;
	(void)inputBuffer;
	int readAvailable = PaUtil_GetRingBufferReadAvailable(&ad->audio_buf);

	if (readAvailable < frameCount)
		return paContinue;

	PaUtil_ReadRingBuffer(&ad->audio_buf, outputBuffer, frameCount*ad->audio_ctx->channels);
	return paContinue;
}

void audio_decoder::handle_error(PaError err)
{
	std::cout << "PortAudio encountered an error:" << std::endl;
	std::cout << Pa_GetErrorText(err) << std::endl;
}