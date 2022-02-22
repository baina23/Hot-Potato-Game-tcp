#include "potato.h"

using namespace std;

int main(int argc, char *argv[])
{
  int my_id;
  int num_players;
  int status;
  
  if (argc < 3) {
      cout << "Syntax: player <hostname> <port_name>\n" << endl;
      return 1;
  }
//*********************************** connect to master **********************************
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = argv[1];
  const char *port     = argv[2];
  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  status = -1;
  while(status == -1){
    socket_fd = socket(host_info_list->ai_family, 
          host_info_list->ai_socktype, 
          host_info_list->ai_protocol);
    if (socket_fd == -1) {
      cerr << "Error: cannot create socket" << endl;
      cerr << "  (" << hostname << "," << port << ")" << endl;
      close(socket_fd);
    } //if
  
    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
      cerr << "Error: cannot connect to socket" << endl;
      cerr << "  (" << hostname << "," << port << ")" << endl;
      close(socket_fd);
    } //if
  }
// ********************************* set my local server ***********************************


  int mysocket_fd;
  struct addrinfo my_info;
  struct addrinfo *my_info_list;
  
  memset(&my_info, 0, sizeof(my_info));

  my_info.ai_family   = AF_UNSPEC;
  my_info.ai_socktype = SOCK_STREAM;
  my_info.ai_flags    = AI_PASSIVE;

  char myhostname[128];
  gethostname(myhostname, sizeof(myhostname));
  
  status = getaddrinfo(myhostname, NULL, &my_info, &my_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << myhostname <<  ")" << endl;
    return -1;
  } //if


  mysocket_fd = socket(my_info_list->ai_family, 
		     my_info_list->ai_socktype, 
		     my_info_list->ai_protocol);
  if (mysocket_fd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << myhostname << ")" << endl;
    return -1;
  } //if

  int yes = 1;
  status = setsockopt(mysocket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(mysocket_fd, my_info_list->ai_addr, my_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot bind socket" << endl;
    //cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  // get port number
  struct sockaddr_in sa;
  socklen_t sa_len = sizeof(sa);
  if (getsockname(mysocket_fd, (struct sockaddr *)&sa, &sa_len) == -1) {
      perror("getsockname() failed");
      return -1;
   }
  
  int my_int_port = (int)ntohs(sa.sin_port);
  string myport_string = to_string(my_int_port);
  const char* myport = myport_string.c_str();

  //**************************** start listen at my local port ****************************
  status = listen(mysocket_fd, 100);
  if (status == -1) {
    cerr << "Error: cannot listen on socket" << endl; 
    cerr << "  (" << myhostname << "," << myport << ")" << endl;
    return -1;
  } //if

  //***************************** send my info to master *********************************
  struct info_from_player myinfo;
  strcpy(myinfo.ip, myhostname);
  strcpy(myinfo.port, myport);


  if(!send_until(socket_fd, &myinfo, sizeof(myinfo), 0)){
    cerr << "Error: cannot send on socket to host" << endl; 
    return -1;
  }

  //**************************** receive info from master *********************************
  struct info_to_player hostbuf;
  int bytes_recv = recv(socket_fd, &hostbuf, sizeof(hostbuf), MSG_WAITALL);
    if(bytes_recv == 0 || bytes_recv == -1){
      cerr << "Error: connection to master is closed!" << endl;
      return -1;
    }
  my_id = hostbuf.player_id;
  num_players = hostbuf.players_num;
  cout << "Connected as player " << my_id << " out of " << hostbuf.players_num << " total players" << endl;
  
  

  //****************************** connect to my neighbor **********************************
  int nbsocket_fd;
  struct addrinfo nb_info;
  struct addrinfo *nb_info_list;
  const char *nb_hostname = hostbuf.neighbor_ip;
  const char *nb_port     = hostbuf.neighbor_port;
  memset(&nb_info, 0, sizeof(nb_info));
  nb_info.ai_family   = AF_UNSPEC;
  nb_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(nb_hostname, nb_port, &nb_info, &nb_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << nb_hostname << "," << nb_port << ")" << endl;
    return -1;
  } //if

  status = -1;
  while(status == -1){
    nbsocket_fd = socket(nb_info_list->ai_family, 
          nb_info_list->ai_socktype, 
          nb_info_list->ai_protocol);
    if (nbsocket_fd == -1) {
      //cerr << "Error: cannot create socket for neighbor" << endl;
      //cerr << "  (" << nb_hostname << "," << nb_port << ")" << endl;
      close(nbsocket_fd);
    } //if
  
    status = connect(nbsocket_fd, nb_info_list->ai_addr, nb_info_list->ai_addrlen);
    if (status == -1) {
      //cerr << "Error: cannot connect to neighbor socket" << endl;
      //cerr << "  (" << nb_hostname << "," << nb_port << ")" << endl;
      close(nbsocket_fd);
    } //if
  }
cout << "finish connect to my neighbor!" << endl;

  //*************************** accept conncection from my neighbor *******************************

  struct sockaddr_storage mysocket_addr;
  socklen_t mysocket_addr_len = sizeof(mysocket_addr);
  int neighbor_fd;
  neighbor_fd = accept(mysocket_fd, (struct sockaddr *)&mysocket_addr, &mysocket_addr_len);
  if (neighbor_fd == -1) {
    cerr << "Error: cannot accept connection socket for neighbor" << endl;
    return -1;
  } //if
cout << "finish accept my neighbor!" << endl;
  //******************************* send ready info to master *************************************

  char ready_info[] = "I'm ready";
  if(!send_until(socket_fd, &ready_info, sizeof(ready_info), 0)){
    cerr << "Error: cannot send on socket to host" << endl; 
    return -1;
  }
  
  //******************************* receive and forward potato ************************************

  fd_set readfds;
  struct potato ptt;
  int rv = 0;

  srand(time(0));
  while(1){
    FD_ZERO(&readfds);
    FD_SET(socket_fd, &readfds);
    FD_SET(nbsocket_fd, &readfds);
    FD_SET(neighbor_fd, &readfds);
    int n = neighbor_fd + 1;

    rv = select(n, &readfds, NULL, NULL, NULL);
    
    if(rv == -1){
      perror("select");
      exit(1);
    }

    if(FD_ISSET(socket_fd, &readfds)){
      if(recv(socket_fd, &ptt, sizeof(ptt),MSG_WAITALL) == 0) break;
    }
    else if(FD_ISSET(nbsocket_fd, &readfds)){
      if(recv(nbsocket_fd, &ptt, sizeof(ptt),MSG_WAITALL) == 0) break;
    }
    else if(FD_ISSET(neighbor_fd, &readfds)){
      if(recv(neighbor_fd, &ptt, sizeof(ptt),MSG_WAITALL) == 0) break;
    }
    else continue;
    
    
    int randplayer = rand() % 2;   
    int fd = randplayer ? nbsocket_fd : neighbor_fd;
    int nb_id = randplayer ? (my_id + 1) % num_players : (my_id+num_players-1) % num_players ;
    ptt.hops--;
    ptt.trace[ptt.index] = my_id;
    ptt.index++;

    if(ptt.hops <= 0){
      cout << "I'm it" << endl;
      if(!send_until(socket_fd, &ptt, sizeof(ptt), 0)){
        cerr << "Error: cannot send on socket " << endl; 
        return -1;
      }
      cout << ptt.index << " : " << ptt.trace[ptt.index-1] << endl;
      break;
    }
    else {
      cout << "sending potato to " << nb_id << endl;
      if(!send_until(fd, &ptt, sizeof(ptt), 0)){
        cerr << "Error: cannot send on socket " << endl; 
        return -1;
      }
      cout << ptt.index << " : " << ptt.trace[ptt.index-1] << endl;
    }
   
  }

  // *************************** game over, close connections ********************************

  freeaddrinfo(host_info_list);
  freeaddrinfo(my_info_list);
  freeaddrinfo(nb_info_list);
  close(socket_fd);
  close(mysocket_fd);
  close(nbsocket_fd);
  close(neighbor_fd);

  return 0;
}
