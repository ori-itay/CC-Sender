#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsock2.h" 
#include <ws2tcpip.h>
#include <math.h>

#pragma comment(lib, "Ws2_32.lib")

#define RECEIVE_BUFF 4
#define S_C_BUFF 64
#define READ_BUFF 49
#define BYTE_SIZE 8

void Init_Winsock();
void compute_block(char read_buff[READ_BUFF], char s_c_buff_1[S_C_BUFF]);


int END_FLAG = 0;

struct ARGS {
	int socket_fd;
	int buff[RECEIVE_BUFF];
};

int main(int argc, char** argv) {

	Init_Winsock();

	char s_c_buff_1[S_C_BUFF], file_read_buff[READ_BUFF];
	int recv_buff[RECEIVE_BUFF];
	int chnl_port = -1, totalsent = -1, num_sent = -1, num_read = -1, notwritten = -1,
		not_been_read = -1, input_file_size = 0, s_c_fd = -1;
	FILE *fp;

	if (argc != 4) {
		fprintf(stderr, "Error: not enough arguments were provided\n");
		exit(1);
	}
	char* ip = argv[1]; char* port = argv[2]; char* file_name = argv[3];

	if ((fp = fopen(file_name, "r")) == NULL) {
		fprintf(stderr, "Error: not able to open file. \n");
		exit(1);
	}
	input_file_size = get_file_size(file_name);


	//sender - channel socket
	if ((s_c_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	chnl_port = (unsigned int)strtoul(port, NULL, 10); //get channel's port number
	struct sockaddr_in cnl_addr;
	memset(&cnl_addr, 0, sizeof(cnl_addr));
	cnl_addr.sin_family = AF_INET;
	cnl_addr.sin_port = htons(chnl_port);
	cnl_addr.sin_addr.s_addr = inet_addr(ip);

	struct ARGS thread_args = { s_c_fd , recv_buff };
	HANDLE thread = CreateThread(NULL, 0, thread_end_listen, &thread_args, 0, NULL);

	not_been_read = input_file_size;
	while (not_been_read > 0 && END_FLAG == 0) {
		memset(file_read_buff, 0, READ_BUFF);
		num_read = read(fp, file_read_buff, READ_BUFF);
		if (num_read <= 0) {
			fprintf(stderr, "Error reading fron file. exiting...\n");
			exit(1);
		}
		not_been_read -= num_read;
		compute_block(file_read_buff, s_c_buff_1);

		notwritten = num_read; //curr num of bytes to write
		totalsent = 0;
		// keep looping until nothing left to write for this BUFF size block
		while (notwritten > 0 && END_FLAG == 0) {
			// notwritten = how much left to write ; totalsent = how much written so far ; num_sent = how much written in last write() call
			num_sent = sendto(s_c_fd, s_c_buff_1 + totalsent, notwritten, 0, (SOCKADDR*)&cnl_addr, sizeof(cnl_addr));
			if (num_sent == -1) {// check if error occured (server closed connection?)
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}
			totalsent += num_sent;
			notwritten -= num_sent;
		}
	}
	printf("received: %d bytes\nwritten: %d bytes\ndetected: %d errors, corrected: %d errors\n",
		recv_buff[0], recv_buff[1], recv_buff[2], recv_buff[3]);

	if (fclose(fp) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return 0;
}




void Init_Winsock() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
		printf("Error at WSAStartup()\n");
	exit(1);
}



int get_file_size(FILE *fp) {
	int size = 0;

	fseek(fp, 0, SEEK_END); // seek to end of file
	size = ftell(fp); // get current file pointer
	rewind(fp); // seek back to beginning of file

	return size;
}

void compute_block(char read_buff[READ_BUFF], char s_c_buff_1[S_C_BUFF]) {

	int bit_ind, read_ind, write_ind, block_ind, xor = 0, bit_pos, i, j;
	char curr_bit;

	memset(s_c_buff_1, 0, S_C_BUFF);

	for (block_ind = 0; block_ind < 8; block_ind++) {
		printf("old block no # %d \n", block_ind); //print - delete after!!!!!!!!!!!!
		for (bit_ind = 0; bit_ind < READ_BUFF; bit_ind++) {

			if (bit_ind % 7 == 0 && bit_ind != 0) { printf("\n"); }//print - delete after!!!!!!!!!!!!

			if ((bit_ind % 7) == 0 && bit_ind != 0) {
				//store parity
				s_c_buff_1[write_ind] = xor | s_c_buff_1[write_ind];
				xor = 0;
			}

			read_ind = (bit_ind / 8) + (block_ind * 8);
			write_ind = (bit_ind / 7) + (block_ind * 8);
			bit_pos = 7 - bit_ind % 7;

			curr_bit = (read_buff[read_ind] & ((int)pow(2, bit_pos))) != 0; // 1 if result after mask is different from 0. otherwise - 0.
			s_c_buff_1[write_ind] = (curr_bit << bit_pos) | s_c_buff_1[write_ind];
			xor ^= curr_bit;

			printf("%d", curr_bit);		//print - delete after!!!!!!!!!!!!
		}
		printf("\n");//print - delete after!!!!!!!!!!!!

		for (i = 0; i < 7; i++) {
			s_c_buff_1[7 + (block_ind * 8)] ^= s_c_buff_1[i];
		}
		printf("\n------------\n");//print - delete after!!!!!!!!!!!!
	}

	printf("\nNew blocks: \n");//print - delete after!!!!!!!!!!!!
	for (i = 0; i < 64; i++) {//print - delete after!!!!!!!!!!!!
		if (i % 8 == 0 && i != 63) {//print - delete after!!!!!!!!!!!!
			printf("new block no # %d \n", i / 8);//print - delete after!!!!!!!!!!!!
		}

		for (j = 0; j < 8; j++) {//print - delete after!!!!!!!!!!!!
			printf("%d", !!((s_c_buff_1[i] << j) & 0x80));//print - delete after!!!!!!!!!!!!
		}//print - delete after!!!!!!!!!!!!
		printf("\n");//print - delete after!!!!!!!!!!!!
	}

	return;
}



DWORD WINAPI thread_end_listen(void *param) {

	int status, bytes_read, notread, totalread, num_sent;
	int recv_buff[] = ((struct ARGS*)param)->buff;
	int s_c_fd = ((struct ARGS*)param)->socket_fd;

	while (END_FLAG == 0) {
		notread = RECEIVE_BUFF;
		bytes_read = 0;
		while (notread > 0) {
			bytes_read = recvfrom(s_c_fd, recv_buff + bytes_read, RECEIVE_BUFF, 0, 0, 0); //deliver fd and buff as parameter or as global vars
			if (bytes_read == -1) {
				fprintf(stderr, "%s\n", strerror(errno));
				_endthread(1);
			}
			totalread += bytes_read;
			notread -= num_sent;
		}
		status = shutdown(s_c_fd, SD_BOTH);
		if (status) {
			printf("Error while closing socket. \n");
			_endthread(1);
		}
		END_FLAG = 1;
	}
	_endthread(0);
}