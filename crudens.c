/* Written by Chris Osborn <fozztexx@fozztexx.com>
 *
 * Quick & dirty hack just to get an example nameserver working. Only
 * responds to A record requests and always returns 127.0.0.1 no
 * matter what is being looked up. Should be enough program to get
 * started so that you can connect to your own backend and add
 * additional functionality.
 *
 * Feel free to re-use this code however you see fit.
 */

/* DNS protocol reference:
   http://www.tcpipguide.com/free/t_DNSMessagingandMessageResourceRecordandMasterFileF.htm
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define PORT	53
#define BUFSIZE	2048

enum {
  DNSTypeA = 1,
  DNSTypeNS = 2,
  DNSTypeMD = 3,
  DNSTypeMF = 4,
  DNSTypeCNAME = 5,
  DNSTypeSOA = 6,
  DNSTypeMB = 7,
  DNSTypeMG = 8,
  DNSTypeMR = 9,
  DNSTypeNULL = 10,
  DNSTypeWKS = 11,
  DNSTypePTR = 12,
  DNSTypeHINFO = 13,
  DNSTypeMINFO = 14,
  DNSTypeMX = 15,
  DNSTypeTXT = 16,
  DNSTypeAAAA = 28,
  DNSTypeIXFR = 251,
  DNSTypeAXFR = 252,
  DNSTypeMAILB = 253,
  DNSTypeMAILA = 254,
  DNSTypeANY = 255,
};

typedef struct {
  unsigned xid:16;	// Identifier

  unsigned rd:1;	// Recursion Desired
  unsigned tc:1;	// Truncation Flag
  unsigned aa:1;	// Authoritive Answer Flag
  unsigned opcode:4;	// Operation Code
  unsigned qr:1;	// Query/Respons Flag

  unsigned rcode:4;	// Response Code
  unsigned cd:1;	// Checking Disabled
  unsigned at:1;	// Authentic Data
  unsigned unused:1;
  unsigned ra:1;	// Recursion Available

  unsigned qdcount:16;	// Question Count
  unsigned ancount:16;	// Answer Record Count
  unsigned nscount:16;	// Authority Record Count
  unsigned adcount:16;	// Additional Record Count
} DNSHeader;

typedef struct {
  DNSHeader header;
  unsigned char data;
} DNSPacket;

unsigned int decodeHostname(const unsigned char *data, unsigned int len)
{
  unsigned int size, total;


  total = len;
  for (;;) {
    size = *data;
    data++;
    len--;

    if (!size)
      break;
  
    if (size >= len)
      return -1;

    len -= size;
    data += size;
  }

  return total - len;
}

int main(int argc, char *argv[])
{
  int sock, err, len;
  struct sockaddr_in server, remote;
  unsigned char buf[BUFSIZE], buf2[BUFSIZE];
  socklen_t addrlen = sizeof(remote);
  DNSPacket *packet, *reply;
  unsigned int used;
  int qtype;
  

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock >= 0) {
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);
    err = bind(sock, (struct sockaddr *) &server, sizeof(server));
    if (err < 0) {
      close(sock);
      sock = -1;
    }
    else {
      for (;;) {
	len = recvfrom(sock, buf, BUFSIZE, 0, (struct sockaddr *) &remote, &addrlen);
	if (len > 0) {
	  packet = (DNSPacket *) buf;
	  used = decodeHostname(&packet->data,
				len - (((void *) &packet->data) - ((void *) packet)));

	  qtype = ntohs(*(uint16_t *) (&packet->data + used));

	  if (qtype == DNSTypeA) {
	    reply = (DNSPacket *) buf2;
	    bzero(reply, sizeof(DNSHeader));
	    reply->header.xid = packet->header.xid;
	    reply->header.qr = 1;
	    reply->header.opcode = packet->header.opcode;
	    reply->header.aa = 1;
	    reply->header.rd = packet->header.rd;

	    reply->header.ancount = htons(1);

	    memcpy(&reply->data, &packet->data, used + 4);
	    *(uint32_t *) (&reply->data + used + 4) = htonl(300);
	    *(uint16_t *) (&reply->data + used + 8) = htons(4);
	    *(uint32_t *) (&reply->data + used + 10) = htonl(0x7f000001);

	    sendto(sock, buf2, sizeof(DNSHeader) + used + 14, 0,
		   (struct sockaddr *) &remote, addrlen);
	  }
	}
	else
	  fprintf(stderr, "Error: %i\n", errno);
      }
    }
  }

  exit(0);
  return 0;
}
