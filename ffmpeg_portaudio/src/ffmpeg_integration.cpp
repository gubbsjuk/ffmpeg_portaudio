#include "ffmpeg_integration.h"
#include <iostream>

ffmpeg_integration::ffmpeg_integration(const char* filePath)
{
	ad = new av_data();
	ad->filePath = filePath;
	ad->parse_pid = new std::thread(&ffmpeg_integration::parse_packets, this, ad);
}

void ffmpeg_integration::parse_packets(av_data* ad)
{
	AVPacket pkt1, * packet = &pkt1; // Clever way to allocate on stack.
	ad->pFormatCtx = avformat_alloc_context();

	avformat_open_input(&ad->pFormatCtx, ad->filePath, NULL, NULL);
	avformat_find_stream_info(ad->pFormatCtx, NULL);
	av_dump_format(ad->pFormatCtx, 0, ad->filePath, 0);

	for (unsigned int i = 0; i < ad->pFormatCtx->nb_streams; i++)
	{
		if (ad->pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)		ad->video_streamID = i;
		if (ad->pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)		ad->audio_streamID = i;
	}

	if (ad->video_streamID >= 0)	open_stream_components(ad->pFormatCtx, ad->video_streamID);
	if (ad->audio_streamID >= 0)	open_stream_components(ad->pFormatCtx, ad->audio_streamID);

	while (1)
	{
		//SJEKK QSize

		if (av_read_frame(ad->pFormatCtx, packet) < 0)
		{
			if (ad->pFormatCtx->pb->error == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));  //WTF? 
				continue;
			}
			else {
				std::cout << "Error in av_read_frame in decode_thread.\n";
				break;
			}
		}
		if (packet->stream_index == ad->video_streamID)
			ad->video_q.packet_queue_put(packet);
		else if (packet->stream_index == ad->audio_streamID)
			ad->audio_q.packet_queue_put(packet);
		else
			av_packet_unref(packet);
	}

	while (!ad->quit)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int ffmpeg_integration::open_stream_components(AVFormatContext* fmt_ctx, int stream_index)
{
	AVCodecParameters* codec_par;
	AVCodecContext* codec_ctx;
	AVCodec* codec;

	if (stream_index < 0 || stream_index >= fmt_ctx->nb_streams)
		return -1;

	codec_par = fmt_ctx->streams[stream_index]->codecpar;
	codec = avcodec_find_decoder(codec_par->codec_id);
	codec_ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(codec_ctx, codec_par);

	avcodec_open2(codec_ctx, codec, NULL);

	switch (codec_ctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		//TODO Better way to do this? going out of scope.
	{
		ad->audio_streamID = stream_index;
		ad->audio_ctx = codec_ctx;
		PacketQueue::packet_queue_init(&ad->audio_q);
		audio_decoder* audio_dec = new audio_decoder(ad);
		break;
	}
	case AVMEDIA_TYPE_VIDEO:
		ad->video_streamID = stream_index;
		ad->video_ctx = codec_ctx;
		PacketQueue::packet_queue_init(&ad->video_q);
		break;
	default:
		break;
	}
}