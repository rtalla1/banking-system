#ifndef _NETWORK_CHANNEL_H_
#define _NETWORK_CHANNEL_H_

#include "common.h"
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
 * NetworkRequestChannel class
 * 
 * This class implements a communication channel using TCP/IP sockets
 * to replace the FIFO-based RequestChannel class. It will allow the banking 
 * system to work across a network instead of just on a single machine.
 */
class NetworkRequestChannel {
public:
    enum Side {SERVER_SIDE, CLIENT_SIDE};
    
    // For server: ip="" means listen on all interfaces
    // For client: connect to specified IP and port
    NetworkRequestChannel(const std::string& ip, int port, Side side);
    
    // For server: use after accept() returns a new client socket
    NetworkRequestChannel(int sockfd);
    
    ~NetworkRequestChannel();
    
    // Communication methods (similar to RequestChannel)
    Response send_request(const Request& req);
    Request receive_request();
    void send_response(const Response& resp);
    
    // New methods specific to networking
    int accept_connection(); // Returns socket fd for new connection
    std::string get_peer_address() const;
    int get_socket_fd() const;
    
private:
    Side my_side;
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    std::string peer_ip;
    int peer_port;
};

#endif