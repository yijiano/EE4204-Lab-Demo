#include "headsock.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// Structure to store statistics for each iteration
struct iteration_stats
{
    float time_ms;
    long bytes_sent;
    float data_rate;
};

float str_cli(FILE *fp, int sockfd, long *len, int data_unit_size);
void tv_sub(struct timeval *out, struct timeval *in);
void print_summary_statistics(struct iteration_stats *stats, int iterations);

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <IP address> <data unit size> <error probability> <iterations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int data_unit_size = atoi(argv[2]);
    double error_prob = atof(argv[3]);
    int iterations = atoi(argv[4]);

    if (data_unit_size <= 0 || error_prob < 0 || error_prob > 1 || iterations <= 0)
    {
        fprintf(stderr, "Invalid arguments\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for storing statistics
    struct iteration_stats *stats = malloc(iterations * sizeof(struct iteration_stats));
    if (stats == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for statistics\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < iterations; i++)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        srand(tv.tv_usec ^ getpid() ^ (i + 1));

        int sockfd, ret;
        float ti, rt;
        long len;
        struct sockaddr_in ser_addr;
        char **pptr;
        struct hostent *sh;
        struct in_addr **addrs;
        FILE *fp;

        sh = gethostbyname(argv[1]);
        if (sh == NULL)
        {
            printf("error when gethostbyname");
            free(stats);
            exit(0);
        }

        printf("\nIteration %d:\n", i + 1);
        printf("canonical name: %s\n", sh->h_name);
        for (pptr = sh->h_aliases; *pptr != NULL; pptr++)
            printf("the aliases name is: %s\n", *pptr);
        switch (sh->h_addrtype)
        {
        case AF_INET:
            printf("AF_INET\n");
            break;
        default:
            printf("unknown addrtype\n");
            break;
        }

        addrs = (struct in_addr **)sh->h_addr_list;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            printf("error in socket");
            free(stats);
            exit(1);
        }
        ser_addr.sin_family = AF_INET;
        ser_addr.sin_port = htons(MYTCP_PORT);
        memcpy(&(ser_addr.sin_addr.s_addr), *addrs, sizeof(struct in_addr));
        bzero(&(ser_addr.sin_zero), 8);
        ret = connect(sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr));
        if (ret != 0)
        {
            printf("connection failed\n");
            close(sockfd);
            free(stats);
            exit(1);
        }

        if ((fp = fopen("myfile.txt", "r+t")) == NULL)
        {
            printf("File doesn't exist\n");
            close(sockfd);
            free(stats);
            exit(0);
        }

        ti = str_cli(fp, sockfd, &len, data_unit_size);
        rt = (len / (float)ti);

        // Store statistics for this iteration
        stats[i].time_ms = ti;
        stats[i].bytes_sent = len;
        stats[i].data_rate = rt;

        printf("Time(ms): %.3f, Data sent(byte): %d\nData rate: %.2f (Kbytes/s)\n",
               ti, (int)len, rt);

        close(sockfd);
        fclose(fp);
    }

    // Print summary statistics
    print_summary_statistics(stats, iterations);

    // Clean up
    free(stats);
    exit(0);
}

void print_summary_statistics(struct iteration_stats *stats, int iterations)
{
    float total_time = 0;
    long total_bytes = 0;
    float total_rate = 0;
    float min_rate = stats[0].data_rate;
    float max_rate = stats[0].data_rate;
    float min_time = stats[0].time_ms;
    float max_time = stats[0].time_ms;

    for (int i = 0; i < iterations; i++)
    {
        total_time += stats[i].time_ms;
        total_bytes += stats[i].bytes_sent;
        total_rate += stats[i].data_rate;

        if (stats[i].data_rate < min_rate)
            min_rate = stats[i].data_rate;
        if (stats[i].data_rate > max_rate)
            max_rate = stats[i].data_rate;
        if (stats[i].time_ms < min_time)
            min_time = stats[i].time_ms;
        if (stats[i].time_ms > max_time)
            max_time = stats[i].time_ms;
    }

    printf("\n=== Summary Statistics (%d iterations) ===\n", iterations);
    printf("Average time: %.3f ms (min: %.3f ms, max: %.3f ms)\n",
           total_time / iterations, min_time, max_time);
    printf("Average data sent: %ld bytes\n", total_bytes / iterations);
    printf("Average data rate: %.2f Kbytes/s (min: %.2f Kbytes/s, max: %.2f Kbytes/s)\n",
           total_rate / iterations, min_rate, max_rate);
    printf("=====================================\n");
}

float str_cli(FILE *fp, int sockfd, long *len, int data_unit_size)
{
    char *buf;
    long lsize, ci = 0;
    struct pack_so *packet;
    struct ack_so ack;
    int n, slen;
    float time_inv = 0.0;
    struct timeval sendt, recvt;
    uint32_t seq_no = 0;

    fseek(fp, 0, SEEK_END);
    lsize = ftell(fp);
    rewind(fp);
    printf("File length: %d bytes\n", (int)lsize);
    printf("Packet length: %d bytes\n", data_unit_size);

    buf = (char *)malloc(lsize);
    if (buf == NULL)
        exit(2);
    fread(buf, 1, lsize, fp);
    buf[lsize] = '\0';

    gettimeofday(&sendt, NULL);

    while (ci <= lsize)
    {
        packet = (struct pack_so *)malloc(sizeof(struct pack_so) + data_unit_size);

        packet->seq_no = seq_no;
        if ((lsize + 1 - ci) <= data_unit_size)
            slen = lsize + 1 - ci;
        else
            slen = data_unit_size;

        packet->len = slen;
        memcpy(packet->data, buf + ci, slen);
        packet->checksum = calculate_checksum(packet->data, slen);

        while (1)
        {
            n = send(sockfd, packet, sizeof(struct pack_so) + slen, 0);
            if (n == -1)
            {
                printf("Send error!\n");
                free(packet);
                exit(1);
            }

            n = recv(sockfd, &ack, sizeof(struct ack_so), 0);
            if (n == -1)
            {
                printf("Error receiving ACK\n");
                free(packet);
                exit(1);
            }

            if (ack.seq_no == seq_no && ack.status == ACK)
                break;

            printf("Received NAK for packet %d, retransmitting\n", seq_no);
        }

        ci += slen;
        seq_no++;
        free(packet);
    }

    gettimeofday(&recvt, NULL);
    *len = ci;
    tv_sub(&recvt, &sendt);
    time_inv += (recvt.tv_sec) * 1000.0 + (recvt.tv_usec) / 1000.0;
    free(buf);
    return (time_inv);
}

void tv_sub(struct timeval *out, struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
        --out->tv_sec;
        out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}