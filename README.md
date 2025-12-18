# Networked Banking System

A C++ client-server banking system built on TCP sockets with multithreading, signal handling, and structured request/response messaging. The system is split into three independent servers and one interactive client.

## Components

- **Client**
  - Interactive CLI for users
  - Connects to all servers
  - Supports retries, graceful shutdown, and signal-safe transactions

- **Finance Server**
  - Manages bank accounts
  - Supports deposit, withdraw, balance checks
  - Accrues interest across all accounts using a thread pool

- **File Server**
  - Handles file upload and download
  - Optional file-extension allowlist
  - Stores files in a local `storage/` directory

- **Logging Server**
  - Records all user actions to a log file
  - Thread-safe file writes

## Features

- TCP-based request/response protocol
- Length-prefixed wire format
- Thread pools for concurrency
- Mutex-protected shared state
- Graceful shutdown via signals (SIGINT, SIGCHLD, SIGALRM)
- Retry logic for failed client operations

## Request Types

- LOGIN
- LOGOUT
- DEPOSIT
- WITHDRAW
- BALANCE
- EARN_INTEREST
- UPLOAD_FILE
- DOWNLOAD_FILE
- QUIT

## Build

Requires:
- g++ (C++11)
- pthreads
- Linux or Unix-like OS

Build everything:

```bash
make
```

Clean build artifacts:

```bash
make clean
```

## Run Order

Start each server in its own terminal before launching the client.

### Finance Server

```bash
./finance [-p PORT] [-m MAX_ACCOUNTS] [-t THREADS]
```

Defaults:
- Port: 8000
- Max accounts: 100
- Threads: 4

### File Server

```bash
./file [-p PORT] [-t THREADS] [ALLOWED_EXTENSIONS...]
```

Defaults:
- Port: 8001
- Threads: 4
- If no extensions are provided, all are allowed

### Logging Server

```bash
./logging [-p PORT] [-f LOG_FILE] [-t THREADS]
```

Defaults:
- Port: 8002
- Log file: system.log
- Threads: 4

### Client

```bash
./client [OPTIONS]
```

Options:
- `--finance-host HOST`
- `--finance-port PORT`
- `--file-host HOST`
- `--file-port PORT`
- `--logging-host HOST`
- `--logging-port PORT`
- `-r`, `--retries N`
- `-h`, `--help`

Defaults connect to localhost on ports 8000, 8001, and 8002.

## Client Menu

- Login
- Deposit
- Withdraw
- View balance
- Upload file
- Download file
- Logout
- Server status
- Accrue interest
- Exit

## Protocol

**Request format**

```
TYPE|USER_ID|AMOUNT|FILENAME|DATA
```

**Response format**

```
SUCCESS|BALANCE|DATA|MESSAGE
```

Messages use a 4-byte length prefix (network byte order).

## Signals

- SIGINT: graceful shutdown (second SIGINT forces exit)
- SIGCHLD: server process tracking
- SIGALRM: timeout support

Signal events are logged to `signals.log`.

## Notes

- Accounts are created lazily on first use
- Interest accrual applies 1% to all positive balances
- File storage is local and not sandboxed
- Designed for instructional and architectural purposes