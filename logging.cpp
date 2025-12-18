#include "common.h"
#include "network_channel.h"
#include "thread_pool.h"
#include "signals.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <unistd.h>
#include <getopt.h>
#include <cstring>

using namespace std;

// Mutex for log file access
mutex log_mutex;

// Function to handle client connection
void handle_client(int client_sockfd, const string& log_file, int thread_count) {
    // Create a network channel from the socket
    NetworkRequestChannel channel(client_sockfd);
    string client_address = channel.get_peer_address();
    cout << "Logging server: new client connection from " << client_address << endl;
    
    bool running = true;
    
    while (running && !SignalHandling::shutdown_requested) {
        try {
            // Receive request
            Request r = channel.receive_request();
            
            if (r.type == QUIT) {
                Response resp(true, 0, "", "Server acknowledged disconnect");
                channel.send_response(resp);
                running = false;
                continue;
            }
            
            // Lock the log file for writing
            lock_guard<mutex> lock(log_mutex);
            ofstream logfile(log_file, ios::app);
            
            if (!logfile) {
                Response resp(false, 0, "", "Failed to open log file");
                channel.send_response(resp);
                continue;
            }

            logfile << "[" << r.user_id << "]: ";
            
            switch(r.type) {
                case LOGIN:
                    logfile << "logged in from " << client_address;
                    break;
                case LOGOUT:
                    logfile << "logged out from " << client_address;
                    break;
                case DEPOSIT:
                    logfile << "deposited " << r.amount;
                    break;
                case WITHDRAW:
                    logfile << "withdrew " << r.amount;
                    break;
                case BALANCE:
                    logfile << "viewed balance: " << r.amount;
                    break;
                case EARN_INTEREST:
                    logfile << "accrued interest in all accounts";
                    break;
                case UPLOAD_FILE:
                    logfile << "uploaded file: " << r.filename;
                    break;
                case DOWNLOAD_FILE:
                    logfile << "downloaded file: " << r.filename;
                    break;
                default:
                    logfile << "unknown action (type=" << r.type << ")";
            }
            logfile << endl;
            logfile.close();

            Response resp;
            resp.success = true;
            resp.message = "Logged successfully";
            
            channel.send_response(resp);
        }
        catch (const exception& e) {
            cerr << "Error handling client " << client_address << ": " << e.what() << endl;
            running = false;
        }
    }
    
    cout << "Logging server: client " << client_address << " disconnected" << endl;
}

void print_usage() {
    cout << "Usage: ./logging_server [-p PORT] [-f LOG_FILE] [-t THREAD_COUNT]" << endl;
    cout << "  -p, --port         Port number to listen on (default: 8002)" << endl;
    cout << "  -f, --file         Log file to write to (default: system.log)" << endl;
    cout << "  -t, --threads      Number of threads in the thread pool (default: 4)" << endl;
    cout << "  -h, --help         Show this help message" << endl;
}

int main(int argc, char* argv[]) {
    int port = 8002;
    string log_file = "system.log";
    int thread_count = 4;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"file", required_argument, 0, 'f'},
        {"threads", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:f:t:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                log_file = optarg;
                break;
            case 't':
                thread_count = atoi(optarg);
                break;
            case 'h':
                print_usage();
                return 0;
            case '?':
                print_usage();
                return 1;
            default:
                abort();
        }
    }
    
    // Setup signal handlers
    SignalHandling::setup_handlers();
    SignalHandling::log_signal_event("Logging server started on port " + to_string(port));
    
    // Create initial log file entry
    {
        lock_guard<mutex> lock(log_mutex);
        ofstream logfile(log_file, ios::app);
        if (logfile) {
            logfile << "=== Logging server started on port " << port << " ===" << endl;
            logfile.close();
        } else {
            cerr << "Error: Could not open log file " << log_file << endl;
            return 1;
        }
    }
    
    try {
        NetworkRequestChannel logging_channel("", port, NetworkRequestChannel::SERVER_SIDE);
        ThreadPool logging_threads(thread_count);
        cout << "Logging server listening on port " << port << endl;
        cout << "Writing logs to " << log_file << endl;
        
        // Accept client connections until shutdown is requested
        while (!SignalHandling::shutdown_requested) {
            try {
                int client_fd = logging_channel.accept_connection();

                if (errno == EINTR) {
                    continue;
                }

                logging_threads.enqueue([client_fd, log_file, thread_count]() {
                    handle_client(client_fd, log_file, thread_count);
                });
            }
            catch (const exception& e) {
                cerr << "Error accepting connection: " << e.what() << endl;
                
                // Check if we need to exit
                if (SignalHandling::shutdown_requested) {
                    break;
                }
                
                // Short delay before retry
                sleep(1);
            }
        }
        
        cout << "Logging server shutting down..." << endl;
        
        // Add shutdown entry to log
        {
            lock_guard<mutex> lock(log_mutex);
            ofstream logfile(log_file, ios::app);
            if (logfile) {
                logfile << "=== Logging server shutdown ===" << endl;
                logfile.close();
            }
        }
    }
    catch (const exception& e) {
        cerr << "Error starting logging server: " << e.what() << endl;
    }
    
    SignalHandling::log_signal_event("Logging server shutdown complete");
    return 0;
}