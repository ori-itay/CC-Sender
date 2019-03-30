#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsock2.h"
#include <ws2tcpip.h>
#include <math.h>
#include <windows.h>
#include <process.h>
#include <errno.h>

#pragma comment(lib, "Ws2_32.lib")

#define UDP_BUFF 64
#define READ_BUFF 49
#define RECV_BUFF 16

int recvfromTimeOutUDP(SOCKET socket);
void Init_Winsock();
int read_from_file(FILE *fp, char file_read_buff[]);
void compute_frame(char read_buff[READ_BUFF], char s_c_buff_1[UDP_BUFF]);
void send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write);
int receive_frame(char buff[], int fd, int bytes_to_read);
DWORD WINAPI thread_end_listen(void *param);

volatile int END_FLAG = 0;
int SelectTiming;

int main(int argc, char** argv) {

	Init_Winsock();

	char udp_buff[UDP_BUFF], file_read_buff[READ_BUFF];
	int recv_buff[RECV_BUFF];
	int local_port = -1, s_c_fd = -1, received_bytes = 0;
	FILE *fp;
	struct sockaddr_in sender_addr, chnl_addr;

	if (argc != 4) {
		fprintf(stderr, "Error: wrong number of arguments! Exiting...\n");
		exit(1);
	}
	char* chnl_ip = argv[1]; char* port = argv[2]; char* file_name = argv[3];

	if ((fp = fopen(file_name, "rb")) == NULL) {
		fprintf(stderr, "Error: not able to open file. \n");
		exit(1);
	}

	//sender - channel socket
	if ((s_c_fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		fprintf(stderr, "Error: problem while opening socket. Exiting... \n");
		exit(1);
	}
	//sender address
	memset(&sender_addr, 0, sizeof(sender_addr));
	sender_addr.sin_family = AF_INET; /* receiving */
	sender_addr.sin_port = htons(0);
	sender_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//channel address
	int chnl_port = (unsigned int)strtoul(port, NULL, 10); 
	memset(&chnl_addr, 0, sizeof(chnl_addr));
	chnl_addr.sin_family = AF_INET; /* sending to */
	chnl_addr.sin_port = htons(chnl_port);
	chnl_addr.sin_addr.s_addr = inet_addr(chnl_ip);

	if (bind(s_c_fd, (SOCKADDR*) &sender_addr, sizeof(sender_addr)) != 0) {
		fprintf(stderr, "Bind failed. Exiting...\n");
		exit(1);
	}

	while (read_from_file(fp, file_read_buff) && received_bytes == 0 && END_FLAG == 0) {
		compute_frame(file_read_buff, udp_buff);
		send_frame(udp_buff, s_c_fd, chnl_addr, UDP_BUFF);
		if ((received_bytes = receive_frame((char*)recv_buff, s_c_fd, UDP_BUFF) )) { //receive stats from channel
			break;
		}
	}
	if (received_bytes == 0) { //still haven't got "end" from channel-receiver
		SelectTiming = recvfromTimeOutUDP(s_c_fd);
		receive_frame((char*)recv_buff, s_c_fd, RECV_BUFF*sizeof(int)); //receive stats from channel
	}
	fprintf(stderr,"received: %d bytes\nwritten: %d bytes\ndetected: %d errors, corrected: %d errors",
		recv_buff[0], recv_buff[1], recv_buff[2], recv_buff[3]);

	if (fclose(fp) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}
	if (closesocket(s_c_fd) != 0) {
		fprintf(stderr, "Error while closing socket. \n");
	}
	WSACleanup();
	return 0;
}



void Init_Winsock() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		fprintf(stderr,"Error at WSAStartup(). Exiting...\n");
		exit(1);
	}
}


void compute_frame(char read_buff[READ_BUFF], char udp_buff[UDP_BUFF]) {

	int bit_ind, read_ind, write_ind, block_ind, xor = 0, r_bit_pos = 7, w_bit_pos = 7, i;
	char curr_bit;

	memset(udp_buff, 0, UDP_BUFF);
	for (block_ind = 0; block_ind < 8; block_ind++) {
		for (bit_ind = 0; bit_ind < READ_BUFF; bit_ind++) {

			read_ind = (bit_ind + (block_ind * 49)) / 8;
			write_ind = bit_ind / 7 + block_ind * 8;

			curr_bit = (read_buff[read_ind] & (int)pow(2, r_bit_pos)) != 0; // 1 if result after mask is different from 0. otherwise - 0.
			udp_buff[write_ind] = (curr_bit << w_bit_pos) | udp_buff[write_ind];
			xor ^= curr_bit;

			if (((bit_ind + 1) % 7) == 0 && bit_ind != 0) {
				//store parity
				udp_buff[write_ind] = xor | udp_buff[write_ind];
				xor = 0;
			}
			r_bit_pos--;
			if (r_bit_pos == -1) { r_bit_pos = 7; }
			w_bit_pos--;
			if (w_bit_pos == 0) { w_bit_pos = 7; }
		}

		for (i = 0; i < 7; i++) {
			udp_buff[block_ind * 8 + 7] ^= udp_buff[i + (block_ind * 8)];
		}
	}
	return;
}


DWORD WINAPI thread_end_listen(void *param) {

	int status;
	int s_c_fd = *(int*)(param);

	while (END_FLAG == 0) {
		END_FLAG = 1;
		status = shutdown(s_c_fd, SD_SEND);
		if (status) {
			fprintf(stderr, "Error while closing socket. \n");
			return 1;
		}
	}
	return 0; // should not get here
}


int read_from_file(FILE *fp, char file_read_buff[]) {
	int num_read = 0;

	memset(file_read_buff, 0, READ_BUFF);
	num_read = fread(file_read_buff, sizeof(char), READ_BUFF, fp);
	if (num_read < 0) {
		fprintf(stderr, "Error reading fron file. exiting...\n");
		exit(1);
	}
	return num_read;
}


void send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write) {
	int totalsent = 0, num_sent = 0;

	while (bytes_to_write > 0 && END_FLAG == 0) {
		num_sent = sendto(fd, buff + totalsent, bytes_to_write, 0, (SOCKADDR*)&to_addr, sizeof(to_addr));
		if (num_sent == -1) {
			fprintf(stderr, "Error while sending frame. \n");
			return;
		}
		totalsent += num_sent;
		bytes_to_write -= num_sent;
	}
}


int receive_frame(char buff[], int fd, int bytes_to_read) {
	int totalread = 0, bytes_been_read = 0;
	unsigned long data_len;

	totalread = 0;
	while (totalread < bytes_to_read) {
		ioctlsocket(fd, FIONREAD, &data_len);
		if (data_len <= 0) { break; }
		bytes_been_read = recvfrom(fd, buff + totalread, bytes_to_read, 0, 0, 0);
		if (bytes_been_read == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalread += bytes_been_read;
		END_FLAG = 1;
	}
	return totalread;
}


int recvfromTimeOutUDP(SOCKET socket)
{
	struct fd_set fds;
	// Setup fd_set structure
	FD_ZERO(&fds);
	FD_SET(socket, &fds);
	return select(socket + 1, &fds, NULL, NULL, NULL);
}