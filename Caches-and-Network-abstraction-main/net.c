#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int total = 0;
  //Read until the entire length is read
  while (total<len){
    int byteRead = read(fd, buf + total, len - total);
    //if length is longer than length left to read
    if (byteRead <= 0){
      return false;
    }
    total += byteRead; 
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int total = 0;
  //write until you write the whole length
  while (total<len){
    int byteWritten = write(fd, buf + total, len - total);
    //error with writing if condition for while loop is not met and you get such a value
    if (byteWritten <= 0){
      return false;
    }
    total += byteWritten;
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  
  //read packet header, make buffer to read from sd and call nread
  uint8_t buf[HEADER_LEN];
  if (!nread(sd, HEADER_LEN, buf)){
    return false;
  }

  /*network to host after reading into buffer (reading packet header, not block yet)
  along with converting network to host, we also store op and return in the pass by ref arguments*/
  uint16_t len;
  memcpy(&len, buf, 2);
  len = ntohs(len);
  memcpy(op, buf+2, 4);
  *op = ntohl(*op);
  memcpy(ret, buf +6, 2);
  *ret = ntohs(*ret);

  /*check if the packet has a block and read it if it does. 
  JBOD_BLOCK_SIZE + the length of the header  should equal len 
  if there is a block in the packet */
  if(len == HEADER_LEN + JBOD_BLOCK_SIZE){
    if(!nread(sd, JBOD_BLOCK_SIZE, block)){
      return false;
    }
  }
  return true;
}


/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint16_t updatedLen = HEADER_LEN;
  //check if op command is write to block, if so add it to the total length
  if((op & 0x3F) == JBOD_WRITE_BLOCK){
    updatedLen += JBOD_BLOCK_SIZE;
  }
  //declare a packet based off of the readme of accurate length
  uint8_t packet[updatedLen];

  //host to net all the values so the network understands
  uint16_t htonLen = htons(updatedLen);
  uint32_t htonOp = htonl(op);
  uint16_t htonReturn = htons(0);

  memcpy(packet, &htonLen, 2);
  memcpy(packet + 2, &htonOp, 4);
  memcpy(packet + 6, &htonReturn, 2);

  //if op is write block, then add the block to the packet
  if((op & 0x3F) == JBOD_WRITE_BLOCK && block != NULL){
    memcpy(packet + HEADER_LEN, block, JBOD_BLOCK_SIZE);
  }

  //send packet
  if(!nwrite(sd, updatedLen, packet)){
    return false;
  }
  return true;
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  //coded this function by following the steps in the network lecture slides

  //create socket
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1){
    return false;
  }
  //defines correct port and type of connection
  struct sockaddr_in caddr;
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  if (inet_aton(ip, &caddr.sin_addr) == 0){
    return false;
  }
  //attempts to connect
  if (connect(cli_sd,(const struct sockaddr *) &caddr,sizeof(caddr)) == -1){
    return false;
  }
  return true;
}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  //if not connected, return error
  if(cli_sd == -1){
    return -1;
  }

  uint32_t recvOp;
  uint16_t recvRet;
  //send packet with provided arugments using the send_packet function
  if(!send_packet(cli_sd, op, block)){
    return -1;
  }
  //recieve the packet using recv_packet function
  if(!recv_packet(cli_sd, &recvOp, &recvRet, block)){
    return -1;
  }
  return 0;
}
