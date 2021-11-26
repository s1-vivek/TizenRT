/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * examples/hello/hello_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <stdio.h>
#include<string.h> 
#include<stdlib.h> 
#include<arpa/inet.h>
#include<sys/socket.h>
#include "../wifi_manager/wm_test/wm_test_log.h"

#define SERVER_IP CONFIG_LWIP_DHCPS_SERVER_IP
#define SERVER_PORT 8788
#define WT_BUF_SIZE 1024
#define TAG "[Hello]"
#define WT_ACK_SIZE 128

static int _recv_data_udp(int fd, int size, struct sockaddr_in *caddr) 
{
	char buf[WT_BUF_SIZE];
	int remain = size;
	unsigned int clen = sizeof(struct sockaddr_in);
	int cnt = 1;
	int total_received = 0;
	int total_ack_for_received = 0;
	WT_LOG(TAG, "_recv_data_udp start");
	while (remain > 0) {
		int read_size = remain > WT_BUF_SIZE ? WT_BUF_SIZE : remain;
		int nbytes = recvfrom(fd, buf, read_size, 0, (struct sockaddr*)caddr, &clen);
		if (nbytes == 0) {
			WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d", total_received);
			WT_LOGE(TAG, "connection closed");
			return -1;
		} else if (nbytes < 0) {
			if (errno == EWOULDBLOCK) {
				WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d", total_received);
				WT_LOGE(TAG, "timeout error %d", errno);
				return -1;
			} else {
				WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d", total_received);
				WT_LOGE(TAG, "connection error %d", errno);
				return -1;
			}
		}
		int nack;
		if(nbytes == WT_BUF_SIZE) {
			nack = sendto(fd, buf, WT_ACK_SIZE, 0, (struct sockaddr*)caddr, clen);
			if (nack == 0) {
				WT_LOG(TAG, "TOTAL BYTES SEND: %d", total_ack_for_received);
				WT_LOGE(TAG, "connection closed");
				return -1;
			} else if (nack < 0) {
				if (errno == EWOULDBLOCK) {
					WT_LOG(TAG, "TOTAL BYTES SEND: %d", total_ack_for_received);
					WT_LOGE(TAG, "timeout error %d", errno);
					return -1;
				} else {
					WT_LOG(TAG, "TOTAL BYTES SEND: %d", total_ack_for_received);
					WT_LOGE(TAG, "connection error %d", errno);
					return -1;
				}
			}
			total_ack_for_received += nbytes;
		}
		total_received += nbytes;
		remain -= nbytes;
		//WT_LOG(TAG, "Packet Number: %d Received: %d Total Received: %d Remaining: %d", cnt, nbytes, total_received, remain);
		cnt++;
	}
	WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d and total ack for %d received data", total_received, total_ack_for_received);
	return 0;
}

static int _send_data_udp(int fd, int size, struct sockaddr_in *caddr)
{
	char buf[WT_BUF_SIZE];
	int remain = size;
	unsigned int clen = sizeof( struct sockaddr_in);
	int cnt = 1;
	int total_sent = 0; 
	int total_ack_for_sent = 0;
	WT_LOG(TAG, "_send_data_udp start");
	while (remain > 0) {
		int send_size = remain > WT_BUF_SIZE ? WT_BUF_SIZE : remain;
		int nbytes = sendto(fd, buf, send_size, 0, (struct sockaddr*)caddr, clen);
		if (nbytes == 0) {
			WT_LOG(TAG, "TOTAL BYTES SEND: %d", total_sent);
			WT_LOGE(TAG, "connection closed");
			return -1;
		} else if (nbytes < 0) {
			if (errno == EWOULDBLOCK) {
				WT_LOG(TAG, "TOTAL BYTES SEND: %d", total_sent);
				WT_LOGE(TAG, "timeout error %d", errno);
				return -1;
			} else {
				WT_LOG(TAG, "TOTAL BYTES SEND: %d", total_sent);
				WT_LOGE(TAG, "connection error %d", errno);
				return -1;
			}
		}
		int nack;
		if(nbytes == WT_BUF_SIZE) {
			nack= recvfrom(fd, buf, WT_ACK_SIZE, 0, (struct sockaddr*)caddr, &clen);
			if (nack == 0) {
				WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d", total_ack_for_sent);
				WT_LOGE(TAG, "connection closed");
				return -1;
			} else if (nack < 0) {
				if (errno == EWOULDBLOCK) {
					WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d", total_ack_for_sent);
					WT_LOGE(TAG, "timeout error %d", errno);
					return -1;
				} else {
					WT_LOG(TAG, "TOTAL BYTES RECEIVED: %d", total_ack_for_sent);
					WT_LOGE(TAG, "connection error %d", errno);
					return -1;
				}
			}
			total_ack_for_sent += nbytes;
		}
		total_sent += nbytes;
		remain -= nbytes;
		//WT_LOG(TAG, "Packet Number: %d Sent: %d Total Sent: %d Remaining: %d", cnt, nbytes, total_sent, remain);
		cnt++;
	}
	WT_LOG(TAG, "TOTAL BYTES SEND: %d and total ack for %d send data", total_sent, total_ack_for_sent);
	return 0;
}

