#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

class ConnectionSocket {
private:
  int client_fd_;

public:
  ConnectionSocket(int client_fd) : client_fd_(client_fd) {}

  ~ConnectionSocket() { close(client_fd_); }

  std::string make_line() {
    int payload_length = rand() % 30 + 10;
    std::string banner_text;

    for (int i = 0; i < payload_length; i++) {
      banner_text += char(32 + rand() % 95);
    }

    // Check to see if starts with SSH banner text, replace
    if (banner_text.rfind("SSH-", 0) == 0) {
      banner_text[0] = 'X';
    }
    return banner_text + "\r\n";
  }

  void send_data() {
    // TODO: Make this vary +/- a couple of seconds
    const int WAIT_TIME = 10; // seconds
    // Loop that handles the connection lifetime
    while (1) {
      std::string message = make_line();
      if (send(client_fd_, message.data(), message.size(), 0) < 0) {
        throw runtime_error("Error sending message to client: " +
                            string(strerror(errno)));
      }

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

  // Loop to accept new connections
  while (1) {
    try {
      ConnectionSocket ssh_connection = ssh_server.accept_connection();
      ssh_connection.send_data();
    } catch (const runtime_error &e) {
      cerr << e.what() << "\n";
    }
  }

  return 0;
}
