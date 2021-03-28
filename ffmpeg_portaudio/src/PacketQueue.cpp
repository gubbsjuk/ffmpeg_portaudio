#include "PacketQueue.h"
#include <iostream>

#define BLOCKING 1

void PacketQueue::packet_queue_init(PacketQueue* q)
{
	q->mutex = new std::mutex();
	q->cond = new std::condition_variable();
}

/*

*/
int PacketQueue::packet_queue_put(AVPacket* pkt)
{
	AVPacketList* pktList;
	AVPacket pkt1, * tempPacket = &pkt1;

	//Duplicate the packet
	if (av_packet_ref(tempPacket, pkt) < 0) {
		std::cout << "Error setting up new refrence to packet";
		return -1;
	}

	pktList = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (!pktList)
		return -1;

	pktList->pkt = *tempPacket;
	pktList->next = NULL;

	std::unique_lock<std::mutex> mlock(*mutex);

	if (!last_pkt)
		first_pkt = pktList;
	else
		last_pkt->next = pktList;

	last_pkt = pktList;
	nb_packets++;
	size += pktList->pkt.size; // + sizeof(*pktList);
	cond->notify_all();
	mlock.unlock();
	return 0;
}

int PacketQueue::packet_queue_get(AVPacket* pkt)
{
	// TODO: Rewrite as either blocking or not.
	AVPacketList* pktList;
	int ret;
	std::unique_lock<std::mutex> mlock(*mutex);

	while (1)
	{
		pktList = first_pkt;
		if (pktList)
		{
			first_pkt = pktList->next;
			if (!first_pkt)
				last_pkt = NULL;
			nb_packets--;
			size -= pktList->pkt.size;
			*pkt = pktList->pkt;
			av_free(pktList);
			ret = 1;
			break;
		}
		else if (!BLOCKING)
		{
			ret = 0;
			break;
		}
		else
		{
			cond->wait(mlock);
		}
	}
	mlock.unlock();
	return ret;
}