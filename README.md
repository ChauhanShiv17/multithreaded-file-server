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