static int _udp_server(int udp_data) {
	struct sockaddr_in saddr, caddr;
	int sockfd = 0;
	int ret = 0;

	/* Socket Creation */
	WT_LOG(TAG, "create socket");
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		WT_LOGE(TAG, "create socket fail %d", errno);
		return -1;
	}

	/* Set Timeout for Sockets data transfer */
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	
	/* Socket binding process */
	WT_LOG(TAG, "bind INADDR_ANY PORT:%d", SERVER_PORT);
	memset(&caddr, 0, sizeof(caddr));
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(SERVER_PORT);
	ret = bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
	if (ret < 0) {
		WT_LOGE(TAG, "bind fail %d", errno);
		return -1;
	}
	
	unsigned int clen = sizeof(struct sockaddr_in);
	int slen = sizeof(struct sockaddr_in);
	
	memset(&caddr, 0, sizeof(caddr));
	/* Receive data through udp socket */
	ret = _recv_data_udp(sockfd, udp_data, &caddr);
	if (ret < 0) {
		WT_LOGE(TAG, "recv fail size %d ret %d code %d", udp_data, ret, errno);
		close(sockfd);
		return -1;
	}
	WT_LOG(TAG, "_recv_data_udp OK");
	
	/* Send data through udp socket */
	ret = _send_data_udp(sockfd, udp_data , &caddr);
	if (ret < 0) {
		WT_LOGE(TAG, "send fail size %d ret %d code %d", udp_data, ret, errno);
		close(sockfd);
		return -1;
	}
	WT_LOG(TAG, "_send_data_udp OK");
	
	close(sockfd);
	WT_LOG(TAG, "terminate udp data transfer");
	return 0;
}

int _udp_client(int size)
{
	struct sockaddr_in saddr;
	int ret = 0;
	int sockfd = 0;
	
	/* Socket Creation */
	WT_LOG(TAG, "create socket");
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		WT_LOGE(TAG, "create socket fail %d", errno);
		return -1;
	}
	
	/* Set Timeout for Sockets data transfer */
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

	/* Connect the socket to the server */
	WT_LOG(TAG, "connect to %s:%d", SERVER_IP, SERVER_PORT);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(SERVER_PORT);
	inet_aton(SERVER_IP, &(saddr.sin_addr));
	
	unsigned int clen = sizeof(struct sockaddr_in);
	int slen = sizeof(struct sockaddr_in);
	/* Send data through udp socket */
	ret = _send_data_udp(sockfd, size, &saddr);
	if (ret < 0) {
		WT_LOGE(TAG, "send fail size %d ret %d code %d", size, ret, errno);
		close(sockfd);
		return -1;
	}
	WT_LOG(TAG, "_udp_send_data OK");

	
	/* Receive data through udp socket */
	ret = _recv_data_udp(sockfd, size, &saddr);
	if (ret < 0) {
		WT_LOGE(TAG, "receive fail size %d ret %d code %d", size, ret, errno);
		close(sockfd);
		return -1;
	}
	WT_LOG(TAG, "_udp_recv_data OK");
	
	close(sockfd);
	WT_LOG(TAG, "udp data transmission finished");
	return 0;
}


/****************************************************************************
 * hello_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int hello_main(int argc, char *argv[])
#endif
{
	if(argc == 1) {
		printf("Hello, World!!\n");
	} else {
		int opt = atoi(argv[1]);
		int data = atoi(argv[2]);
	        if (opt == 0) {
			_udp_server(data);
		} else {
			_udp_client(data);
		}	
	}	
	return 0;
}
