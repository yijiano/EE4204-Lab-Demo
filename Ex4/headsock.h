// headfile for TCP program
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>

#define NEWFILE (O_WRONLY | O_CREAT | O_TRUNC)
#define MYTCP_PORT 4950
#define MYUDP_PORT 5350
#define BUFSIZE 60000

#define NAK 0
#define ACK 1

#define PACKLEN 508
#define HEADLEN 8

struct pack_so
{
    uint32_t seq_no;   // Sequence number
    uint32_t len;      // Length of data
    uint32_t checksum; // For error detection
    char data[];       // Data (variable length)
};

struct ack_so
{
    uint32_t seq_no; // Sequence number being acknowledged
    uint8_t status;  // ACK or NAK
};

uint32_t calculate_checksum(char *data, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len; i++)
    {
        sum += (uint8_t)data[i];
    }
    return sum;
}

int simulate_error(double error_prob)
{
    // Generate random number between 0 and 999
    int random = rand() % 1000;
    // Return 1 (error) if number is less than error probability * 1000
    return random < (error_prob * 1000);
}
