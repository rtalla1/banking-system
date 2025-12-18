#include "common.h"
#include "network_channel.h"
#include "thread_pool.h"
#include "signals.h"
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <getopt.h>
#include <cstring>

using namespace std;

class Account {
public:
    int id;
    double balance;
    bool active;
    mutex account_mutex; // Add mutex for thread safety
    
    Account() : id(-1), balance(0.0), active(false) {}
    Account(int _id) : id(_id), balance(0.0), active(true) {}
    
    // Explicitly delete copy constructor and assignment operator
    Account(const Account&) = delete;
    Account& operator=(const Account&) = delete;
    
    // Also delete move operations since mutex is not movable
    Account(Account&&) = delete;
    Account& operator=(Account&&) = delete;
    
    // Method to initialize an account with an ID
    void initialize(int _id) {
        lock_guard<mutex> lock(account_mutex);
        id = _id;
        balance = 0.0;
        active = true;
    }
};

void applyInterest(Account& account) {
    lock_guard<mutex> lock(account.account_mutex);
    if (!account.active) {
        return;
    }

    if (account.balance > 0) account.balance *= 1.01;
}

// Function to handle client connection
void handle_client(int client_sockfd, Account* accounts, int max_accounts, int thread_count) {
    // Create a network channel from the socket
    NetworkRequestChannel channel(client_sockfd);
    string client_address = channel.get_peer_address();
    cout << "Finance server: new client connection from " << client_address << endl;
    
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

            if (r.user_id < 0 || r.user_id >= max_accounts) {
                resp.success = false;
                resp.message = "Invalid account ID";
                channel.send_response(resp);
                continue;
            }

            // Create account if it doesn't exist
            if (!accounts[r.user_id].active) {
                // Use the initialize method instead of assignment
                accounts[r.user_id].initialize(r.user_id);
            }

            Account& acc = accounts[r.user_id];
            
            if (r.type == DEPOSIT) {
                lock_guard<mutex> lock(acc.account_mutex);
                acc.balance += r.amount;
                resp.balance = acc.balance;
                resp.message = "Deposit successful";
            } 
            else if (r.type == WITHDRAW) {
                lock_guard<mutex> lock(acc.account_mutex);
                if (acc.balance >= r.amount) {
                    acc.balance -= r.amount;
                    resp.balance = acc.balance;
                    resp.message = "Withdrawal successful";
                } else {
                    resp.success = false;
                    resp.message = "Insufficient funds";
                }
            }
            else if (r.type == BALANCE) {
                lock_guard<mutex> lock(acc.account_mutex);
                resp.balance = acc.balance;
                resp.message = "View balance successful";
            }
            else if (r.type == EARN_INTEREST) {
                try {
                    int numThreads = thread_count;
                    if (r.amount > 0) numThreads = r.amount;
                    
                    ThreadPool pool(numThreads);
                    for (int id = 0; id < max_accounts; id++) {
                        pool.enqueue([accounts, id]() {
                            applyInterest(accounts[id]);
                        });
                    }
                    resp.message = "Interest accrual successful";
                } catch (const std::exception& e) {
                    std::cerr << "Exception in EARN_INTEREST: " << e.what() << std::endl;
                    resp.success = false;
                    resp.message = std::string("Interest accrual failed: ") + e.what();
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
    
    cout << "Finance server: client " << client_address << " disconnected" << endl;
}

void print_usage() {
    cout << "Usage: ./finance_server [-p PORT] [-m MAX_ACCOUNTS] [-t THREAD_COUNT]" << endl;
    cout << "  -p, --port         Port number to listen on (default: 8000)" << endl;
    cout << "  -m, --max-accounts Maximum number of accounts (default: 100)" << endl;
    cout << "  -t, --threads      Number of threads in the thread pool (default: 4)" << endl;
    cout << "  -h, --help         Show this help message" << endl;
}

int main(int argc, char* argv[]) {
    int port = 8000;
    int max_accounts = 100;
    int thread_count = 4;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"max-accounts", required_argument, 0, 'm'},
        {"threads", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "p:m:t:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'm':
                max_accounts = atoi(optarg) + 1;
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
    SignalHandling::log_signal_event("Finance server started on port " + to_string(port));
    
    // Allocate account array
    Account* accounts = new Account[max_accounts];
    
    try {
        NetworkRequestChannel finance_channel("", port, NetworkRequestChannel::SERVER_SIDE);
        ThreadPool finance_threads(thread_count);
        cout << "Finance server listening on port " << port << endl;
        
        // Accept client connections until shutdown is requested
        while (!SignalHandling::shutdown_requested) {
            try {
                int client_fd = finance_channel.accept_connection();
                
                if (errno == EINTR) {
                    continue;
                }

                finance_threads.enqueue([client_fd, accounts, max_accounts, thread_count]() {
                    handle_client(client_fd, accounts, max_accounts, thread_count);
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
        
        cout << "Finance server shutting down..." << endl;
    }
    catch (const exception& e) {
        cerr << "Error starting finance server: " << e.what() << endl;
    }
    
    // Cleanup
    delete[] accounts;
    
    SignalHandling::log_signal_event("Finance server shutdown complete");
    return 0;
}