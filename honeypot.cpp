#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

enum LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

class Logger {
private:
  string filename_;
  ofstream log_file_;
  LogLevel log_level_;

  Logger(LogLevel log_level, const string &filename)
      : log_level_(log_level), filename_(filename) {
    string string_log_level = log_level_to_string(log_level_);
    log_file_.open(filename, ios::app);
    if (!log_file_.is_open()) {
      throw runtime_error("Error opening log file: " +
                          string(strerror((errno))));
    }
  }

public:
  // Singleton logger implementation, otherwise would have to refactor
  // classes to take Logger object as param
  static Logger &instance() {
    static Logger logger(INFO, "app.log");
    return logger;
  }

  string log_level_to_string(LogLevel log_level_) {
    switch (log_level_) {
    case DEBUG:
      return "DEBUG";
    case INFO:
      return "INFO";
    case WARNING:
      return "WARNING";
    case ERROR:
      return "ERROR";
    case CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
    }
  }

  ~Logger() { log_file_.close(); }

  void log(LogLevel log_level_, const string &message) {
    string string_log_level = log_level_to_string(log_level_);

    // Get current timestamp
    time_t now = time(0);
    tm *timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    ostringstream log_entry;
    log_entry << "[" << timestamp << "] " << string_log_level << ": " << message
              << endl;

    // Output to console
    // Add logic here to filter based on enum, i.e. INFO doesn't show DEBUG
    cout << log_entry.str();

    // Output to log file
    if (log_file_.is_open()) {
      log_file_ << log_entry.str();
      log_file_.flush();
    }
  }
};

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
    const int WAIT_TIME = 5; // seconds
    // Loop that handles the connection lifetime
    while (1) {
      std::string message = make_line();
      if (send(client_fd_, message.data(), message.size(), 0) < 0) {
        Logger::instance().log(INFO, "Client disconnected");
        throw runtime_error("Error sending message to client: " +
                            string(strerror(errno)));
        break;
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

    Logger::instance().log(DEBUG, "ServerSocket constructed");
    /*Before calling bind(), neewd to set up sockaddr_in struct*/
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(bind_port_);

    Logger::instance().log(DEBUG, "Socket binded");
    /*Handle error if throws < 0 (fail)*/
    if (bind(socket_fd_, (struct sockaddr *)&address, sizeof(address)) < 0) {
      throw runtime_error("Error binding: " + string(strerror(errno)));
    }
  }

  ~ServerSocket() { close(socket_fd_); }

  /*TODO: Change connection_queue amount to 5 when threading added*/
  void listen_for_connections() {
    int CONNECTION_QUEUE = 5;

    /*Handle error the same way as bind failure < 0*/
    if (listen(socket_fd_, CONNECTION_QUEUE) < 0) {
      throw runtime_error("Error listening: " + string(strerror(errno)));
    }
  }

  ConnectionSocket accept_connection() {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd = accept(socket_fd_, (struct sockaddr *)&address, &addrlen);
    char connection_ipv4[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, connection_ipv4, INET_ADDRSTRLEN);
    Logger::instance().log(INFO, string("Accepting connection from ") +
                                     connection_ipv4);
    if (client_fd < 0) {
      throw runtime_error("Error accepting connection: " +
                          string(strerror(errno)));
    } else
      Logger::instance().log(DEBUG,
                             "Creating additional socket for connection");
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
