
#include <time.h>
#include "rdp.h"
#include "send_packet.h"

void print_record(struct frame *r)
{
    printf("Flags: %x\n"
           "pktseq: %x\n"
           "ackseq: %x\n"
           "sender id: %d\n"
           "recvid id: %ld\n"
           "metadata id: %d\n\n",
           r->flags, r->pktseq,
           r->ackseq, r->senderid, r->recvid, r->metadata);
}
/*Serialize the frames along with the payload data (if exists). Allocating heap mem only if the payload
  is preasent.
*/
char *serialize(struct frame *r, unsigned int *size)
{
    int payloadsize = 0;
    int header_size = sizeof(char) + sizeof(char) + sizeof(char) + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(int);
    if (r->flags == 0x04)
    {
        payloadsize = r->metadata - header_size;
        if (r->payload == NULL)
        {
            payloadsize = 0;
        }
    }

    unsigned char *buffer = malloc(sizeof(struct frame) + payloadsize); // +payloadsize

    if (!buffer)
        return NULL;

    int offset = 0;
    memcpy(buffer + offset, &r->flags, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, &r->pktseq, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, &r->ackseq, sizeof(char));
    offset += sizeof(char);
    memcpy(buffer + offset, &r->unassigned, sizeof(char));
    offset += sizeof(char);

    int tmp_s_id = htonl(r->senderid);
    memcpy(buffer + offset, &tmp_s_id, sizeof(int));
    offset += sizeof(int);

    int tmp_r_id = htonl(r->recvid);
    memcpy(buffer + offset, &tmp_r_id, sizeof(int));
    offset += sizeof(int);

    int tmp_meta = htonl(r->metadata);
    memcpy(buffer + offset, &tmp_meta, sizeof(int));
    offset += sizeof(int);

    if (r->flags == 0x04 && payloadsize != 0)
        memcpy(buffer + offset, r->payload, payloadsize);

    *size = sizeof(char) + sizeof(char) + sizeof(char) + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(int) + payloadsize; // obs! added sizeof(char *) +sizeof(char *)+ payloadsize
    return buffer;
}
/* Deserialize the data from the buffer. Allocating ekstra heap memory for payload if detected*/
struct frame *de_serialize(char *d)
{

    struct frame *r = malloc(sizeof(struct frame));
    if (!r)
        return NULL;

    int offset = 0;

    r->flags = d[offset];
    offset += sizeof(char);

    r->pktseq = d[offset];
    offset += sizeof(char);

    r->ackseq = d[offset];
    offset += sizeof(char);

    r->unassigned = d[offset];
    offset += sizeof(char);

    r->senderid = ntohl(*((int *)&d[offset]));
    offset += sizeof(int);

    r->recvid = ntohl(*((int *)&d[offset]));
    offset += sizeof(int);

    r->metadata = ntohl(*((int *)&d[offset]));
    offset += sizeof(int);

    if (r->flags == 0x04 && r->metadata != 0)
    {
        int header_size = sizeof(char) + sizeof(char) + sizeof(char) + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(int);
        int payload_size = r->metadata - header_size;
        r->payload = malloc(payload_size);
        if (!r->payload)
            return NULL;
        memcpy(r->payload, d + offset, payload_size);
    }
    else
        r->payload = NULL;

    return r;
}
/*utility method used to send a disconnect request*/
void send_dic_req(int sock, struct addrinfo *to, int my_id)
{
    struct frame disconnect_req = {
        .flags = 0x02,
        .ackseq = 0,
        .pktseq = 0,
        .unassigned = 0,
        .senderid = my_id,
        .recvid = 0,
        .metadata = 0};

    unsigned int size;
    unsigned char *buffer = serialize(&disconnect_req, &size);

    if (!buffer)
    {
        perror("serilize");
        exit(EXIT_FAILURE);
    }

    ssize_t ret;

    ret = send_packet(sock, buffer, size, 0, to->ai_addr, to->ai_addrlen);
    if (ret < 0)
    {
        free(buffer);
        perror("sendto");
        free(buffer);
        exit(EXIT_FAILURE);
        //break;
    }
    free(buffer);
}

