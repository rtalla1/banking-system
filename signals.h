#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include <signal.h>
#include <atomic>
#include <string>
#include <vector>
#include <sys/types.h>
#include <iostream>

namespace SignalHandling {
    // Signal flags (using std::atomic for thread safety)
    extern std::atomic<bool> shutdown_requested;
    extern std::atomic<bool> timeout_occurred;
    extern std::atomic<int> child_exited;
    
    // Server process tracking
    struct ServerProcess {
        pid_t pid;
        std::string name;
        bool active;
    };
    
    extern std::vector<ServerProcess> server_processes;
    
    // Signal handlers
    void setup_handlers();
    void sigint_handler(int sig);
    void sigalrm_handler(int sig);
    void sigchld_handler(int sig);
    
    // Signal operations
    void block_signals();
    void unblock_signals();
    bool wait_with_timeout(int seconds);
    void cancel_timeout();
    
    // Server management
    void register_server(pid_t pid, const std::string& name);
    bool is_server_active(const std::string& name);
    void print_server_status();
    
    // Logging
    void log_signal_event(const std::string& message);
}

// Helper template for executing functions with timeout
template<typename Func>
bool execute_with_timeout(Func operation, int timeout_seconds) {
    SignalHandling::timeout_occurred = false;
    
    // Set alarm
    alarm(timeout_seconds);
    
    bool result = operation();
    
    // Cancel alarm
    alarm(0);
    return result && !SignalHandling::timeout_occurred;
}

#endif