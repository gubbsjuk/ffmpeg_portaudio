#pragma once
struct av_data {
	PacketQueue audio_q;
	PacketQueue video_q;

	const char* filePath;
	int audio_streamID = -1;
	int video_streamID = -1;
	AVFormatContext* pFormatCtx;
	bool quit = false;
	std::thread* parse_pid;
	std::thread* audio_decoder_pid;
	AVCodecContext* audio_ctx;
	AVCodecContext* video_ctx;
	PaUtilRingBuffer audio_buf;
};