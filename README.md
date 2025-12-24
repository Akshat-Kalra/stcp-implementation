# Simple Transport Control Protocol (STCP) Implementation

A reliable transport protocol implementation built on top of UDP, providing TCP-like functionality including connection establishment, reliable data transfer, flow control, and graceful connection termination.

## Project Overview

This project implements a simplified version of TCP (Simple TCP or STCP) that runs over UDP. It includes:
- Three-way handshake for connection establishment
- Reliable data transfer with acknowledgments and retransmissions
- Flow control using a sliding window protocol
- Sequence number wraparound handling
- Connection teardown with FIN packets

## Files

### Core Implementation
- **`stcp.c`** / **`stcp.h`** - Main STCP protocol implementation
- **`sender.c`** - STCP sender application
- **`tcp.c`** / **`tcp.h`** - TCP packet handling utilities
- **`wraparound.c`** / **`wraparound.h`** - Sequence number wraparound utilities
- **`log.c`** / **`log.h`** - Logging functionality

### Testing Infrastructure
- **`testtcp.c`** - TCP utility tests
- **`testwraparound.c`** - Wraparound logic tests
- **`waitForPorts.c`** - Port availability checker

### Test Scripts
- **`dropsyn.script`** - Drop first SYN segment test
- **`delaysyn.script`** - Delay SYN segment test
- **`probdelayconsume.script`** - Probabilistic delay/consume test
- **`probpointtwocorrupt.script`** - 20% corruption test
- **`probpointtwonocorrupt.script`** - No corruption test
- **`test.script`** - General test script

### Shell Scripts
- **`runnoerrors.sh`** - Run tests without errors
- **`runnoerrorsbig.sh`** - Run tests with large files, no errors
- **`runallerrors.sh`** - Run tests with all error conditions
- **`runallerrorsbig.sh`** - Run tests with large files and all errors

## Building

```bash
make
```

This will:
1. Compile all source files
2. Create the `sender`, `testtcp`, `testwraparound`, and `waitForPorts` executables
3. Run the comprehensive test suite

## Usage

### Running the Sender

```bash
./sender <receiver_host> <receiver_port> <input_file>
```

Example:
```bash
./sender localhost 5555 input.txt
```

### Running Tests

Run unit tests:
```bash
./testtcp
./testwraparound
```

Run integration tests:
```bash
bash ./runnoerrors.sh        # Basic functionality
bash ./runallerrors.sh       # With packet loss/corruption
bash ./runnoerrorsbig.sh     # Large file transfers
bash ./runallerrorsbig.sh    # Large files with errors
```

## Cleaning Up

```bash
make clean
```

Removes all compiled object files, executables, and test output files.

## Features Implemented

- **Connection Management**: Three-way handshake (SYN, SYN-ACK, ACK)
- **Reliable Transfer**: Cumulative acknowledgments and retransmissions
- **Flow Control**: Sliding window protocol
- **Error Handling**: Packet loss, corruption, and out-of-order delivery
- **Connection Teardown**: Graceful close with FIN packets
- **Sequence Number Wraparound**: Proper handling of 32-bit sequence number overflow

## Academic Integrity Notice

This project was completed as part of CPSC 317 at the University of British Columbia. If you are currently taking this course, please adhere to the academic integrity policies of your institution.

## License

Adapted from course materials at Boston University for use in CPSC 317 at UBC.
