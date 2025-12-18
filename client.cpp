#include "common.h"
#include "network_channel.h"
#include "signals.h"
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <limits>
#include <getopt.h>
#include <memory>
#include <cstring>

using namespace std;
using namespace SignalHandling;

void print_menu() {
    cout << "\n=== Networked Banking System Menu ===\n"
         << "1. Login\n"
         << "2. Deposit\n"
         << "3. Withdraw\n"
         << "4. View Balance\n"
         << "5. Upload File\n"
         << "6. Download File\n"
         << "7. Logout\n"
         << "8. Server Status\n"
         << "9. Update Interest for All Accounts\n"
         << "0. Exit\n"
         << "Enter choice: ";
}

void empty_file(const string& filename) {
    ofstream clear_file(filename, ios::trunc);
    clear_file.close();
}

void clear_input() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// Retry mechanism for failed operations
template<typename Func>
void retry_operation(const string& operation_name, Func operation, int max_retries = 3) {
    int retries = 0;
    bool success = false;
    
    while (!success && retries < max_retries && !shutdown_requested) {
        if (retries > 0) {
            cout << "Retrying " << operation_name << " (attempt " << retries+1 << " of " << max_retries << ")..." << endl;
        }
        
        success = operation();
        
        if (!success && !shutdown_requested) {
            retries++;
            if (retries < max_retries) {
                char retry;
                cout << "Operation failed. Retry? (y/n): ";
                cin >> retry;
                clear_input();
                
                if (tolower(retry) != 'y') {
                    cout << "Operation canceled." << endl;
                    break;
                }
            } else {
                cout << "Maximum retry attempts reached." << endl;
            }
        }
    }
}

void print_usage() {
    cout << "Usage: ./network_client [OPTIONS]" << endl;
    cout << "  -h, --help                      Show this help message" << endl;
    cout << "  --finance-host=HOST             Finance server hostname/IP (default: localhost)" << endl;
    cout << "  --finance-port=PORT             Finance server port (default: 8000)" << endl;
    cout << "  --logging-host=HOST             Logging server hostname/IP (default: localhost)" << endl;
    cout << "  --logging-port=PORT             Logging server port (default: 8002)" << endl;
    cout << "  --file-host=HOST                File server hostname/IP (default: localhost)" << endl;
    cout << "  --file-port=PORT                File server port (default: 8001)" << endl;
    cout << "  -r, --retries=N                 Max connection retries (default: 3)" << endl;
}

