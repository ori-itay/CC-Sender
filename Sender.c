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

#pragma comment(lib, "Ws2_32.lib")

#define UDP_BUFF 64
#define READ_BUFF 49
#define RECV_BUFF 4


int recvfromTimeOutUDP(SOCKET socket, long sec, long usec);
void Init_Winsock();
int get_file_size(FILE *fp);
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
	int local_port = -1, not_been_read = -1, input_file_size = 0, s_c_fd = -1, received_bytes = 0;
	FILE *fp;
	struct sockaddr_in sender_addr, chnl_addr;

	if (argc != 4) {
		fprintf(stderr, "Error: not enough arguments were provided\n");
		exit(1);
	}
	char* chnl_ip = argv[1]; char* port = argv[2]; char* file_name = argv[3];

	if ((fp = fopen(file_name, "rb")) == NULL) {
		fprintf(stderr, "Error: not able to open file. \n");
		exit(1);
	}
	input_file_size = get_file_size(fp);

	//sender - channel socket
	if ((s_c_fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		fprintf(stderr, "%s\n", strerror(errno));
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
		fprintf(stderr, "Bind failed. exiting...\n");
		exit(1);
	}

	not_been_read = input_file_size;
	while (not_been_read > 0 && received_bytes == 0) {
		printf("before read from file \n");
		read_from_file(fp, file_read_buff);
		not_been_read -= READ_BUFF;
		compute_frame(file_read_buff, udp_buff);
		send_frame(udp_buff, s_c_fd, chnl_addr, UDP_BUFF);
		printf("before receive frame\n");
		if ((received_bytes = receive_frame((char*)recv_buff, s_c_fd, UDP_BUFF) )) { //receive stats from channel
			break;
		}
		printf("after receive frame\n");
	}
	if (received_bytes == 0) {
		SelectTiming = recvfromTimeOutUDP(s_c_fd, 100000, 0);
		receive_frame((char*)recv_buff, s_c_fd, RECV_BUFF*sizeof(int)); //receive stats from channel
	}

	printf("received: %d bytes\nwritten: %d bytes\ndetected: %d errors, corrected: %d errors\n",
		recv_buff[0], recv_buff[1], recv_buff[2], recv_buff[3]);

	if (fclose(fp) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}
	if (shutdown(s_c_fd, SD_SEND) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}
	WSACleanup();
	return 0;
}




void Init_Winsock() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		exit(1);
	}
}


int get_file_size(FILE *fp) {
	int size = 0;

	fseek(fp, 0, SEEK_END); // seek to end of file
	size = ftell(fp); // get current file pointer
	rewind(fp); // seek back to beginning of file

	return size;
}


void compute_frame(char read_buff[READ_BUFF], char udp_buff[UDP_BUFF]) {

	int bit_ind, read_ind, write_ind, block_ind, xor = 0, bit_pos, i, j;
	char curr_bit;

	memset(udp_buff, 0, UDP_BUFF);
	for (block_ind = 0; block_ind < 8; block_ind++) {
		printf("old block no # %d \n", block_ind); //print - delete after!!!!!!!!!!!!
		for (bit_ind = 0; bit_ind < READ_BUFF; bit_ind++) {

			if (bit_ind % 7 == 0 && bit_ind != 0) { printf("\n"); }//print - delete after!!!!!!!!!!!!

			if ((bit_ind % 7) == 0 && bit_ind != 0) {
				//store parity
				udp_buff[write_ind] = xor | udp_buff[write_ind];
				xor = 0;
			}

			read_ind = (bit_ind / 8) + (block_ind * 8);
			write_ind = (bit_ind / 7) + (block_ind * 8);
			bit_pos = 7 - bit_ind % 7;

			curr_bit = (read_buff[read_ind] & ((int)pow(2, bit_pos))) != 0; // 1 if result after mask is different from 0. otherwise - 0.
			udp_buff[write_ind] = (curr_bit << bit_pos) | udp_buff[write_ind];
			xor ^= curr_bit;

			printf("%d", curr_bit);		//print - delete after!!!!!!!!!!!!
		}
		printf("\n");//print - delete after!!!!!!!!!!!!

		for (i = 0; i < 7; i++) {
			udp_buff[7 + (block_ind * 8)] ^= udp_buff[i];
		}
		printf("\n------------\n");//print - delete after!!!!!!!!!!!!
	}

	printf("\nNew blocks: \n");//print - delete after!!!!!!!!!!!!
	for (i = 0; i < 64; i++) {//print - delete after!!!!!!!!!!!!
		if (i % 8 == 0 && i != 63) {//print - delete after!!!!!!!!!!!!
			printf("new block no # %d \n", i / 8);//print - delete after!!!!!!!!!!!!
		}

		for (j = 0; j < 8; j++) {//print - delete after!!!!!!!!!!!!
			printf("%d", !!((udp_buff[i] << j) & 0x80));//print - delete after!!!!!!!!!!!!
		}//print - delete after!!!!!!!!!!!!
		printf("\n");//print - delete after!!!!!!!!!!!!
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
			printf("Error while closing socket. \n");
			return 1;
		}
	}
	return 0;
}


int read_from_file(FILE *fp, char file_read_buff[]) {
	int num_read = -1;

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
		printf("num sent: %d\n", num_sent);
		if (num_sent == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalsent += num_sent;
		bytes_to_write -= num_sent;
	}
	//sprintf("wrote %d bytes \n", totalsent);
}


int receive_frame(char buff[], int fd, int bytes_to_read) {
	int totalread = 0, bytes_been_read = 0, data_len;

	totalread = 0;
	while (totalread < bytes_to_read) {
		printf("in receive frame before recvfrom\n");
		//addrsize = sizeof(from_addr);
		//bytes_been_read = recvfrom(fd, buff + totalread, bytes_to_read, 0,  &from_addr, &addrsize);
		ioctlsocket(fd, FIONREAD, &data_len);
		if (data_len == 0) { break; }
		bytes_been_read = recvfrom(fd, buff + totalread, bytes_to_read, 0, 0, 0);
		printf("in receive frame after recvfrom. bytes_been_read: %d\n", bytes_been_read);
		if (bytes_been_read == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalread += bytes_been_read;
	}
	return totalread;
}


int recvfromTimeOutUDP(SOCKET socket, long sec, long usec)
{

	// Setup timeval variable

	struct timeval timeout;

	struct fd_set fds;



	timeout.tv_sec = sec;

	timeout.tv_usec = usec;

	// Setup fd_set structure

	FD_ZERO(&fds);

	FD_SET(socket, &fds);

	// Return value:

	// -1: error occurred

	// 0: timed out

	// > 0: data ready to be read

	return select(0, &fds, 0, 0, &timeout);

}