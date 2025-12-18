#include "network_channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <vector>

using namespace std;


/**
 * Creates a NetworkRequestChannel
 * 
 * @param ip IP address to connect to (client) or interface to bind to (server)
 *           Empty string for server side means bind to all interfaces
 * @param port Port number to use
 * @param side SERVER_SIDE to create a listening socket, CLIENT_SIDE to connect to a server
 * 
 * SERVER_SIDE behavior:
 * - Creates a socket and configures it for listening on the specified port
 * - Sets socket options to allow address reuse
 * 
 * CLIENT_SIDE behavior:
 * - Creates a socket and connects to the specified server address
 * - If the ip parameter is not a valid IP address, attempts to resolve it as a hostname
 * 
 * @throws Exits with error message if socket operations fail
 */

// Constructor for setting up a connection (server listening or client connecting)
NetworkRequestChannel::NetworkRequestChannel(const std::string& ip, int port, Side side) 
    : my_side(side), client_addr_len(sizeof(client_addr)) {
    
    // Initialize address structures to zero
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    
    if (side == SERVER_SIDE) {
        // create socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            throw runtime_error("socket() failed!");
        }

        // allow address reuse
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(sockfd);
            throw runtime_error("setsockopt() failed!");
        }

        // prep server_addr
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (ip.empty()) server_addr.sin_addr.s_addr = INADDR_ANY;
        else {
            if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
                close(sockfd);
                throw runtime_error("IP conversion failed!");
            }
        }

        // bind socket to address
        if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            throw runtime_error("bind() failed!");
        }

        // listen for connections
        if (listen(sockfd, 10) < 0) {
            close(sockfd);
            throw runtime_error("listen() failed!");
        }

        // Set peer information for logging purposes
        peer_ip = "0.0.0.0";
        peer_port = port;
        cout << "Server listening on port " << port << endl;
    } else {
        // create socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            throw runtime_error("socket() failed!");
        }

        // prep server_addr
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);

        // convert IP: text -> binary
        // try resolving as hostname
        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
            struct hostent* hostname = gethostbyname(ip.c_str());
            if (!hostname) {
                close(sockfd);
                throw runtime_error("IP conversion failed!");
            }
            memcpy(&server_addr.sin_addr, hostname->h_addr_list[0], hostname->h_length);
        }

        // connect to server
        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            throw runtime_error("connect() failed!");
        }

        // Store peer information for logging
        peer_ip = ip;
        peer_port = port;
        cout << "Connected to server at " << ip << ":" << port << endl;
    }
}

/**
 * Constructor for client connections accepted by a server
 * 
 * @param fd Socket file descriptor returned by accept()
 * 
 * This constructor initializes a NetworkRequestChannel using an existing
 * socket connection that was established by accepting a client connection.
 */
NetworkRequestChannel::NetworkRequestChannel(int fd) 
    : my_side(SERVER_SIDE), sockfd(fd), client_addr_len(sizeof(client_addr)) {
    
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    // Store the IP address and port in peer_ip and peer_port
    if (getpeername(fd, (struct sockaddr*)&addr, &len) == 0) {
        peer_ip = inet_ntoa(addr.sin_addr);
        peer_port = ntohs(addr.sin_port);
    }
    else {
        peer_ip = "Unknown";
        peer_port = -1;
    }
}

/**
 * Destructor
 * 
 * Cleans up resources used by this NetworkRequestChannel
 */
NetworkRequestChannel::~NetworkRequestChannel() {
    if (sockfd >= 0) close(sockfd);
}

/**
 * Accepts a new client connection on a server socket
 */
int NetworkRequestChannel::accept_connection() {
    client_addr_len = sizeof(client_addr);
    int newfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
    
    // Print connection information for logging purposes
    cout << "Accepted connection from " 
         << inet_ntoa(client_addr.sin_addr) << ":" 
         << ntohs(client_addr.sin_port) << endl;
    
    return newfd;
}

string NetworkRequestChannel::get_peer_address() const {
    return peer_ip + ":" + to_string(peer_port);
}