int main(int argc, char* argv[]) {
    string finance_host = "localhost";
    int finance_port = 8000;
    string logging_host = "localhost";
    int logging_port = 8002;
    string file_host = "localhost";
    int file_port = 8001;
    int max_retries = 3;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"finance-host", required_argument, 0, 0},
        {"finance-port", required_argument, 0, 0},
        {"logging-host", required_argument, 0, 0},
        {"logging-port", required_argument, 0, 0},
        {"file-host", required_argument, 0, 0},
        {"file-port", required_argument, 0, 0},
        {"retries", required_argument, 0, 'r'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hr:", long_options, &option_index)) != -1) {
        switch (c) {
            case 0:
                if (string(long_options[option_index].name) == "finance-host") {
                    finance_host = optarg;
                } else if (string(long_options[option_index].name) == "finance-port") {
                    finance_port = atoi(optarg);
                } else if (string(long_options[option_index].name) == "logging-host") {
                    logging_host = optarg;
                } else if (string(long_options[option_index].name) == "logging-port") {
                    logging_port = atoi(optarg);
                } else if (string(long_options[option_index].name) == "file-host") {
                    file_host = optarg;
                } else if (string(long_options[option_index].name) == "file-port") {
                    file_port = atoi(optarg);
                }
                break;
            case 'r':
                max_retries = atoi(optarg);
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
    
    // Initialize signal handling
    SignalHandling::setup_handlers();
    SignalHandling::log_signal_event("Network client started");
    
    cout << "Connecting to servers..." << endl;
    
    // Connection pointers - using raw pointers instead of unique_ptr with make_unique
    NetworkRequestChannel* finance_channel = nullptr;
    NetworkRequestChannel* logging_channel = nullptr;
    NetworkRequestChannel* file_channel = nullptr;
    
    // Try to connect to servers
    try {
        cout << "Connected to finance server at " << finance_host << ":" << finance_port << endl;
    } catch (const exception& e) {
        cerr << "Failed to connect to finance server: " << e.what() << endl;
    }
    
    try {
        cout << "Connected to logging server at " << logging_host << ":" << logging_port << endl;
    } catch (const exception& e) {
        cerr << "Failed to connect to logging server: " << e.what() << endl;
    }
    
    try {
        cout << "Connected to file server at " << file_host << ":" << file_port << endl;
    } catch (const exception& e) {
        cerr << "Failed to connect to file server: " << e.what() << endl;
    }
    
    int current_user = -1;  // -1 means no user logged in
    bool running = true;
    
    while (running && !shutdown_requested) {
        print_menu();
        
        int choice;
        if (!(cin >> choice)) {
            // Check for shutdown after failed input
            if (SignalHandling::shutdown_requested) {
                break;  // Exit the main loop
            }
            cout << "Invalid input. Please enter a number: ";
            clear_input();
            continue;
        }
        clear_input();

        // Check for shutdown before processing
        if (shutdown_requested) {
            break;
        }

        try {
            switch (choice) {
                case 0: {  // Exit
                    running = false;
                    break;
                }
                
                case 1: {  // Login
                    if (current_user != -1) {
                        cout << "Already logged in! Please logout first.\n";
                        break;
                    }
                    
                    cout << "Enter user ID: ";
                    cin >> current_user;
                    clear_input();
                    
                    // Login operation
                    auto login_operation = [&]() {
                        if (!logging_channel) {
                            cout << "Not connected to logging server!" << endl;
                            return false;
                        }
                        
                        Request login(LOGIN, current_user);
                        Response resp;
                        
                        try {
                            resp = logging_channel->send_request(login);
                        } catch (const exception& e) {
                            cout << "Login failed: " << e.what() << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Logged in as user " << current_user << endl;
                            return true;
                        } else {
                            cout << "Login failed: " << resp.message << endl;
                            current_user = -1;
                            return false;
                        }
                    };

                    // Block signals during transaction
                    block_signals();
                    
                    retry_operation("login", login_operation, max_retries);
                    
                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 2: {  // Deposit
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    double amount;
                    cout << "Enter amount to deposit: ";
                    cin >> amount;
                    clear_input();
                    
                    // Deposit operation
                    auto deposit_operation = [&]() {
                        if (!finance_channel) {
                            cout << "Not connected to finance server!" << endl;
                            return false;
                        }
                        
                        Request txn(DEPOSIT, current_user, amount);
                        Response resp;
                        
                        try {
                            resp = finance_channel->send_request(txn);
                        } catch (const exception& e) {
                            cout << "Deposit failed: " << e.what() << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Deposit successful. New balance: " << resp.balance << endl;
                            
                            // Log the deposit
                            if (logging_channel) {
                                Request audit(DEPOSIT, current_user, amount);
                                Response log_resp = logging_channel->send_request(audit);
                                if (!log_resp.success) {
                                    cout << "Warning: Failed to log transaction" << endl;
                                }
                            } else {
                                cout << "Warning: Not connected to logging server" << endl;
                            }
                            return true;
                        } else {
                            cout << "Deposit failed: " << resp.message << endl;
                            return false;
                        }
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("deposit", deposit_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 3: {  // Withdraw
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    double amount;
                    cout << "Enter amount to withdraw: ";
                    cin >> amount;
                    clear_input();
                    
                    // Withdraw operation
                    auto withdraw_operation = [&]() {
                        if (!finance_channel) {
                            cout << "Not connected to finance server!" << endl;
                            return false;
                        }
                        
                        Request txn(WITHDRAW, current_user, amount);
                        Response resp;
                        
                        try {
                            resp = finance_channel->send_request(txn);
                        } catch (const exception& e) {
                            cout << "Withdrawal failed: " << e.what() << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Withdrawal successful. New balance: " << resp.balance << endl;
                            
                            // Log the withdrawal
                            if (logging_channel) {
                                Request audit(WITHDRAW, current_user, amount);
                                Response log_resp = logging_channel->send_request(audit);
                                if (!log_resp.success) {
                                    cout << "Warning: Failed to log transaction" << endl;
                                }
                            } else {
                                cout << "Warning: Not connected to logging server" << endl;
                            }
                            return true;
                        } else {
                            cout << "Withdrawal failed: " << resp.message << endl;
                            return false;
                        }
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("withdrawal", withdraw_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 4: {  // View Balance
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    // View balance operation
                    auto balance_operation = [&]() {
                        if (!finance_channel) {
                            cout << "Not connected to finance server!" << endl;
                            return false;
                        }
                        
                        Request txn(BALANCE, current_user);
                        Response resp;

                        try {
                            resp = finance_channel->send_request(txn);
                        } catch (const exception& e) {
                            cout << "Balance request failed: " << e.what() << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "Current balance: " << resp.balance << endl;
                            
                            // Log the balance view
                            if (logging_channel) {
                                Request audit(BALANCE, current_user, resp.balance);
                                Response log_resp = logging_channel->send_request(audit);
                                if (!log_resp.success) {
                                    cout << "Warning: Failed to log transaction" << endl;
                                }
                            } else {
                                cout << "Warning: Not connected to logging server" << endl;
                            }
                            return true;
                        } else {
                            cout << "Failed to get balance: " << resp.message << endl;
                            return false;
                        }
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("balance check", balance_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 5: {  // Upload File
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    string filename;
                    cout << "Enter filename to upload: ";
                    getline(cin, filename);
                    
                    ifstream infile(filename);
                    if (!infile) {
                        cout << "Error: Could not open file\n";
                        break;
                    }
                    
                    string content((istreambuf_iterator<char>(infile)), {});
                    infile.close();

                    // Upload file operation
                    auto upload_operation = [&]() {
                        if (!file_channel) {
                            cout << "Not connected to file server!" << endl;
                            return false;
                        }
                        
                        Request upload(UPLOAD_FILE, current_user, 0, filename, content);
                        Response resp;
                        
                        try {
                            resp = file_channel->send_request(upload);
                        } catch (const exception& e) {
                            cout << "File upload failed: " << e.what() << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            cout << "File upload successful\n";
                            
                            // Log the file upload
                            if (logging_channel) {
                                Request audit(UPLOAD_FILE, current_user, 0, filename);
                                Response log_resp = logging_channel->send_request(audit);
                                if (!log_resp.success) {
                                    cout << "Warning: Failed to log file upload" << endl;
                                }
                            } else {
                                cout << "Warning: Not connected to logging server" << endl;
                            }
                            return true;
                        } else {
                            cout << "File upload failed: " << resp.message << endl;
                            return false;
                        }
                    };

                     // Block signals during transaction
                    block_signals();

                    retry_operation("file upload", upload_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();

                    break;
                }
                
                case 6: {  // Download File
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }
                    
                    string filename;
                    cout << "Enter filename to download: ";
                    getline(cin, filename);
                    
                    // Download file operation
                    auto download_operation = [&]() {
                        if (!file_channel) {
                            cout << "Not connected to file server!" << endl;
                            return false;
                        }
                        
                        Request download(DOWNLOAD_FILE, current_user, 0, filename);
                        Response resp;
                        
                        try {
                            resp = file_channel->send_request(download);
                        } catch (const exception& e) {
                            cout << "File download failed: " << e.what() << endl;
                            return false;
                        }
                        
                        if (resp.success) {
                            ofstream outfile(filename);
                            if (!outfile) {
                                cout << "Error: Could not create output file\n";
                                return false;
                            }
                            outfile << resp.data;
                            outfile.close();
                            cout << "File downloaded successfully\n";
                            
                            // Log the file download
                            if (logging_channel) {
                                Request audit(DOWNLOAD_FILE, current_user, 0, filename);
                                Response log_resp = logging_channel->send_request(audit);
                                if (!log_resp.success) {
                                    cout << "Warning: Failed to log file download" << endl;
                                }
                            } else {
                                cout << "Warning: Not connected to logging server" << endl;
                            }
                            return true;
                        } else {
                            cout << "File download failed: " << resp.message << endl;
                            return false;
                        }
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("file download", download_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();
                    
                    break;
                }
                
                case 7: {  // Logout
                    if (current_user == -1) {
                        cout << "Not logged in!\n";
                        break;
                    }

                    // Logout operation
                    auto logout_operation = [&]() {
                        if (!logging_channel) {
                            cout << "Not connected to logging server!" << endl;
                            // Still allow logout even if logging server is down
                            current_user = -1;
                            cout << "Logged out locally\n";
                            return true;
                        }
                        
                        Request logout(LOGOUT, current_user);
                        Response resp;
                        
                        try {
                            resp = logging_channel->send_request(logout);
                        } catch (const exception& e) {
                            cout << "Logout from server failed: " << e.what() << endl;
                            // Still logout locally
                            current_user = -1;
                            cout << "Logged out locally\n";
                            return true;
                        }
                        
                        current_user = -1;
                        cout << "Logged out successfully\n";
                        return true;
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("logout", logout_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();

                    break;
                }
                
                case 8: { 
                    SignalHandling::print_server_status();
                    break;
                }
                
                case 9: {  // Accrue Interest
                    if (current_user == -1) {
                        cout << "Please login first!\n";
                        break;
                    }

                    int numThreads = 2;
                    cout << "Input a number of threads to use: ";
                    cin >> numThreads;
                    clear_input();

                    auto interest_operation = [&]() {
                        if (!finance_channel) {
                            cout << "Not connected to finance server!" << endl;
                            return false;
                        }
                        
                        Request request(EARN_INTEREST, current_user, numThreads);
                        Response resp;
                        
                        try {
                            resp = finance_channel->send_request(request);
                        } catch (const exception& e) {
                            cout << "Interest update failed: " << e.what() << endl;
                            return false;
                        }

                        if (!resp.success) {
                            cout << "Interest update failed: " << resp.message << endl;
                            return false;
                        } else {
                            cout << "Interest update successful!" << endl;
                                
                            if (logging_channel) {
                                Response log_resp = logging_channel->send_request(request);
                                if (!log_resp.success) {
                                    cout << "Warning: Failed to log transaction" << endl;
                                }
                            } else {
                                cout << "Warning: Not connected to logging server" << endl;
                            }
                            return true;
                        }
                    };

                    // Block signals during transaction
                    block_signals();

                    retry_operation("interest update", interest_operation, max_retries);

                    // Unblock signals after transaction
                    unblock_signals();

                    break;
                }
                
                default:
                    cout << "Invalid choice. Please try again.\n";
            }
        } catch (const exception& e) {
            cout << "Error during operation: " << e.what() << endl;
            log_signal_event("Exception caught: " + string(e.what()));
        }
    }

    // Graceful shutdown
    if (shutdown_requested) {
        cout << "\nPerforming graceful shutdown..." << endl;
        log_signal_event("Beginning graceful shutdown");
    } else {
        cout << "Exiting normally..." << endl;
        log_signal_event("Normal exit requested");
    }

    // Send QUIT to all connected servers
    cout << "Sending shutdown signals to connected servers..." << endl;
    
    Request quit(QUIT);
    
    if (finance_channel) {
        try {
            finance_channel->send_request(quit);
            cout << "QUIT sent to finance server" << endl;
        } catch (const exception& e) {
            cerr << "Failed to send QUIT to finance server: " << e.what() << endl;
        }
    }
    
    if (file_channel) {
        try {
            file_channel->send_request(quit);
            cout << "QUIT sent to file server" << endl;
        } catch (const exception& e) {
            cerr << "Failed to send QUIT to file server: " << e.what() << endl;
        }
    }
    
    if (logging_channel) {
        try {
            logging_channel->send_request(quit);
            cout << "QUIT sent to logging server" << endl;
        } catch (const exception& e) {
            cerr << "Failed to send QUIT to logging server: " << e.what() << endl;
        }
    }
    
    // Clean up resources
    delete finance_channel;
    delete logging_channel;
    delete file_channel;
    
    log_signal_event("Network client shutdown complete");
    cout << "Shutdown complete.\n";
    
    return 0;
}