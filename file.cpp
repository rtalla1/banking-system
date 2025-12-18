#include "common.h"
#include "network_channel.h"
#include "thread_pool.h"
#include "signals.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <cstring>

using namespace std;

// Function to handle client connection
void handle_client(int client_sockfd, const vector<string>& allowed_extensions, int thread_count) {
    // Create a network channel from the socket
    NetworkRequestChannel channel(client_sockfd);
    string client_address = channel.get_peer_address();
    cout << "File server: new client connection from " << client_address << endl;
    
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
            
            Response resp;
            resp.success = true;
            
            if (r.type == UPLOAD_FILE) {
                // Check file extension if extensions were provided
                if (!allowed_extensions.empty()) {
                    size_t dot_pos = r.filename.find_last_of(".");
                    if (dot_pos == string::npos) {
                        resp.success = false;
                        resp.message = "File has no extension";
                        channel.send_response(resp);
                        continue;
                    }

                    string ext = r.filename.substr(dot_pos);
                    bool allowed = false;
                    for (const string& allowed_ext : allowed_extensions) {
                        if (ext == allowed_ext) {
                            allowed = true;
                            break;
                        }
                    }
                    
                    if (!allowed) {
                        resp.success = false;
                        resp.message = "File extension not allowed";
                        channel.send_response(resp);
                        continue;
                    }
                }
                
                string filepath = "storage/" + r.filename;
                ofstream outfile(filepath);
                
                if (!outfile) {
                    resp.success = false;
                    resp.message = "Failed to create file";
                } else {
                    outfile << r.data;
                    outfile.close();
                    resp.message = "File uploaded successfully";
                }
            }
            else if (r.type == DOWNLOAD_FILE) {
                string filepath = "storage/" + r.filename;
                ifstream infile(filepath);
                
                if (!infile) {
                    resp.success = false;
                    resp.message = "File not found";
                } else {
                    stringstream buffer;
                    buffer << infile.rdbuf();
                    resp.data = buffer.str();
                    resp.message = "File downloaded successfully";
                    infile.close();
                }
            }
            else {
                resp.success = false;
                resp.message = "Unknown RequestType";
            }

            channel.send_response(resp);
        }
        catch (const exception& e) {
            cerr << "Error handling client " << client_address << ": " << e.what() << endl;
            running = false;
        }
    }
    
    cout << "File server: client " << client_address << " disconnected" << endl;
}

void print_usage() {
    cout << "Usage: ./file_server [-p PORT] [-t THREAD_COUNT] [ALLOWED_EXTENSIONS...]" << endl;
    cout << "  -p, --port         Port number to listen on (default: 8001)" << endl;
    cout << "  -t, --threads      Number of threads in the thread pool (default: 4)" << endl;
    cout << "  -h, --help         Show this help message" << endl;
    cout << "  ALLOWED_EXTENSIONS List of allowed file extensions (e.g., .txt .pdf)" << endl;
}

int main(int argc, char* argv[]) {
    int port = 8001;
    int thread_count = 4;
    vector<string> allowed_extensions;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"threads", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:t:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                port = atoi(optarg);
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
    
    // Collect allowed extensions from remaining arguments
    for (int i = optind; i < argc; i++) {
        allowed_extensions.push_back(argv[i]);
    }
    
    // Setup signal handlers
    SignalHandling::setup_handlers();
    SignalHandling::log_signal_event("File server started on port " + to_string(port));
    
    // Create storage directory if it doesn't exist
    if (mkdir("storage", 0755) != 0 && errno != EEXIST) {
        cerr << "Error creating storage directory: " << strerror(errno) << endl;
        return 1;
    }
    
    try {
        NetworkRequestChannel file_channel("", port, NetworkRequestChannel::SERVER_SIDE);
        ThreadPool file_threads(thread_count);
        cout << "File server listening on port " << port << endl;
        
        // Print allowed extensions
        if (allowed_extensions.empty()) {
            cout << "All file extensions are allowed" << endl;
        } else {
            cout << "Allowed file extensions:";
            for (const string& ext : allowed_extensions) {
                cout << " " << ext;
            }
            cout << endl;
        }
        
        // Accept client connections until shutdown is requested
        while (!SignalHandling::shutdown_requested) {
            try {
                
                int client_fd = file_channel.accept_connection();

                
                if (errno == EINTR) {
                    continue;
                }

                file_threads.enqueue([client_fd, allowed_extensions, thread_count]() {
                    handle_client(client_fd, allowed_extensions, thread_count);
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
        
        cout << "File server shutting down..." << endl;
    }
    catch (const exception& e) {
        cerr << "Error starting file server: " << e.what() << endl;
    }
    
    SignalHandling::log_signal_event("File server shutdown complete");
    return 0;
}