int NetworkRequestChannel::get_socket_fd() const {
    return sockfd;
}

/**
 * Sends a request to the server and waits for a response
 * 
 * @param req The Request object to send
 * @return Response from the server
 * 
 * This method:
 * Sends the length of the request followed by the request itself and receives the responses.
 * 
 * The wire format uses a 4-byte length header followed by the serialized data.
 * Request format: TYPE|USER_ID|AMOUNT|FILENAME|DATA
 * Response format: SUCCESS|BALANCE|DATA|MESSAGE
 * 
 * @throws May throw exceptions on network errors
 */
Response NetworkRequestChannel::send_request(const Request& req) {
    // Format: TYPE|USER_ID|AMOUNT|FILENAME|DATA
    stringstream ss;
    ss << static_cast<int>(req.type) << "|"
       << req.user_id << "|"
       << req.amount << "|"
       << req.filename << "|"
       << req.data;
    
    string request_str = ss.str();
    
    // Add message length as header (4 bytes)
    // Convert the request string length to network byte order
    uint32_t length = htonl(request_str.length());
    char length_buf[4];
    memcpy(length_buf, &length, 4);

    // send header
    ssize_t n = send(sockfd, length_buf, 4, 0);
    if (n != 4) {
        throw runtime_error("send() request length failed!");
    }

    // send body
    n = send(sockfd, request_str.c_str(), request_str.size(), 0);
    if (n != (ssize_t)request_str.size()) {
        throw runtime_error("send() request body failed!");
    }

    // receive header
    uint32_t resp_len_net;
    n = recv(sockfd, &resp_len_net, 4, MSG_WAITALL);
    if (n != 4) {
        throw runtime_error("recv() response length failed!");
    }
    
    // receive body
    uint32_t resp_len = ntohl(resp_len_net);
    vector<char> buffer(resp_len);
    n = recv(sockfd, buffer.data(), resp_len, MSG_WAITALL);
    if (n != (ssize_t)resp_len) {
        throw runtime_error("recv() response body failed!");
    }

    // parse response
    string resp(buffer.begin(), buffer.end());
    stringstream fields(resp);
    string success, balance, data, message;

    getline(fields, success, '|');
    getline(fields, balance, '|');
    getline(fields, data, '|');
    getline(fields, message, '|');

    return Response(success == "1", stod(balance), data, message);
}

/**
 * Receives a request from a client
 * 
 * @return The received Request object
 * 
 * This method:
 * Receives the length of the incoming request (4-byte header) and the actual data
 * 
 */
Request NetworkRequestChannel::receive_request() {
    uint32_t len_net;
    if (recv(sockfd, &len_net, 4, MSG_WAITALL) != 4) {
        throw runtime_error("recv() request length failed!");
    }

    uint32_t len = ntohl(len_net);
    vector<char> buffer(len);
    if (recv(sockfd, buffer.data(), len, MSG_WAITALL) != (ssize_t)len) {
        throw runtime_error("recv() request body failed!");
    }

    string req(buffer.begin(), buffer.end());
    return Request::parseRequest(req);
}

/**
 * Sends a response to a client
 * 
 * @param resp The Response object to send
 * 
 * This method:
 * Sends the length of the response followed by the response itself
 * 
 * The wire format uses a 4-byte length header followed by the serialized data.
 * Response format: SUCCESS|BALANCE|DATA|MESSAGE
 */
void NetworkRequestChannel::send_response(const Response& resp) {
    // Format: SUCCESS|BALANCE|DATA|MESSAGE
    stringstream ss;
    ss << (resp.success ? "1" : "0") << "|"
       << resp.balance << "|"
       << resp.data << "|"
       << resp.message;
    
    string response_str = ss.str();
    
    uint32_t len = htonl(response_str.size());
    if (send(sockfd, &len, 4, 0) != 4) {
        throw runtime_error("send() response length failed!");
    }

    if (send(sockfd, response_str.c_str(), response_str.size(), 0) != (ssize_t)response_str.size()) {
        throw runtime_error("send() response body failed!");
    }
}