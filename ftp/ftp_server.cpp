#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <mutex>

// --- Configuration ---
const int BUFFER_SIZE = 4096;
const std::string DEFAULT_USER = "user";
const std::string DEFAULT_PASS = "pass";

// --- FTP Status Codes ---
enum Code {
    DATA_OPEN = 150,
    OK = 200,
    SYST_NAME = 215,
    READY = 220,
    GOODBYE = 221,
    TRANSFER_COMPLETE = 226,
    LOGIN_SUCCESS = 230,
    PASS_REQ = 331,
    CANT_OPEN_DATA = 425,
    UNKNOWN_CMD = 500,
    SYNTAX_ERR = 501,
    NOT_LOGGED_IN = 530,
    FILE_UNAVAILABLE = 550
};

// --- Helper Utilities ---
void log(const std::string& msg) {
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "[LOG] " << msg << std::endl;
}

// --- Session Handler (One per client) ---
class FtpSession {
private:
    int control_sock;
    struct sockaddr_in client_addr;
    bool authenticated = false;
    std::string username;
    bool binary_mode = false;
    
    // Active Data Connection Info (from PORT command)
    std::string data_ip;
    int data_port = 0;

public:
    FtpSession(int socket, struct sockaddr_in addr) 
        : control_sock(socket), client_addr(addr) {}

    ~FtpSession() {
        if (control_sock != -1) close(control_sock);
        log("Client disconnected: " + get_client_ip());
    }

    std::string get_client_ip() {
        return std::string(inet_ntoa(client_addr.sin_addr));
    }

    void send_response(int code, const std::string& msg) {
        std::string payload = std::to_string(code) + " " + msg + "\r\n";
        send(control_sock, payload.c_str(), payload.length(), 0);
    }

    // Connect back to the client (Active Mode)
    int establish_data_connection() {
        if (data_port == 0 || data_ip.empty()) return -1;

        int data_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (data_sock < 0) return -1;

        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(data_port);
        inet_pton(AF_INET, data_ip.c_str(), &dest_addr.sin_addr);

        if (connect(data_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
            close(data_sock);
            return -1;
        }
        return data_sock;
    }

    void handle_port(const std::string& params) {
        // Expected format: h1,h2,h3,h4,p1,p2
        std::string params_copy = params;
        std::replace(params_copy.begin(), params_copy.end(), ',', ' ');
        std::stringstream ss(params_copy);
        int h1, h2, h3, h4, p1, p2;
        
        if (ss >> h1 >> h2 >> h3 >> h4 >> p1 >> p2) {
            data_ip = std::to_string(h1) + "." + std::to_string(h2) + "." + std::to_string(h3) + "." + std::to_string(h4);
            data_port = p1 * 256 + p2;
            send_response(OK, "PORT command successful.");
            log("Client " + get_client_ip() + " set active data port to " + data_ip + ":" + std::to_string(data_port));
        } else {
            send_response(SYNTAX_ERR, "Invalid PORT parameters.");
        }
    }

    void handle_list() {
        if (!authenticated) {
            send_response(NOT_LOGGED_IN, "Please login first.");
            return;
        }

        int data_sock = establish_data_connection();
        if (data_sock < 0) {
            send_response(CANT_OPEN_DATA, "Failed to open data connection. Did you send PORT?");
            return;
        }

        send_response(DATA_OPEN, "Here comes the directory listing.");

        std::string listing;
        DIR* dir;
        struct dirent* ent;
        if ((dir = opendir(".")) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                std::string fname = ent->d_name;
                if (fname == "." || fname == "..") continue;
                // Format: permissions links owner group size month day time name
                // Sending a simplified listing for compatibility
                listing += "-rw-r--r-- 1 ftp ftp 0 Jan 01 00:00 " + fname + "\r\n";
            }
            closedir(dir);
        }

        ssize_t sent = send(data_sock, listing.c_str(), listing.length(), 0);
        if (sent < 0) {
            log("Error sending directory listing to client");
        }
        close(data_sock);
        send_response(TRANSFER_COMPLETE, "Directory send OK.");
    }

    void handle_retr(const std::string& filename) {
        if (!authenticated) {
            send_response(NOT_LOGGED_IN, "Please login first.");
            return;
        }

        // Security: Prevent directory traversal attacks
        if (filename.find("..") != std::string::npos || filename.find("/") != std::string::npos || 
            filename.find("\\") != std::string::npos || filename.empty()) {
            send_response(FILE_UNAVAILABLE, "Invalid filename.");
            return;
        }

        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            send_response(FILE_UNAVAILABLE, "File not found.");
            return;
        }

        int data_sock = establish_data_connection();
        if (data_sock < 0) {
            file.close();
            send_response(CANT_OPEN_DATA, "Failed to open data connection.");
            return;
        }

        send_response(DATA_OPEN, "Opening data connection for " + filename);

        char buffer[BUFFER_SIZE];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            ssize_t sent = send(data_sock, buffer, file.gcount(), 0);
            if (sent < 0) {
                log("Error sending data to client");
                break;
            }
            if (file.gcount() == 0) break;  // EOF
        }

        file.close();
        close(data_sock);
        send_response(TRANSFER_COMPLETE, "Transfer complete.");
        log("Sent file " + filename + " to " + get_client_ip());
    }

    void run() {
        send_response(READY, "Simple C++ FTP Server Ready.");
        
        char buffer[BUFFER_SIZE];
        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = recv(control_sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) break;

            std::string raw(buffer);
            // Remove \r\n
            while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.pop_back();

            if (raw.empty()) continue;

            std::stringstream ss(raw);
            std::string cmd, args;
            ss >> cmd;
            std::getline(ss, args);
            if (!args.empty() && args[0] == ' ') args.erase(0, 1);

            // Log command (avoid logging passwords)
            if (cmd != "PASS") log(get_client_ip() + " CMD: " + cmd + " " + args);

            if (cmd == "USER") {
                username = args;
                send_response(PASS_REQ, "Password required.");
            } else if (cmd == "PASS") {
                if (username == DEFAULT_USER && args == DEFAULT_PASS) {
                    authenticated = true;
                    send_response(LOGIN_SUCCESS, "Login successful.");
                } else if (username == "anon") {
                    authenticated = true;
                    send_response(LOGIN_SUCCESS, "Anonymous login successful.");
                } else {
                    send_response(NOT_LOGGED_IN, "Login failed.");
                }
            } else if (cmd == "PORT") {
                handle_port(args);
            } else if (cmd == "LIST" || cmd == "NLST") {
                handle_list();
            } else if (cmd == "RETR") {
                handle_retr(args);
            } else if (cmd == "TYPE") {
                if (args == "I") binary_mode = true;
                else binary_mode = false;
                send_response(OK, "Type set to " + args);
            } else if (cmd == "SYST") {
                send_response(SYST_NAME, "UNIX Type: L8");
            } else if (cmd == "FEAT") {
                send_response(SYST_NAME, "No features.");
            } else if (cmd == "QUIT") {
                send_response(GOODBYE, "Goodbye.");
                break;
            } else {
                send_response(UNKNOWN_CMD, "Unknown command.");
            }
        }
    }
};

// --- Main Server ---
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./ftp_server <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (server_sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Allow immediate reuse of the port
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        return 1;
    }

    log("FTP Server started on port " + std::to_string(port));
    log("Waiting for connections...");

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);

        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        log("New connection accepted.");

        // Spawn a thread for the new client
        std::thread([client_sock, client_addr]() {
            FtpSession session(client_sock, client_addr);
            session.run();
        }).detach();
    }

    close(server_sock);
    return 0;
}