#include "signals.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctime>
#include <algorithm>
#include <sstream>

using namespace std;

namespace SignalHandling {
    // Initialize atomic flags
    std::atomic<bool> shutdown_requested(false);
    std::atomic<bool> timeout_occurred(false);
    std::atomic<int> child_exited(0);
    
    // Server process registry
    std::vector<ServerProcess> server_processes;
    
    void setup_handlers() {
        
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);

        // Set up SIGALRM handler
        sa.sa_handler = sigalrm_handler;
        sa.sa_flags = 0;
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            perror("Failed to set SIGALRM handler");
            exit(1);
        }
        
        // Set up SIGINT handler
        sa.sa_handler = sigint_handler;
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("Failed to set SIGINT handler");
            exit(1);
        }
        
        // Set up SIGCHLD handler
        sa.sa_handler = sigchld_handler;
        sa.sa_flags = SA_RESTART; // Restart interrupted system calls
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror("Failed to set SIGCHLD handler");
            exit(1);
        }
        
        log_signal_event("Signal handlers initialized");
    }
    
    void sigint_handler(int sig) {
        if (!shutdown_requested) {
            shutdown_requested = true; // Explicit atomic store
            log_signal_event("SIGINT received - initiating graceful shutdown");
            
            // We can't use cout here directly as it's not async-signal-safe
            // The '\n' helps ensure the message appears
            write(STDOUT_FILENO, "\nShutdown requested. Completing current operation...\n", 52);
        } else {
            // Second SIGINT - force exit
            log_signal_event("Second SIGINT received - forcing exit");
            write(STDOUT_FILENO, "\nForced exit. Terminating immediately.\n", 39);
            exit(1);
        }
    }
    
    void sigalrm_handler(int sig) {
        timeout_occurred = true;
        write(STDOUT_FILENO, "SIGALRM fired!\n", 15);
        log_signal_event("SIGALRM received - operation timed out");
    }
    
    void sigchld_handler(int sig) {
        int status;
        pid_t pid;
        
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            child_exited++;
            
            // Update server registry
            for (auto& server : server_processes) {
                if (server.pid == pid) {
                    server.active = false;
                    
                    std::stringstream ss;
                    ss << "Child process terminated: " << server.name << " (PID: " << pid << ")";
                    log_signal_event(ss.str());
                    
                    break;
                }
            }
        }
    }
    
    void block_signals() {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        
        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
            perror("Failed to block signals");
        } else {
            log_signal_event("Signals blocked for critical section");
        }
    }
    
    void unblock_signals() {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        
        if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
            perror("Failed to unblock signals");
        } else {
            log_signal_event("Signals unblocked");
        }
    }
    
    bool wait_with_timeout(int seconds) {
        timeout_occurred = false;
        alarm(seconds);
        return true;
    }
    
    void cancel_timeout() {
        alarm(0);
    }
    
    void register_server(pid_t pid, const std::string& name) {
        server_processes.push_back({pid, name, true});
        
        std::stringstream ss;
        ss << "Registered server: " << name << " (PID: " << pid << ")";
        log_signal_event(ss.str());
    }
    
    bool is_server_active(const std::string& name) {
        auto it = std::find_if(server_processes.begin(), server_processes.end(),
                              [&name](const ServerProcess& p) { return p.name == name; });
        
        return (it != server_processes.end()) && it->active;
    }
    
    void print_server_status() {
        std::cout << "\n=== Server Status ===\n";
        for (const auto& server : server_processes) {
            std::cout << server.name << " (PID: " << server.pid << "): "
                      << (server.active ? "ACTIVE" : "TERMINATED") << std::endl;
        }
        std::cout << "====================\n";
    }
    
    void log_signal_event(const std::string& message) {
        // Get timestamp
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        // Format log entry
        std::string log_entry = std::string(timestamp) + " - " + message + "\n";
        
        // Write to log file (signal-safe)
        int fd = open("signals.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            write(fd, log_entry.c_str(), log_entry.size());
            close(fd);
        }
    }
}