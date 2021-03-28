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
	static void packet_queue_init(PacketQueue* q);
	int packet_queue_put(AVPacket* pkt);
	int packet_queue_get(AVPacket* pkt);
	inline int get_nb_packets() { return nb_packets; }
};
