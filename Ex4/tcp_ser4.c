/**********************************
tcp_ser.c: the source file of the server in tcp transmission
***********************************/

#include "headsock.h"
#include <stdio.h>
#include <string.h>
#include <time.h>     // Include time.h for seeding the random number generator
#include <unistd.h>   // Include unistd.h for getpid() and getppid()
#include <sys/time.h> // Include sys/time.h for gettimeofday()

#define BACKLOG 10

void str_ser(int sockfd, int data_unit_size, double error_prob); // transmitting and receiving function

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <data unit size> <error probability> <iterations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int data_unit_size = atoi(argv[1]);
    double error_prob = atof(argv[2]);
    int iterations = atoi(argv[3]);

    if (data_unit_size <= 0 || error_prob < 0 || error_prob > 1 || iterations <= 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        exit(EXIT_FAILURE);
    }

    int iteration_count = 0;

    while (iteration_count < iterations)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        srand(tv.tv_usec ^ getpid() ^ getppid() ^ iteration_count); // Seed the random number generator with time, PID, PPID, and iteration

        int sockfd, con_fd, ret;
        struct sockaddr_in my_addr;
        struct sockaddr_in their_addr;
        int sin_size;
        pid_t pid;

        sockfd = socket(AF_INET, SOCK_STREAM, 0); // create socket
        if (sockfd < 0)
        {
            printf("error in socket!");
            exit(1);
        }

        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(MYTCP_PORT);
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // inet_addr("172.0.0.1");
        bzero(&(my_addr.sin_zero), 8);
        ret = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)); // bind socket
        if (ret < 0)
        {
            printf("error in binding");
            exit(1);
        }

        ret = listen(sockfd, BACKLOG); // listen
        if (ret < 0)
        {
            printf("error in listening");
            exit(1);
        }

        printf("waiting for data\n");
        sin_size = sizeof(struct sockaddr_in);
        con_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size); // accept the packet
        if (con_fd < 0)
        {
            printf("error in accept\n");
            exit(1);
        }

        str_ser(con_fd, data_unit_size, error_prob); // receive packet and response
        close(con_fd);
        close(sockfd);

        iteration_count++;
    }

    printf("Completed all iterations.\n");
    exit(0);
}

void str_ser(int sockfd, int data_unit_size, double error_prob)
{
    char buf[BUFSIZE];
    FILE *fp;
    struct pack_so *packet;
    struct ack_so ack;
    int n = 0;
    long lseek = 0;
    uint32_t expected_seq = 0;

    printf("Receiving data...\n");

    while (1)
    {
        // Allocate memory for packet with variable data length
        packet = (struct pack_so *)malloc(sizeof(struct pack_so) + data_unit_size);

        // Receive packet
        n = recv(sockfd, packet, sizeof(struct pack_so) + data_unit_size, 0);
        if (n == -1)
        {
            printf("Error receiving packet\n");
            exit(1);
        }

        // Simulate random errors
        if (simulate_error(error_prob))
        {
            // Send NAK
            ack.seq_no = packet->seq_no;
            ack.status = NAK;
            send(sockfd, &ack, sizeof(struct ack_so), 0);
            printf("Packet %d corrupted, requesting retransmission\n", packet->seq_no);
            free(packet);
            continue;
        }

        // Verify checksum
        uint32_t calculated_checksum = calculate_checksum(packet->data, packet->len);
        if (calculated_checksum != packet->checksum)
        {
            // Send NAK
            ack.seq_no = packet->seq_no;
            ack.status = NAK;
            send(sockfd, &ack, sizeof(struct ack_so), 0);
            printf("Checksum error in packet %d\n", packet->seq_no);
            free(packet);
            continue;
        }

        // Check sequence number
        if (packet->seq_no == expected_seq)
        {
            // Copy data to buffer
            memcpy(buf + lseek, packet->data, packet->len);
            lseek += packet->len;
            expected_seq++;

            // Send ACK
            ack.seq_no = packet->seq_no;
            ack.status = ACK;
            send(sockfd, &ack, sizeof(struct ack_so), 0);

            // Check if this is the last packet
            if (packet->data[packet->len - 1] == '\0')
            {
                free(packet);
                break;
            }
        }
        free(packet);
    }

    // Write received data to file
    if ((fp = fopen("myTCPreceive.txt", "wt")) == NULL)
    {
        printf("Cannot create file\n");
        exit(0);
    }
    fwrite(buf, 1, lseek, fp);
    fclose(fp);
    printf("File received successfully! Total bytes: %d\n", (int)lseek);
}
