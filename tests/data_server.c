#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include "data_server.h"
#include "scrambled_frame.h"

#define MAX_PACKETS 4000

int main(int argc, char *argv[]){
  udp_packet *packet;
  int num_packets;

  num_packets = setup_packets(&packet, CIN_PACKET_LEN, 
                              scrambled_frame, scrambled_frame_len - 312);

  start_server(packet, num_packets);

  return(0);
}

int setup_packets(udp_packet **packets, int packet_size, 
                  unsigned char* stream, unsigned int stream_len){
  unsigned char* stream_p;
  int packet_len;
  int bytes_left;
  int num_packets = 0;
  udp_packet *packet_p;

  *packets = (udp_packet *)malloc(MAX_PACKETS * sizeof(udp_packet));

  bytes_left = stream_len;
  stream_p = stream;
  packet_p = *packets;
  while(bytes_left > 0){
    packet_p->data = malloc(CIN_MAX_MTU * sizeof(unsigned char));
    if(!packet_p->data){
      return 0;
    }

    if((bytes_left - packet_size) >= 0){
      packet_len = packet_size;
    } else {
      packet_len = (unsigned int)bytes_left;
    }

    packet_p->len = packet_len + 8;

    *((uint64_t*)(packet_p->data)) = 0x0000F4F3F2F1F000;
    *(packet_p->data) = (unsigned char)num_packets;
    memcpy((packet_p->data + 8), stream_p, packet_len );
    stream_p += packet_len;
    bytes_left -= packet_len;
    num_packets++;
    packet_p++;
  }
  return num_packets;
}

int start_server(udp_packet *packets, int num_packets){
  int s; /* socket */
  struct sockaddr_in dest_addr;
  udp_packet* packet_p;
  uint16_t frame_num;
  int i = 1;
  int d;
  struct timespec delay = {0,5e6};

  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port   = htons(49201);
  inet_pton(AF_INET, "10.0.5.23", &dest_addr.sin_addr);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if(s < 0){
    perror("Unable to open socket :");
    return 1;
  }
  
  if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0){
    perror("setsockopt() failed :");
    return 1;
  }

  frame_num = 0;

  while(1){
    packet_p = packets;
    for(i=0;i<num_packets;i++){
      packet_p->data[6] = (char)(frame_num >> 8);
      packet_p->data[7] = (char)(frame_num & 0xFF);
      d = sendto(s, packet_p->data, packet_p->len, 0, 
             (struct sockaddr*) &dest_addr, sizeof(dest_addr));
      packet_p++;
    }
    nanosleep(&delay, NULL);
    frame_num++;
  }

  close(s);
}
