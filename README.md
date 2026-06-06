# Multithreaded File Server

## Project Overview
This project implements a simple POSIX TCP file server in C++17. The server listens on port `8080`, accepts multiple client connections concurrently using `std::thread`, and serves file contents from a secure local `files/` directory.

## Features
- TCP socket server using Linux/POSIX sockets
- Multithreaded client handling with `std::thread`
- File request handling from `files/` folder
- Safe filename validation to prevent path traversal attacks
- Mutex-based logging to `logs.txt`
- Clean error handling and user-friendly responses
- Sample files included for testing

## Architecture
- `server.cpp` contains the server logic.
  - Listens on port `8080`
  - Accepts multiple clients
  - Validates client file requests
  - Sends file contents or error messages
  - Writes request logs with thread-safe access
- `client.cpp` contains the client logic.
  - Connects to `localhost:8080`
  - Sends a filename request
  - Receives either a file payload or an error message
  - Stores received files as `downloaded_<filename>`
- `files/` contains sample files for the server to serve.
- `logs.txt` captures request and error logs.

## Setup Steps
1. Install a C++17-compatible compiler on Linux.
2. Install `git` and GitHub CLI (`gh`).
3. Open the terminal in the project root.

## Compile Commands
```bash
g++ -std=c++17 -pthread server.cpp -o server
g++ -std=c++17 -pthread client.cpp -o client
```

## Run Commands
1. Start the server:
```bash
./server
```
2. Run the client:
```bash
./client example.txt
```
or
```bash
./client
```
Then enter a filename when prompted.

## Interview Explanation
This project demonstrates knowledge of:
- POSIX socket programming in C++
- Concurrency with `std::thread`
- File I/O and binary transfer
- Secure input validation for path traversal mitigation
- Logging using `std::mutex` to avoid race conditions
- Robust error handling for network and file operations

## Possible Interview Questions
1. How does your server handle multiple clients at the same time?
2. Why is path traversal a security concern and how did you prevent it?
3. What is the purpose of `SO_REUSEADDR` in the server socket?
4. Why is `std::mutex` needed for logging in `logs.txt`?
5. How does the client know when the file transfer is complete?
6. How would you extend this server to support directories or file listings?

## Notes
- The server only serves files inside the `files/` directory.
- Received files are saved locally as `downloaded_<filename>`.
- The server responds with a clear `ERROR:` message on failure.
