## Project Info

Created to test C++ skills, designed originally with a simple single threaded approach in mind, to eventually refactor to multithreading implementation.

Current implementation uses two objects `ServerSocket` and `ConnectionSocket`, the former of which handles binding, listening on a specified port, and accepting connections, which returns the latter object, that handles creating the randomly generated lines to feed to client expecting SSH Banner which keeps the connection alive. This was modeled from the EndleSSH source code. 

## Next Steps
- [X] Add Logger class that outputs to stdout
- [ ] Clean up main file to only import header files if project continues to grow
- [ ] Create more robust error handling, basic currently
- [ ] Optimize to single-threaded, event based architecture to stop CPU spikes
- [ ] Containerize and tweak to receive environment variables as config
