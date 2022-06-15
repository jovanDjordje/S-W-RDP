#ifndef rdp
#define rdp

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "send_packet.h"

struct client {
    struct sockaddr_in * client_addr;
    char do_i_need_to_resend;
    int con_id;
    int my_fd;
    int packet_i_have;
    unsigned char *buffer;
    int buf_size;
    struct timeval start;

};

struct frame {
    unsigned char flags;
    unsigned char pktseq;
    unsigned char ackseq;
    unsigned char unassigned;
    int senderid;
    int recvid;
    int metadata;
    unsigned char * payload;
    
};

void print_record(struct frame *r);

char *serialize(struct frame *r,unsigned int *size);
struct frame* de_serialize(char *d);
void send_dic_req (int sock, struct addrinfo *to, int my_id);// server
void send_refuse_conn(int sock, struct sockaddr_in rem_adr, int my_id, int reason); // s

struct client * rdp_accept(struct sockaddr_in rem_adr, char buf_with_the_data[], int sock, int * current_c_arr_size);
int rdp_write_fin(struct client *c, int sock);

int rdp_write(struct client *c, char read_buf[], int write_count, int sock);

void send_dic_req_client (int sock, struct addrinfo *to, int my_id); // client
void send_ack(int sock, struct addrinfo *to, int my_id, char ackseq); // client

int rdp_connect(int my_con_id, int sock, struct addrinfo * dest_addr, struct sockaddr_storage server,socklen_t addr_len);

void rdp_read(int sock, int my_con_id, int w_descr  ,struct addrinfo * p, struct sockaddr_storage server,socklen_t addr_len);







#endif // rdp