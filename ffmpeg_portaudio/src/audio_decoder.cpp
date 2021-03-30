#include "audio_decoder.h"
#include <malloc.h>
#include <stdint.h>
#include <iostream>

#define RING_BUF_SIZE (1024*2*4)

//TODO: stop stream after buffer has been emptied.

audio_decoder::audio_decoder(av_data* ad) :
	m_ad(ad)
{
	init_port_audio(ad);
	init_audio_q(ad);

	/*
	* Get channel layout.
	* If not specified in ctx, get default for ch count.
	*/
	int64_t chLayout = ad->audio_ctx->channel_layout;
	if (chLayout == 0)
		chLayout = av_get_default_channel_layout(ad->audio_ctx->channels);

	ad->audio_swr = swr_alloc_set_opts(NULL, chLayout, AV_SAMPLE_FMT_FLT, ad->audio_ctx->sample_rate, chLayout, ad->audio_ctx->sample_fmt, ad->audio_ctx->sample_rate, 0, NULL);
	if (!ad->audio_swr)
		return;
	int ret = swr_init(ad->audio_swr);
	if (ret < 0)
		handle_av_error(ret);

	ad->audio_decoder_pid = new std::thread(&audio_decoder::decode_thread, this, ad);
}

audio_decoder::~audio_decoder()
{
	swr_free(&m_ad->audio_swr);
}

void audio_decoder::decode_thread(av_data* ad)
{
	AVPacket packet, * pkt = &packet;
	AVFrame* audio_frame = av_frame_alloc();
	uint8_t** dstBuffer = (uint8_t**)av_malloc(sizeof(float) * 2048);
	int ret = 0;
	int sampleCount;

	// main decode loop
	while (1)
	{
		// Better way to do this?
		if (ad->quit && ad->audio_q.get_nb_packets() < 1)
		{
			std::cout << "Done decoding" << std::endl;
			break;
		}

		while (ad->audio_q.get_nb_packets() < 1)
			std::this_thread::sleep_for(std::chrono::milliseconds(10)); //replace with condition?

		ad->audio_q.packet_queue_get(pkt);

		avcodec_send_packet(ad->audio_ctx, pkt); // Error handling is handled by the avcodec_recieve_frame
		ret = receive_frame(ad->audio_ctx, audio_frame);
		if (ret == -1)

			continue;
		else if (ret < -1)
			break;

		sampleCount = audio_frame->nb_samples;
		//Convert here if needed
		if (ad->audio_ctx->sample_fmt != AV_SAMPLE_FMT_FLT)
		{
			sampleCount = convert_buffer(ad, audio_frame, dstBuffer);
			if (sampleCount < 0)
				std::cout << "error while converting samples." << std::endl;
		}
		else {
			dstBuffer = audio_frame->extended_data;
		}

		// Wait until ringbuffer can hold new data.
		while (PaUtil_GetRingBufferWriteAvailable(&ad->audio_buf) < sampleCount * audio_frame->channels)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		ret = PaUtil_WriteRingBuffer(&ad->audio_buf, *dstBuffer, sampleCount * audio_frame->channels);

		av_frame_unref(audio_frame);
	}
	av_free(dstBuffer);
	av_frame_free(&audio_frame);
}

int audio_decoder::receive_frame(AVCodecContext* audio_ctx, AVFrame* audio_frame)
{
	int ret = avcodec_receive_frame(audio_ctx, audio_frame);
	if (ret == AVERROR(EAGAIN))
		return -1; // output is not available in this state - user must try to send new input
	else if (ret == AVERROR_EOF)
		return -2; // the decoder has been fully flushed, and there will be no more output frames
	else if (ret == AVERROR(EINVAL))
		return -3; // codec not opened TODO: ASSERT?
	else if (ret < 0)
		return -4; // legitimate decoding error.
	return 0;
}

int audio_decoder::convert_buffer(av_data* ad, AVFrame* audio_frame, uint8_t** dstBuffer)
{
	int ret;
	int dst_linesize;

	// Get samplecount after conversion. (Should be equal when not doing samplerate conversion?)
	const int outSampleCount = swr_get_out_samples(ad->audio_swr, audio_frame->nb_samples);
	if (outSampleCount < 0)
		std::cout << "Error calculating out-sample-count" << std::endl;

	// Allocated outputbuffer. Try with av_samples_alloc aswell.
	ret = av_samples_alloc(dstBuffer, &dst_linesize, ad->audio_ctx->channels, outSampleCount, AV_SAMPLE_FMT_FLT, 1); // TODO: Fix memory-leak. Is this the cause?
	if (ret < 0)
		std::cout << "Could not allocate destination samples." << std::endl;

	// Convert current frame.
	ret = swr_convert(ad->audio_swr, dstBuffer, outSampleCount, (const uint8_t**)audio_frame->extended_data, audio_frame->nb_samples);
	if (ret < 0)
		handle_av_error(ret);

	return ret;
}

int audio_decoder::init_audio_q(av_data* ad)
{
	float* bufloc = new float[RING_BUF_SIZE];
	PaUtil_InitializeRingBuffer(&ad->audio_buf, sizeof(float), RING_BUF_SIZE, bufloc);
	return 1;
}

PaError audio_decoder::init_port_audio(av_data* ad)
{
	PaError err;
	PaStream* stream;

	err = Pa_Initialize();
	if (err != paNoError)	handle_pa_error(err);

	int selectedDevice = select_portaudio_device();

	PaStreamParameters outputParameters;
	outputParameters.channelCount = ad->audio_ctx->channels;
	outputParameters.device = selectedDevice;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(selectedDevice)->defaultLowOutputLatency;

	err = Pa_OpenStream(&stream, NULL, &outputParameters, ad->audio_ctx->sample_rate, paFramesPerBufferUnspecified, paNoFlag, pa_audio_callback, ad);

	//err = Pa_OpenDefaultStream(&stream, 0, 1, paInt16, ad->audio_ctx->sample_rate, 1024, pa_audio_callback, &m_audio_buf);
	if (err != paNoError)	handle_pa_error(err);
	err = Pa_StartStream(stream);
	if (err != paNoError)	handle_pa_error(err);
	std::cout << "PortAudio stream started." << std::endl;

	return 1;
}

int audio_decoder::select_portaudio_device()
{
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
selectdevice:
	std::cout << "Select a device ID: ";
	std::cin >> selectedDevice;

	if (selectedDevice < 0 || selectedDevice > devcount - 1)
		goto selectdevice;

	return selectedDevice;
}

int audio_decoder::pa_audio_callback(const void* inputBuffer, void* outputBuffer, unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData)
{
	av_data* ad = (av_data*)userData;
	uint8_t* out = (uint8_t*)outputBuffer;
	(void)inputBuffer;

	int readAvailable = PaUtil_GetRingBufferReadAvailable(&ad->audio_buf);
	if (readAvailable == 0 && ad->audio_q.get_nb_packets() == 0 && ad->quit)
		return paAbort;
	if (readAvailable < frameCount)
		return paContinue;

	PaUtil_ReadRingBuffer(&ad->audio_buf, outputBuffer, frameCount * ad->audio_ctx->channels);
	return paContinue;
}

void audio_decoder::handle_pa_error(PaError err)
{
	std::cout << "PortAudio encountered an error:" << std::endl;
	std::cout << Pa_GetErrorText(err) << std::endl;
}

void audio_decoder::handle_av_error(int av_error)
{
	char errtxt[64];
	std::cout << av_make_error_string(errtxt, 64, av_error);
}