#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <fcntl.h> // deals with file check
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "send_packet.h"
#include "rdp.h"

//valgrind --track-fds=yes  ./NewFSP_server 2000 o 0 0
//valgrind --track-fds=yes ./NewFSP_client 127.0.0.1 2000 0

// ./NewFSP_server 2000 o 9 9
// ./NewFSP_client 127.0.0.1 2000 0

//  valgrind --track-fds=yes  ./NewFSP_server 2000 hemmelig.secret 1 0.5
//  valgrind --track-fds=yes ./NewFSP_client 127.0.0.1 2000 0.5

int main(int argc, char const *argv[])
{

    if (argc < 4)
    {

        fprintf(stderr, "Usage: %s<dest ip/hostname> <dest port> < loss>\n", argv[0]);
        return 1;
    }

    float l_prob = atof(argv[3]);

    set_loss_probability(l_prob);

    srand(time(NULL));
    unsigned int random_con_id = (unsigned int)rand();

    
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    struct addrinfo hints;
    struct addrinfo *result, *p;
    int status, sock;

    struct sockaddr_storage server;
    socklen_t addr_len;
    addr_len = sizeof server;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }
    for (p = result; p != NULL; p = p->ai_next)
    {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "Failed to create socket\n");
        return -1;
    }

    //--------------- trying to connect----------------------
    if (rdp_connect(random_con_id, sock, p, server, addr_len) == 0)
    {
        freeaddrinfo(result);
        close(sock);
        exit(EXIT_FAILURE);
    }
    //-------connected--------------

    //---------------------making a random file name-----------
    //https://stackoverflow.com/questions/308695/how-do-i-concatenate-const-literal-strings-in-c
    char file_name[30];
    strcpy(file_name, "kernel-file-");

    char str[17];
    sprintf(str, "%d", random_con_id);
    strcat(file_name, str);
    //--------------------------------------------------------
    int w_descr;

    // code and advice found at https://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c
    w_descr = open(file_name, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
    if (w_descr < 0)
    {
        /* failure */
        if (errno == EEXIST)
        {
            perror("File exists\n");
            send_dic_req_client(sock, p, random_con_id);
            close(sock);
            freeaddrinfo(result);
            exit(EXIT_FAILURE);
        }
    }
    else
    {

        rdp_read(sock, random_con_id, w_descr, p, server, addr_len);
        printf("Written to file: %s\n", file_name);

        close(w_descr);

        freeaddrinfo(result);
        close(sock);

        return EXIT_SUCCESS;
    }
}
