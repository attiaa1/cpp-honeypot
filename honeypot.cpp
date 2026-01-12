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

std::string sockaddr_to_string(const sockaddr_in &address) {
  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &address.sin_addr, ip_str, INET_ADDRSTRLEN);
  return std::string(ip_str);
}

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
    /*Change this to test logger filter logic*/
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

  void log(LogLevel log_level, const string &message) {
    string string_log_level = log_level_to_string(log_level_);

    if (log_level < log_level_) {
      /*Don't log*/
      return;
    }
    // Get current timestamp
    time_t now = time(0);
    tm *timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    ostringstream log_entry;
    log_entry << "[" << timestamp << "] " << string_log_level << ": " << message
              << endl;

    // Output to console
    cout << log_entry.str();
    cout.flush();

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
  time_t connection_start_;

public:
  ConnectionSocket(int client_fd) : client_fd_(client_fd) {
    connection_start_ = time(0);
  }

  ~ConnectionSocket() {
    time_t connection_end = time(0);
    std::string connection_duration =
        std::to_string(connection_end - connection_start_);
    Logger::instance().log(INFO, "Connection lasted " + connection_duration +
                                     " seconds");
    close(client_fd_);
  }

  string make_line() {
    int payload_length = rand() % 30 + 10;
    string banner_text;

    for (int i = 0; i < payload_length; i++) {
      banner_text += char(32 + rand() % 95);
    }

    // Check to see if starts with SSH banner text, replace
    if (banner_text.rfind("SSH-", 0) == 0) {
      banner_text[0] = 'X';
    }
    return banner_text + "\r\n";
  }

  bool is_connection_alive() {
    char connection_status_buffer[1];
    ssize_t connection_pulse =
        recv(client_fd_, connection_status_buffer,
             sizeof(connection_status_buffer), MSG_PEEK | MSG_DONTWAIT);

    Logger::instance().log(
        DEBUG, "recv() returned: " + std::to_string(connection_pulse) +
                   ", errno: " + std::to_string(errno));

    if (connection_pulse == 0) {
      Logger::instance().log(INFO, "Client disconnected");
      return false;
    } else if (connection_pulse < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        Logger::instance().log(ERROR,
                               "Connection error: " + string(strerror(errno)));
        return false;
      }
    } else {
      /*Handle bug if client sends data, SSH only, discard*/
      char discard_buffer[1024];
      recv(client_fd_, discard_buffer, sizeof(discard_buffer), MSG_DONTWAIT);
      Logger::instance().log(DEBUG, "Recieved and discarded " +
                                        std::to_string(connection_pulse) +
                                        "bytes");
    }
    return true;
  }

  void send_data() {
    const int WAIT_TIME = 10; // seconds
    // Loop that handles the connection lifetime
    while (1) {
      if (!is_connection_alive()) {
        break;
      }

      string message = make_line();
      Logger::instance().log(DEBUG,
                             "Sent " + to_string(message.size()) + "bytes");
      if (send(client_fd_, message.data(), message.size(), 0) <= 0) {
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

    Logger::instance().log(INFO, "Starting honeypot on port " +
                                     std::to_string(bind_port_));

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

  /*TODO: Change connection_queue aount to 5 when threading added*/
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
    std::string connection_ipv4 = sockaddr_to_string(address);

    if (client_fd < 0) {
      throw runtime_error("Error accepting connection: " +
                          string(strerror(errno)));
    } else {
      Logger::instance().log(INFO, string("Accepting connection from ") +
                                       connection_ipv4);
      return ConnectionSocket(client_fd);
    }
  }
};

int main() {
  ServerSocket honeypot(2222);
  honeypot.listen_for_connections();

  // Loop to accept new connections
  while (1) {
    try {
      ConnectionSocket honeypot_connection = honeypot.accept_connection();
      honeypot_connection.send_data();
    } catch (const runtime_error &e) {
      Logger::instance().log(ERROR, string("Exception caught: ") + e.what());
    }
  }

  return 0;
}
