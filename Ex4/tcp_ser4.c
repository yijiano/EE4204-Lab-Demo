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
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bzero(&(my_addr.sin_zero), 8);
    ret = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
    if (ret < 0)
    {
        printf("error in binding");
        exit(1);
    }

    ret = listen(sockfd, BACKLOG);
    if (ret < 0)
    {
        printf("error in listening");
        exit(1);
    }

    while (1)
    {
        printf("waiting for data\n");
        sin_size = sizeof(struct sockaddr_in);
        con_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (con_fd < 0)
        {
            printf("error in accept\n");
            exit(1);
        }

        if ((pid = fork()) == 0)
        {
            close(sockfd);
            // Generate a unique seed for each connection
            struct timeval tv;
            gettimeofday(&tv, NULL);
            unsigned int seed = tv.tv_sec * 1000000 + tv.tv_usec;
            seed ^= getpid(); // XOR with process ID
            seed ^= rand();   // XOR with another random number
            srand(seed);      // Set the new seed

            str_ser(con_fd, data_unit_size, error_prob);
            close(con_fd);
            exit(0);
        }
        else
            close(con_fd);
    }
    close(sockfd);
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
        packet = (struct pack_so *)malloc(sizeof(struct pack_so) + data_unit_size);

        n = recv(sockfd, packet, sizeof(struct pack_so) + data_unit_size, 0);
        if (n == -1)
        {
            printf("Error receiving packet\n");
            exit(1);
        }

        // Simulate random errors with newly seeded random number generator
        if (simulate_error(error_prob) ||
            calculate_checksum(packet->data, packet->len) != packet->checksum)
        {
            // On error, don't send anything - let client timeout
            printf("Packet %d corrupted or error simulated\n", packet->seq_no);
            free(packet);
            continue;
        }

        if (packet->seq_no == expected_seq)
        {
            memcpy(buf + lseek, packet->data, packet->len);
            lseek += packet->len;
            expected_seq++;

            // Send ACK
            ack.seq_no = packet->seq_no;
            send(sockfd, &ack, sizeof(struct ack_so), 0);

            if (packet->data[packet->len - 1] == '\0')
            {
                free(packet);
                break;
            }
        }
        free(packet);
    }

    if ((fp = fopen("myTCPreceive.txt", "wt")) == NULL)
    {
        printf("Cannot create file\n");
        exit(0);
    }
    fwrite(buf, 1, lseek, fp);
    fclose(fp);
    printf("File received successfully! Total bytes: %d\n", (int)lseek);
}