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

#define NEWFILE (O_WRONLY|O_CREAT|O_TRUNC)
#define MYTCP_PORT 4950
#define MYUDP_PORT 5350
#define BUFSIZE 60000

#define NAK 0
#define ACK 1

#define ERROR_PROB 0.1 
#define DATALEN 500
#define PACKLEN 508
#define HEADLEN 8

struct pack_so {
    uint32_t seq_no;      // Sequence number
    uint32_t len;         // Length of data
    char data[DATALEN];   // Data
    uint32_t checksum;    // For error detection
};

struct ack_so {
    uint32_t seq_no;     // Sequence number being acknowledged 
    uint8_t status;      // ACK or NAK
};
