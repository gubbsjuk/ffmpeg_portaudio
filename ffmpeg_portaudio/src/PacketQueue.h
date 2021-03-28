#pragma once
#include <mutex>

extern "C"
{
#include "libavformat/avformat.h"
}

class PacketQueue
{
	AVPacketList* first_pkt, * last_pkt;
	int nb_packets;
	int size;
	std::mutex* mutex;
	std::condition_variable* cond;
public:
	/*
	PacketQueue must be initialized with this call before use.
	TODO: Optimalize away?
	*/
	static void packet_queue_init(PacketQueue* q);

	/*
	Adds AVPacket to queue.
	*/
	int packet_queue_put(AVPacket* pkt);

	/*
	Retrieves one AVPacket from queue.
	*/
	int packet_queue_get(AVPacket* pkt);

	/*
	Gets current amount of packets in queue.
	*/
	inline int get_nb_packets() { return nb_packets; }
};