void send_refuse_conn(int sock, struct sockaddr_in rem_adr, int my_id, int reason)
{
    struct frame ack = {
        .flags = 0x20,
        .ackseq = 0,
        .pktseq = 0,
        .unassigned = 0,
        .senderid = my_id,
        .recvid = 0,
        .metadata = reason};

    unsigned int size;
    unsigned char *buffer = serialize(&ack, &size);

    if (!buffer)
    {
        perror("serilize");
        exit(EXIT_FAILURE);
    }

    ssize_t ret;

    ret = send_packet(sock, buffer, size, 0, (struct sockaddr *)&rem_adr, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        free(buffer);
        perror("sendto");
        free(buffer);
        exit(EXIT_FAILURE);
        //break;
    }
    free(buffer);
}
/*rdp_accept decides if the connection req is accepted or denied. If accepted, the client information is initilized
and returned, otherwise NULL is returned.
*/
struct client *rdp_accept(struct sockaddr_in rem_adr, char buf_with_the_data[], int sock, int *current_c_arr_size)
{

    struct frame *r = de_serialize(buf_with_the_data);

    int ret;

    if (r->flags == 1 && *current_c_arr_size != 0)
    { // got connection req
        printf("SERVER: connected to client with ID: %ld server ID 0.\n", r->senderid);
        *current_c_arr_size = *current_c_arr_size - 1; // update the capasity of the global list
        struct client *new_client = malloc(sizeof(struct client));

        if (new_client == NULL)
        {
            fprintf(stderr, "malloc failed");
            exit(EXIT_FAILURE);
        }

        int sender_id = r->senderid;

        new_client->con_id = sender_id;

        new_client->packet_i_have = 0;
        new_client->buffer = NULL;
        new_client->client_addr = malloc(sizeof(struct sockaddr_in));
        if (new_client->client_addr == NULL)
        {
            perror("malloc failed rdp_accept\n");
            exit(EXIT_FAILURE);
        }
        memcpy(new_client->client_addr, (struct sockaddr *)&rem_adr,
               sizeof(struct sockaddr_in));

        struct frame con_accept = {
            .flags = 0x10,
            .ackseq = 0,
            .pktseq = 0,
            .unassigned = 0,
            .senderid = 0,
            .recvid = r->senderid,
            .metadata = 0};

        unsigned int size;
        unsigned char *buffer = serialize(&con_accept, &size); // member to free buffer!!!
        if (!buffer)
        {

            perror("Serialize error in rdp_accept");
            exit(EXIT_FAILURE); // not sure?
        }

        ret = send_packet(sock, buffer, size, 0, (struct sockaddr *)&rem_adr, sizeof(struct sockaddr_in));

        if (ret < 0)
        {
            // free buffer?
            perror("sendto error in rdp_accept");
            exit(EXIT_FAILURE);
        }

        free(r);
        free(buffer);
        return new_client;
    }
    else
    {
        printf("NOT CONNECTED %ld server ID 0.\n", r->senderid);
        send_refuse_conn(sock, rem_adr, 0, 5);
        // need to send a code why
        free(r);
        return NULL;
    }
}
/*  This is a helper method that takes care of FIN packets or packets with no payload
    It uses the recvfrom in non-blocking mode in order to get eventual ack frame. If no packet
    is recieved the packet is marked as "need to resend" and 0 returned. If right ack packet is recieved it is marked and 0 returned as well.
    If by some chance a disconnect frame is recieved here -1 is returned to mark that.
    A short comment on non blocking recvfrom. Even though man pages say that this option is possible,
    I have not been able to test it. It probably takes a lot of packets in order to activate the part of the code 
    that doesn't return -1 from recvfrom. 
    
*/
int rdp_write_fin(struct client *c, int sock)
{
    struct sockaddr_in remote_addr;
    int ret;
    socklen_t addr_len = sizeof(remote_addr);
    struct frame pakke_fin = {
        .flags = 0x04,
        .ackseq = 0,
        .pktseq = 1,
        .unassigned = 0,
        .senderid = 0,
        .recvid = c->con_id,
        .metadata = 0

    };
    unsigned int size;
    unsigned char *buffer_fin = serialize(&pakke_fin, &size);
    if (!buffer_fin)
        exit(EXIT_FAILURE);

    ret = send_packet(sock, buffer_fin, size, 0, (struct sockaddr *)c->client_addr, sizeof(struct sockaddr_in));

    if (ret < 0)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }
    free(buffer_fin);
    unsigned char ack_recieved_buf[100]; // hard coded size!!

    if ((ret = recvfrom(sock, ack_recieved_buf, 100, // Remember hard coded size
                        MSG_DONTWAIT, (struct sockaddr *)&remote_addr, &addr_len)) == -1)
    {

        c->do_i_need_to_resend = 1;
        gettimeofday(&c->start, NULL);
        if (c->buffer != NULL)
        {

            free(c->buffer);
            c->buffer = NULL;

            return 0;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        struct frame *r_ack = de_serialize(ack_recieved_buf);
        if (r_ack->flags == 0x08 && r_ack->ackseq == 1 && r_ack->senderid == c->con_id)
        {
            printf("recieved FINAL ack: %d \n", r_ack->ackseq);

            c->packet_i_have = r_ack->ackseq ^ 1;
            c->do_i_need_to_resend = 0;
            free(r_ack);

            return 0;
        }
        else if (r_ack->flags == 0x02 && r_ack->senderid == c->con_id)
        {
            free(r_ack);

            return -1;
        }
        else
        {
            free(r_ack);
            c->do_i_need_to_resend = 1;
            gettimeofday(&c->start, NULL);
            if (c->buffer != NULL)
            {

                free(c->buffer);
                c->buffer = NULL;

                return 0;
            }
            else
            {
                return 0;
            }
        }
    }
}
/*Rdp_write returnes 1 if the right ack has been recieved and 0 otherwise.
  It uses select in order to check if an ack packet has arrived. It also checks for the final packet
  and forward it to rdp_write_fin.
*/
int rdp_write(struct client *c, char read_buf[], int write_count, int sock)
{

    if (write_count == 0)
    {
        rdp_write_fin(c, sock);
    }
    else
    {

        struct sockaddr_in remote_addr;
        int ret;
        socklen_t addr_len = sizeof(remote_addr);
        unsigned char buf_temp[write_count];
        memcpy(buf_temp, read_buf, write_count);

        int hed_size = sizeof(char) + sizeof(char) + sizeof(char) + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(int);
        struct frame pakke = {
            .flags = 0x04,
            .ackseq = 0,
            .pktseq = c->packet_i_have,
            .unassigned = 0,
            .senderid = 0,
            .recvid = c->con_id,
            .metadata = hed_size + write_count};

        pakke.payload = malloc(write_count);
        if (!pakke.payload)
            exit(EXIT_FAILURE);
        memcpy(pakke.payload, read_buf, write_count);
        unsigned int size;
        unsigned char *buffer1 = serialize(&pakke, &size); // member to free buffer!!!
        if (!buffer1)
            exit(EXIT_FAILURE);
        free(pakke.payload);

        ret = send_packet(sock, buffer1, size, 0, (struct sockaddr *)c->client_addr, sizeof(struct sockaddr_in));

        if (ret < 0)
        {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        //-----------------------------------setting up select-----------------------------------

        int fdmax = sock;
        fd_set readfds;
        struct timeval tv = {0};
        unsigned char ack_recieved_buf[100];

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100;

        int retv = select(fdmax + 1, &readfds, NULL, NULL, &tv);

        if (retv == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(sock, &readfds))
        {

            ret = recvfrom(sock, ack_recieved_buf, 100, // Remember hard coded size also 0 byte
                           0, (struct sockaddr *)&remote_addr, &addr_len);

            if (ret == -1)
            {
                perror("Failed to receive on UDP socket");
                exit(EXIT_FAILURE);
            }
            struct frame *r_ack = de_serialize(ack_recieved_buf);
            if (r_ack->ackseq == c->packet_i_have && r_ack->senderid == c->con_id)
            {

                c->packet_i_have = r_ack->ackseq ^ 1;
                c->do_i_need_to_resend = 0;
                free(r_ack);
                free(buffer1);
                return 1;
            }
            else
            {

                free(r_ack);
                free(buffer1);
                c->do_i_need_to_resend = 1;
                gettimeofday(&c->start, NULL);
                if (c->buffer != NULL)
                {
                    free(c->buffer);
                    c->buffer = malloc(write_count);

                    if (!c->buffer)
                    {
                        perror("malloc failed in rdp_write3\n");
                        exit(EXIT_FAILURE);
                    }
                    memcpy(c->buffer, buf_temp, write_count);
                    printf("FIRE 4 NO ACK print ret:  %d\n", ret);
                    return 0;
                }
                else
                {
                    c->buffer = malloc(write_count);
                    if (!c->buffer)
                    {
                        perror("malloc failed in rdp_write4\n");
                        exit(EXIT_FAILURE);
                    }
                    memcpy(c->buffer, buf_temp, write_count);
                    printf("FIRE 5 NO ACK print ret:%d\n", ret);

                    return 0;
                }
            }
        }
        else if (retv == 0)
        {
            // NO ACK
            c->do_i_need_to_resend = 1;
            gettimeofday(&c->start, NULL);
            if (c->buffer != NULL)
            {

                free(c->buffer);
                c->buffer = malloc(sizeof(char) * write_count);
                if (!c->buffer)
                {
                    perror("malloc failed in rdp_write1\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(c->buffer, buf_temp, write_count);

                free(buffer1);
                return 0;
            }
            else
            {

                c->buffer = malloc(sizeof(write_count) * write_count);
                if (!c->buffer)
                {
                    perror("malloc failed in rdp_write2\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(c->buffer, buf_temp, write_count);

                free(buffer1);
                return 0;
            }
        }
    }
}

void send_dic_req_client(int sock, struct addrinfo *to, int my_id)
{
    struct frame disconnect_req = {
        .flags = 0x02,
        .ackseq = 0,
        .pktseq = 0,
        .unassigned = 0,
        .senderid = my_id,
        .recvid = 0,
        .metadata = 0};

    unsigned int size;
    unsigned char *buffer = serialize(&disconnect_req, &size);

    if (!buffer)
    {
        perror("serilize");
        exit(EXIT_FAILURE);
    }

    ssize_t ret;

    ret = send_packet(sock, buffer, size, 0, to->ai_addr, to->ai_addrlen);
    if (ret < 0)
    {
        free(buffer);
        perror("sendto");
        free(buffer);
        exit(EXIT_FAILURE);
    }
    free(buffer);
}

void send_ack(int sock, struct addrinfo *to, int my_id, char ackseq)
{
    struct frame ack = {
        .flags = 0x08,
        .ackseq = ackseq,
        .pktseq = 0,
        .unassigned = 0,
        .senderid = my_id,
        .recvid = 0,
        .metadata = 0};

    unsigned int size;
    unsigned char *buffer = serialize(&ack, &size);

    if (!buffer)
    {
        perror("serilize");
        exit(EXIT_FAILURE);
    }

    ssize_t ret;

    ret = send_packet(sock, buffer, size, 0, to->ai_addr, to->ai_addrlen);
    if (ret < 0)
    {
        free(buffer);
        perror("sendto");
        free(buffer);
        exit(EXIT_FAILURE);
    }
    free(buffer);
}

/*rdp_connect is sending the connection request to the server and waits for 1 sec. It returns 1 on success and 0 on failure*/
int rdp_connect(int my_con_id, int sock, struct addrinfo *dest_addr, struct sockaddr_storage server, socklen_t addr_len)
{

    struct frame con_req = {
        .flags = 1,
        .ackseq = 0,
        .pktseq = 0,
        .unassigned = 0,
        .senderid = my_con_id,
        .recvid = 0,
        .metadata = 0};

    unsigned int size;
    unsigned char *buffer = serialize(&con_req, &size);
    if (!buffer)
    {
        printf("Serialized failed in rdp_connect\n");
        exit(EXIT_FAILURE);
    }

    ssize_t ret;

    ret = send_packet(sock, buffer, size, 0, dest_addr->ai_addr, dest_addr->ai_addrlen);
    if (ret < 0)
    {
        free(buffer);
        perror("sendto");
        exit(EXIT_FAILURE);
    }
    free(buffer);

    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    select(sock + 1, &readfds, NULL, NULL, &tv);

    if (FD_ISSET(sock, &readfds))
    {

        unsigned char n_buf[500];

        ret = recvfrom(sock, n_buf, sizeof(n_buf), 0, (struct sockaddr *)&server, &addr_len);
        if (ret < 0)
        {
            perror("rcvfrom failed in rdp_connnect\n.");
            exit(EXIT_FAILURE);
            //break;
        }
        struct frame *r = de_serialize(n_buf);
        //print_record(r);

        if (r->flags == 0x10)
        {
            printf("Connected to server %ld \n", r->senderid);
            free(r);
            return 1;
        }
        if (r->flags == 0x20)
        {
            if (r->metadata == 5)
            {
                printf("Server has reached max capacity!\n");
            }
            if (r->metadata == 6)
            {
                printf("Server has other problems!\n");
            }
            free(r);
            return 0;
        }
    }

    else
        printf("Timed out.\n");
    return 0;
}

void rdp_read(int sock, int my_con_id, int w_descr, struct addrinfo *p, struct sockaddr_storage server, socklen_t addr_len)
{
    char need = 0;
    char have = 1;

    unsigned char n_buf1[980]; // both payload and header
    ssize_t ret;

    struct frame ack = {
        .flags = 0x08,
        .ackseq = have,
        .pktseq = 0,
        .unassigned = 0,
        .senderid = my_con_id,
        .recvid = 0,
        .metadata = 0

    };
    while (1)
    {

        ret = recvfrom(sock, n_buf1, sizeof(n_buf1), 0, (struct sockaddr *)&server, &addr_len);
        if (ret < 0)
        {
            perror("Error recvfrom in rdp_read\n");
            break;
        }

        struct frame *r1 = de_serialize(n_buf1); // got payload here

        if (r1->flags == 0x04 && r1->pktseq == need && r1->metadata != 0)
        {
            printf("Writing data to file.....\n");
            int header_size = sizeof(char) + sizeof(char) + sizeof(char) + sizeof(char) + sizeof(int) + sizeof(int) + sizeof(int);

            //print_record(r1);
            if (write(w_descr, r1->payload, r1->metadata - header_size) == -1)
            {
                perror("Error write in rdp_read.\n");
                break;
            }

            need = r1->pktseq ^ 1;
            have = r1->pktseq;
            ack.ackseq = have;
            unsigned int size1;
            unsigned char *buffer1 = serialize(&ack, &size1);
            if (!buffer1)
            {
                printf("error buffer in rdp_read\n");
                break;
            }

            ret = send_packet(sock, buffer1, size1, 0, p->ai_addr, p->ai_addrlen);
            if (ret < 0)
            {
                free(buffer1);
                perror("sendto");
                break;
            }
            free(buffer1);
        }
        else if (r1->flags == 0x04 && r1->pktseq == have && r1->metadata != 0)
        {
            printf("Recieved duplicate!Sending ACK: %d\n", have);
            ack.ackseq = have;
            unsigned int size1;
            unsigned char *buffer1 = serialize(&ack, &size1);
            if (!buffer1)
            {
                printf("error serialize in rdp_read\n");
                break;
            }

            ret = send_packet(sock, buffer1, size1, 0, p->ai_addr, p->ai_addrlen);
            if (ret < 0)
            {
                free(buffer1);
                perror("sendto");
                break;
            }
            free(buffer1);
        }
        else if (r1->flags == 0x04 && r1->metadata == 0)
        {
            printf("Recieved final packet! Disconnecting!\n");
            send_ack(sock, p, my_con_id, r1->pktseq);
            free(r1);
            send_dic_req_client(sock, p, my_con_id);
            break;
        }
        free(r1->payload);
        free(r1);
    }
}
