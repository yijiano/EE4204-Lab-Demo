/*******************************
tcp_client.c: the source file of the client in tcp transmission 
********************************/

#include "headsock.h"

float str_cli(FILE *fp, int sockfd, long *len);                       //transmission function

uint32_t calculate_checksum(char *data, int len);

void tv_sub(struct  timeval *out, struct timeval *in);	    //calcu the time interval between out and in

int main(int argc, char **argv)
{
	int sockfd, ret;
	float ti, rt;
	long len;
	struct sockaddr_in ser_addr;
	char ** pptr;
	struct hostent *sh;
	struct in_addr **addrs;
	FILE *fp;

	if (argc != 2) {
		printf("parameters not match");
	}

	sh = gethostbyname(argv[1]);	                                       //get host's information
	if (sh == NULL) {
		printf("error when gethostby name");
		exit(0);
	}

	printf("canonical name: %s\n", sh->h_name);					//print the remote host's information
	for (pptr=sh->h_aliases; *pptr != NULL; pptr++)
		printf("the aliases name is: %s\n", *pptr);
	switch(sh->h_addrtype)
	{
		case AF_INET:
			printf("AF_INET\n");
		break;
		default:
			printf("unknown addrtype\n");
		break;
	}
        
	addrs = (struct in_addr **)sh->h_addr_list;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);                           //create the socket
	if (sockfd <0)
	{
		printf("error in socket");
		exit(1);
	}
	ser_addr.sin_family = AF_INET;                                                      
	ser_addr.sin_port = htons(MYTCP_PORT);
	memcpy(&(ser_addr.sin_addr.s_addr), *addrs, sizeof(struct in_addr));
	bzero(&(ser_addr.sin_zero), 8);
	ret = connect(sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr));         //connect the socket with the host
	if (ret != 0) {
		printf ("connection failed\n"); 
		close(sockfd); 
		exit(1);
	}
	
	if((fp = fopen ("myfile.txt","r+t")) == NULL)
	{
		printf("File doesn't exit\n");
		exit(0);
	}

	ti = str_cli(fp, sockfd, &len);                       //perform the transmission and receiving
	rt = (len/(float)ti);                                         //caculate the average transmission rate
	printf("Time(ms) : %.3f, Data sent(byte): %d\nData rate: %f (Kbytes/s)\n", ti, (int)len, rt);

	close(sockfd);
	fclose(fp);
//}
	exit(0);
}

// Modified str_cli function in tcp_client3.c

float str_cli(FILE *fp, int sockfd, long *len) 
{
    char *buf;
    long lsize, ci = 0;
    struct pack_so packet;
    struct ack_so ack;
    int n, slen;
    float time_inv = 0.0;
    struct timeval sendt, recvt;
    uint32_t seq_no = 0;

    // Load file into buffer
    fseek(fp, 0, SEEK_END);
    lsize = ftell(fp);
    rewind(fp);
    printf("File length: %d bytes\n", (int)lsize);
    printf("Packet length: %d bytes\n", DATALEN);
    
    buf = (char *)malloc(lsize);
    if (buf == NULL) exit(2);
    fread(buf, 1, lsize, fp);
    buf[lsize] = '\0';

    gettimeofday(&sendt, NULL);

    // Transmit file using stop-and-wait
    while(ci <= lsize) {
        // Prepare packet
        packet.seq_no = seq_no;
        if((lsize + 1 - ci) <= DATALEN)
            slen = lsize + 1 - ci;
        else 
            slen = DATALEN;
        
        packet.len = slen;
        memcpy(packet.data, buf + ci, slen);
        packet.checksum = calculate_checksum(packet.data, slen);

        while(1) {
            // Send packet
            n = send(sockfd, &packet, sizeof(struct pack_so), 0);
            if(n == -1) {
                printf("Send error!\n");
                exit(1);
            }

            // Wait for acknowledgment
            n = recv(sockfd, &ack, sizeof(struct ack_so), 0);
            if(n == -1) {
                printf("Error receiving ACK\n");
                exit(1);
            }

            // Check acknowledgment
            if(ack.seq_no == seq_no && ack.status == ACK) {
                // Packet successfully received
                break;
            }
            // If NAK received, retransmit packet
            printf("Received NAK for packet %d, retransmitting\n", seq_no);
        }

        ci += slen;
        seq_no++;
    }

    gettimeofday(&recvt, NULL);
    *len = ci;
    tv_sub(&recvt, &sendt);
    time_inv += (recvt.tv_sec)*1000.0 + (recvt.tv_usec)/1000.0;
    free(buf);
    return(time_inv);
}

uint32_t calculate_checksum(char *data, int len) {
    uint32_t sum = 0;
    for(int i = 0; i < len; i++) {
        sum += (uint8_t)data[i];
    }
    return sum;
}

void tv_sub(struct  timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) <0)
	{
		--out ->tv_sec;
		out ->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}
