#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string>
#include <time.h>

struct potato{
    int trace[1000];
    int hops;
    int index;
};

struct info_from_player{
    char ip[128];
    char port[16];
};

struct info_to_player{
    int player_id;
    int players_num;
    char neighbor_ip[128];
    char neighbor_port[16];
};


bool send_until(int sockfd, const void *msg, int len, int flags){
    int x = 0;
    while(x < len){
    int bytes_send = send(sockfd, (char*)msg + x, len-x, flags);
    if(bytes_send == -1) return false;
    x = x + bytes_send;
    }
    return true;
};


