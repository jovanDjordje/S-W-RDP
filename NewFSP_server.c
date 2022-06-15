#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <sys/time.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <time.h>
#include "send_packet.h"
#include "rdp.h"

fd_set master_fds;

int max_c_arr_size;
int current_c_arr_size;
int number_of_files_sent;

struct timeval timeout = {

    .tv_sec = 0,
    .tv_usec = 100000};

struct client **c_array;

void free_client_mem(int id)
{
    for (size_t i = 0; i < max_c_arr_size; i++)
    {
        if (c_array[i] != NULL)
        {
            if (c_array[i]->con_id == id)
            {
                printf("\n");
                printf("Disconecting client: %d\n", c_array[i]->con_id);
                printf("   ***   ***   ***   \n");
                if (c_array[i]->buffer != NULL)
                {
                    free(c_array[i]->buffer);
                }
                close(c_array[i]->my_fd);

                free(c_array[i]->client_addr);
                free(c_array[i]);
                c_array[i] = NULL;
            }
        }
        
    }
}

int main(int argc, char const *argv[])
{

    if (argc < 5)
    {

        fprintf(stderr, "Usage: %s<port> <file name> <N> <loss>\n", argv[0]);
        return EXIT_SUCCESS;
    }
    max_c_arr_size = atoi(argv[3]);
    current_c_arr_size = atoi(argv[3]);   // is decreased when a new connection is established
    number_of_files_sent = atoi(argv[3]); // is decreased when one whole file is delivered

    float l_prob = atof(argv[4]);
    set_loss_probability(l_prob);

    int test_fd = open(argv[2], O_RDONLY);
    if (test_fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    else
    {
        close(test_fd);

        // the file is available and the program continues

        c_array = malloc(max_c_arr_size * sizeof(struct client *));
        if (c_array == NULL)
        {
            perror("malloc failed\n");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < max_c_arr_size; i++)
        {
            c_array[i] = NULL;
        }
        struct addrinfo hints;
        struct addrinfo *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;
        int status;

        if ((status = getaddrinfo(NULL, argv[1], &hints, &result)) != 0)
        {
            fprintf(stderr, "getaddrinfo\n");
            return EXIT_FAILURE;
        }
        int sock;
        struct addrinfo *p;
        for (p = result; p != NULL; p = p->ai_next)
        {
            sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (sock == -1)
            {
                perror("socket;\n");
                continue;
            }
            int yes = 1; // reuse socket fix
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

            if (bind(sock, result->ai_addr, result->ai_addrlen) == -1)
            {
                close(sock);
                perror("bind\n");
                continue;
            }
            break;
        }
        if (p == NULL)
        {
            fprintf(stderr, "Failed to create socket\n");
            return EXIT_FAILURE;
        }

        int fdmax = sock + 1;
        int i;

        unsigned char buf[980]; // MAX buf size 980, meaning, header + payload + some extra bytes just in case
        struct sockaddr_in remote_addr;
        int ret;
        socklen_t addr_len = sizeof(remote_addr);

        int num_fds;
        fd_set read_fds;
        while (number_of_files_sent > 0)
        {

            FD_ZERO(&read_fds);
            FD_SET(sock, &read_fds);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            num_fds = select(FD_SETSIZE, &read_fds, NULL, NULL, &timeout);
            printf("Select returned! Got activity on %d sockets.\n", num_fds);
            printf("\n");

            if (num_fds < 0)
            {
                perror("select");
                break;
            }

            if (num_fds == 0)
            { // using timeout function provided by select to resend the packets
                printf("time out.\n");
                for (int i = 0; i < max_c_arr_size; i++)
                {
                    if (c_array[i] != NULL)
                    {

                        if (c_array[i]->do_i_need_to_resend == 1)
                        {

                            if (rdp_write(c_array[i], c_array[i]->buffer, c_array[i]->buf_size, sock) == 1)
                            {

                                int wc;
                                unsigned char r_buf[950];
                                int fd = c_array[i]->my_fd;
                                while ((wc = read(fd, r_buf, 950)) > 0)
                                {
                                    c_array[i]->buf_size = wc;
                                    if (rdp_write(c_array[i], r_buf, wc, sock) == 0)
                                    {

                                        break;
                                    }
                                }
                                if (wc == 0)
                                {
                                    // LAST packet
                                    c_array[i]->buf_size = wc;
                                    if (rdp_write(c_array[i], r_buf, wc, sock) == -1)
                                    {
                                        free_client_mem(c_array[i]->con_id);
                                        number_of_files_sent--;
                                    }
                                }
                            }
                        }
                    }
                }
                continue;
            }
            if (FD_ISSET(sock, &read_fds))
            {

                if ((ret = recvfrom(sock, buf, sizeof(buf),
                                    0, (struct sockaddr *)&remote_addr, &addr_len)) == -1)
                {
                    perror("recvfrom failed in FD_ISSET");
                    break;
                }
                struct frame *rcv = de_serialize(buf);

                if (rcv->flags == 0x01) // connect frame
                {

                    struct client *c;

                    c = rdp_accept(remote_addr, buf, sock, &current_c_arr_size);

                    if (c != NULL)
                    {
                        c->my_fd = open(argv[2], O_RDONLY);
                        if (c->my_fd == -1)
                        {
                            perror("Error opening file\n");
                            exit(EXIT_FAILURE);
                        }
                        c_array[current_c_arr_size] = c;
                        int wc;
                        unsigned char r_buf[950]; // MAX payload buf size 950
                        int fd = c_array[current_c_arr_size]->my_fd;
                        while ((wc = read(fd, r_buf, 950)) > 0)
                        {
                            c_array[current_c_arr_size]->buf_size = wc;
                            if (rdp_write(c_array[current_c_arr_size], r_buf, wc, sock) == 0)
                            {

                                break;
                            }
                        }
                        if (wc == 0)
                        {

                            c_array[i]->buf_size = wc;
                            if (rdp_write(c_array[i], r_buf, wc, sock) == -1)
                            {
                                free_client_mem(c_array[i]->con_id);
                                number_of_files_sent--;
                            }
                        }
                    }
                    free(rcv);
                }
                else if (rcv->flags == 0x08)
                { // ACK
                    for (size_t i = 0; i < max_c_arr_size; i++)
                    {
                        if (c_array[i] != NULL)
                        {
                            if (c_array[i]->con_id == rcv->senderid)
                            {

                                if (c_array[i]->packet_i_have == rcv->ackseq)
                                {
                                    c_array[i]->packet_i_have = rcv->ackseq ^ 1;
                                    c_array[i]->do_i_need_to_resend = 0;
                                    int wc;
                                    unsigned char r_buf[950];
                                    int fd = c_array[i]->my_fd;
                                    while ((wc = read(fd, r_buf, 950)) > 0)
                                    {
                                        c_array[i]->buf_size = wc;
                                        if (rdp_write(c_array[i], r_buf, wc, sock) == 0)
                                        {
                                            break;

                                        }
                                    }
                                    if (wc == 0)
                                    {

                                        c_array[i]->buf_size = wc;
                                        if (rdp_write(c_array[i], r_buf, wc, sock) == -1)
                                        {
                                            free_client_mem(c_array[i]->con_id);
                                            number_of_files_sent--;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    free(rcv);
                }
                else if (rcv->flags == 0x02)
                {
                    for (size_t i = 0; i < max_c_arr_size; i++)
                    {
                        if (c_array[i] != NULL)
                        {
                            if (c_array[i]->con_id == rcv->senderid)
                            {
                                printf("\n");
                                printf("Disconecting client: %d\n", c_array[i]->con_id);
                                printf("   ***   ***   ***   \n");
                                if (c_array[i]->buffer != NULL)
                                {
                                    free(c_array[i]->buffer);
                                }
                                close(c_array[i]->my_fd);

                                free(c_array[i]->client_addr);
                                free(c_array[i]);
                                c_array[i] = NULL;
                                number_of_files_sent--;
                            }
                        }
                    }
                    free(rcv);
                }
            }
        }

        free(c_array);

        freeaddrinfo(result);
        close(sock);

        return EXIT_SUCCESS;
    }
}