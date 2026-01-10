#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

class ConnectionSocket {
private:
  int client_fd_;

public:
  ConnectionSocket(int client_fd) : client_fd_(client_fd) {}

  ~ConnectionSocket() { close(client_fd_); }

  /*Refactor to use EndleSSH implementation, random gibberish as banner*/
  void send_data() {
    const char *message = "SSH-2.0-OpenSSH_8.9\r\n";
    const char *p = message;
    int WAIT_TIME = 5; // seconds

    /*While character is not null terminator*/
    while (*p) {
      send(client_fd_, &p, 1, 0);
      p++;
      sleep(WAIT_TIME);
    }
  }
};

class ServerSocket {
private:
  int socket_fd_;
  int bind_port_;

public:
  ServerSocket(int bind_port) : bind_port_(bind_port) {
    /*AF_INET = ipv4, SOCK_STREAM & 0 = TCP*/
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
      throw runtime_error("Error creating socket: " + string(strerror(errno)));
    }

    /*Before calling bind(), neewd to set up sockaddr_in struct*/
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(bind_port_);

    /*Handle error if throws < 0 (fail)*/
    if (bind(socket_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
      throw runtime_error("Error binding: " + string(strerror(errno)));
    }
  }

  ~ServerSocket() { close(socket_fd_); }

  /*TODO: Change connection_queue amount to 5 when threading added*/
  void listen_for_connections() {
    int CONNECTION_QUEUE = 1;

    /*Handle error the same way as bind failure < 0*/
    if (listen(socket_fd_, CONNECTION_QUEUE) < 0) {
      throw runtime_error("Error listening: " + string(strerror(errno)));
    }
  }

  /*TODO: Refactor with threading to simultaneously hold multiple connections*/
  ConnectionSocket accept_connection() {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd = accept(socket_fd_, (struct sockaddr *)&address, &addrlen);
    if (client_fd < 0) {
      throw runtime_error("Error accepting connection: " +
                          string(strerror(errno)));
    } else
      return ConnectionSocket(client_fd);
  }
};

int main() {
  ServerSocket ssh_server(2222);
  ssh_server.listen_for_connections();

  while(1) {
    try {
     ConnectionSocket ssh_connection = ssh_server.accept_connection(); 
     ssh_connection.send_data();
    }
    catch (const runtime_error& e) {
      cerr << e.what() << "\n";
    }
  }

  return 0;
}
