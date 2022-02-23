#include <vector>
#include <arpa/inet.h>
#include "potato.h"

using namespace std;

int main(int argc, char *argv[])
{

  // ********************************** set up server ****************************************
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = NULL;
  //const char *port     = "4444";


  if (argc < 4) {
    cerr << "Error: miss arguments: ringmaster <port_num> <num_players> <num_hops>" << endl;
    return -1;
  } //if

  const char *port = argv[1];
  const int num_players = atoi(argv[2]);
  const int num_hops = atoi(argv[3]);

  cout << "Potato Ringmaster" << endl;
  cout << "Players = " << num_players << endl;
  cout << "Hops = " << num_hops << endl;

  if(num_hops < 1 || num_players < 1) return 0;
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags    = AI_PASSIVE;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  int yes = 1;
  status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot bind socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  status = listen(socket_fd, 100);
  if (status == -1) {
    cerr << "Error: cannot listen on socket" << endl; 
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if



// ******************** accept player connections and receive info from player ********************

  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  vector<string> players_ip;
  vector<string> players_port;
  
  int client_connection_fd[num_players];

  for(int i = 0; i < num_players; ++i){

    client_connection_fd[i] = accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
    if (client_connection_fd[i] == -1) {
      cerr << "Error: cannot accept connection on socket" << " player:" << i << endl;
      return -1;
    } //if

    struct info_from_player playerbuf;
    int bytes_recv = recv(client_connection_fd[i], &playerbuf, sizeof(playerbuf), MSG_WAITALL);
    if(bytes_recv == 0 || bytes_recv == -1){
      cerr << "Error: connection to player " << i << " is closed!" << endl;
      return -1;
    }
   
    struct sockaddr_in *s_addr = (struct sockaddr_in *) &socket_addr;
    //string _ip = inet_ntoa(s_addr->sin_addr);
    string _ip = playerbuf.ip;
    players_ip.push_back(_ip);
    string _port = playerbuf.port;
    players_port.push_back(_port);

  }

// ************************************* send info to players *************************************
  fd_set readfds0;
  FD_ZERO(&readfds0);
  for(int i = 0; i < num_players; ++i){
    struct info_to_player masterbuf;
    int masterbuf_len = sizeof(masterbuf);
    strcpy(masterbuf.neighbor_ip, players_ip[(i+1) % num_players].c_str());
    strcpy(masterbuf.neighbor_port, players_port[(i+1) % num_players].c_str());
    masterbuf.player_id = i;
    masterbuf.players_num = num_players;
    FD_SET(client_connection_fd[i],&readfds0);
    if(!send_until(client_connection_fd[i], &masterbuf, masterbuf_len, 0))
      return -1;
   
  }

// ******************************** receive ready info from players ******************************

  
  int count_ready = 0;
  int n0 = client_connection_fd[num_players-1] + 1;
  char buf_ready[32];
  struct timeval timeout;
  timeout.tv_sec = 3;
  timeout.tv_usec = 500000;
  while(count_ready < num_players){
    int rv;
    while((rv = select(n0, &readfds0, NULL, NULL, &timeout)) == 0);
    if(rv == -1){
      perror("select");
      return -1;
    }
    else {
      for(int i = 0; i < num_players; i++){
        if(FD_ISSET(client_connection_fd[i], &readfds0)){
          int recv_ready = recv(client_connection_fd[i], &buf_ready, sizeof(buf_ready),0);
          if(recv_ready == 0) {
            cout << "lose conncetion to player " << i << endl;
            return -1;
          }
          count_ready ++;
          cout << "Player "<< i << " is ready to play" << endl;
        }
      }
    }
    FD_ZERO(&readfds0);
    for(int i = 0; i < num_players; ++i)
      FD_SET(client_connection_fd[i],&readfds0);
  }
  

// ******************************** send potato to a random player *******************************
  struct potato boom;
  boom.hops = num_hops;
  boom.index = 0;
  int potato_size = sizeof(boom);
  int* trace = boom.trace;
  memset(trace, -1, sizeof(boom.trace));
  srand(time(NULL));
  int randplayer = rand() % num_players;
  
  fd_set readfds;
  FD_ZERO(&readfds);
  for(int i = 0; i < num_players; ++i)
    FD_SET(client_connection_fd[i],&readfds);
  
  if(!send_until(client_connection_fd[randplayer], &boom, potato_size,0)) 
    return -1;

  cout << "Ready to start the game, sending potato to player " << randplayer << endl;

// ************************************** receive the potato *************************************

  
  struct potato buf_res;
  struct potato tmp;
  int recv_potato = 0;

  while(recv_potato == 0){
    
    int n = client_connection_fd[num_players-1] + 1;
    int rv;
    rv = select(n, &readfds, NULL, NULL, NULL);
    
    if(rv == -1)
      perror("select");
    else {
      for(int i = 0; i < num_players; i++){
        if(FD_ISSET(client_connection_fd[i], &readfds)){
          recv_potato = recv(client_connection_fd[i], &tmp, sizeof(tmp),MSG_WAITALL);
          if(recv_potato > 0) {
            buf_res = tmp;
            break;
          }
        }
      }
    }
  }

  // ******************************* game over, close connections ********************************

  cout << "Trace of potato:" << endl;
  int m = 0;
  cout << buf_res.trace[m++];
  while(buf_res.trace[m] != -1){
    cout << ", " << buf_res.trace[m++];
  }
  cout << endl;

  freeaddrinfo(host_info_list);
  close(socket_fd);
  for(int i = 0; i < num_players; ++i)
    close(client_connection_fd[i]);

  return 0;
}
