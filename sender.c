/************************************************************************
 * Adapted from a course at Boston University for use in CPSC 317 at UBC
 *
 *
 * The interfaces for the STCP sender (you get to implement them), and a
 * simple application-level routine to drive the sender.
 *
 * This routine reads the data to be transferred over the connection
 * from a file specified and invokes the STCP send functionality to
 * deliver the packets as an ordered sequence of datagrams.
 *
 * Version 2.0
 *
 *
 *************************************************************************/


#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/file.h>

#include "stcp.h"

#define STCP_SUCCESS 1
#define STCP_ERROR -1

typedef struct {
    
    /* YOUR CODE HERE */
    int fd;
    int state;
    unsigned int isn;
    // unsigned int next_seq_num;
    // unsigned int last_ack_num;
    unsigned short window_size;

    



} stcp_send_ctrl_blk;
/* ADD ANY EXTRA FUNCTIONS HERE */

/*
 * Send STCP. This routine is to send all the data (len bytes).  If more
 * than MSS bytes are to be sent, the routine breaks the data into multiple
 * packets. It will keep sending data until the send window is full or all
 * the data has been sent. At which point it reads data from the network to,
 * hopefully, get the ACKs that open the window. You will need to be careful
 * about timing your packets and dealing with the last piece of data.
 *
 * Your sender program will spend almost all of its time in either this
 * function or in tcp_close().  All input processing (you can use the
 * function readWithTimeout() defined in stcp.c to receive segments) is done
 * as a side effect of the work of this function (and stcp_close()).
 *
 * The function returns STCP_SUCCESS on success, or STCP_ERROR on error.
 */
int stcp_send(stcp_send_ctrl_blk *stcp_CB, unsigned char* data, int length) {

    /* YOUR CODE HERE */
    int bytes_sent = 0;
    int window_size = stcp_CB->window_size;
    int unacked_bytes = 0;
    int next_seq_num = stcp_CB->isn + 1;

    while (bytes_sent < length || unacked_bytes > 0) {

        while (bytes_sent < length && unacked_bytes < window_size) {
            int chunk_size = min(STCP_MSS, window_size - unacked_bytes);
            packet data_packet;
            htonHdr(data_packet.hdr);
            createSegment(&data_packet, ACK, window_size, next_seq_num, 0, data + bytes_sent, chunk_size);
            data_packet.hdr->checksum = ipchecksum(data_packet.data, sizeof(tcpheader) + chunk_size);
            logLog("segment", "Sending data packet");
            dump('s', data, length);
            send(stcp_CB->fd, data_packet.data, data_packet.len, 0);
            bytes_sent += chunk_size;
            next_seq_num += chunk_size;
            unacked_bytes += chunk_size;
        }


        packet ack_packet;
        int ack_length = readWithTimeout(stcp_CB->fd, ack_packet.data, STCP_INITIAL_TIMEOUT);
        if (ack_length < 0) {
            logLog("error", "Timeout waiting for ACK packet");
        } else {
            logLog("segment", "Received ACK packet blah blah");
        }

        unacked_bytes -= ack_packet.hdr->ackNo - next_seq_num;
        next_seq_num = ack_packet.hdr->ackNo;

    }

    return STCP_SUCCESS;
}



/*
 * Open the sender side of the STCP connection. Returns the pointer to
 * a newly allocated control block containing the basic information
 * about the connection. Returns NULL if an error happened.
 *
 * If you use udp_open() it will use connect() on the UDP socket
 * then all packets then sent and received on the given file
 * descriptor go to and are received from the specified host. Reads
 * and writes are still completed in a datagram unit size, but the
 * application does not have to do the multiplexing and
 * demultiplexing. This greatly simplifies things but restricts the
 * number of "connections" to the number of file descriptors and isn't
 * very good for a pure request response protocol like DNS where there
 * is no long term relationship between the client and server.
 */
stcp_send_ctrl_blk * stcp_open(char *destination, int sendersPort,
                             int receiversPort) {

    logLog("init", "Sending from port %d to <%s, %d>", sendersPort, destination, receiversPort);
    // Since I am the sender, the destination and receiversPort name the other side
    int fd = udp_open(destination, receiversPort, sendersPort);
    // (void) fd;
    /* YOUR CODE HERE */

    // for reference:
        // #define STCP_SENDER_CLOSED 0
        // #define STCP_SENDER_SYN_SENT 1
        // #define STCP_SENDER_ESTABLISHED 2
        // #define STCP_SENDER_CLOSING 3
        // #define STCP_SENDER_FIN_WAIT 4


    stcp_send_ctrl_blk *cb = (stcp_send_ctrl_blk *) malloc(sizeof(stcp_send_ctrl_blk));
    // TODO: handle malloc error


    cb->fd = fd;
    cb->state = STCP_SENDER_CLOSED;

    unsigned int isn = rand();
    cb->isn = isn;
    packet syn_packet;
    createSegment(&syn_packet, SYN, STCP_MAXWIN, isn, 0, NULL, 0);
    htonHdr(syn_packet.hdr);

    

    syn_packet.hdr->checksum = ipchecksum(syn_packet.data, sizeof(tcpheader));
    logLog("segment", "Sending SYN packet");

    
    send(cb->fd, syn_packet.data, syn_packet.len, 0);

    cb->state = STCP_SENDER_SYN_SENT;

    packet ack_packet;
    int ack_length = readWithTimeout(cb->fd, ack_packet.data, STCP_INITIAL_TIMEOUT);
    if (ack_length < 0) {
        logLog("error", "Timeout waiting for ACK packet");
    } else {
        logLog("segment", "Connection Established: Received ACK packet");
    }

    cb->window_size = ack_packet.hdr->windowSize;
    logLog("init", "Connection established with window size %d", cb->window_size);
    cb->state = STCP_SENDER_ESTABLISHED;


    return cb;



    // return NULL;
}


/*
 * Make sure all the outstanding data has been transmitted and
 * acknowledged, and then initiate closing the connection. This
 * function is also responsible for freeing and closing all necessary
 * structures that were not previously freed, including the control
 * block itself.
 *
 * Returns STCP_SUCCESS on success or STCP_ERROR on error.
 */
int stcp_close(stcp_send_ctrl_blk *cb) {
    /* YOUR CODE HERE */
    return STCP_SUCCESS;
}
/*
 * Return a port number based on the uid of the caller.  This will
 * with reasonably high probability return a port number different from
 * that chosen for other uses on the undergraduate Linux systems.
 *
 * This port is used if ports are not specified on the command line.
 */
int getDefaultPort() {
    uid_t uid = getuid();
    int port = (uid % (32768 - 512) * 2) + 1024;
    assert(port >= 1024 && port <= 65535 - 1);
    return port;
}

/*
 * This application is to invoke the send-side functionality.
 */
int main(int argc, char **argv) {
    stcp_send_ctrl_blk *cb;

    char *destinationHost;
    int receiversPort, sendersPort;
    char *filename = NULL;
    int file;
    /* You might want to change the size of this buffer to test how your
     * code deals with different packet sizes.
     */
    unsigned char buffer[STCP_MSS];
    int num_read_bytes;

    logConfig("sender", "init,segment,error,failure");
    /* Verify that the arguments are right */
    if (argc > 5 || argc == 1) {
        fprintf(stderr, "usage: sender DestinationIPAddress/Name receiveDataOnPort sendDataToPort filename\n");
        fprintf(stderr, "or   : sender filename\n");
        exit(1);
    }
    if (argc == 2) {
        filename = argv[1];
        argc--;
    }

    // Extract the arguments
    destinationHost = argc > 1 ? argv[1] : "localhost";
    receiversPort = argc > 2 ? atoi(argv[2]) : getDefaultPort();
    sendersPort = argc > 3 ? atoi(argv[3]) : getDefaultPort() + 1;
    if (argc > 4) filename = argv[4];

    /* Open file for transfer */
    file = open(filename, O_RDONLY);
    if (file < 0) {
        logPerror(filename);
        exit(1);
    }

    /*
     * Open connection to destination.  If stcp_open succeeds the
     * control block should be correctly initialized.
     */
    cb = stcp_open(destinationHost, sendersPort, receiversPort);
    if (cb == NULL) {
        /* YOUR CODE HERE */
    }

    /* Start to send data in file via STCP to remote receiver. Chop up
     * the file into pieces as large as max packet size and transmit
     * those pieces.
     */
    while (1) {
        num_read_bytes = read(file, buffer, sizeof(buffer));

        /* Break when EOF is reached */
        if (num_read_bytes <= 0)
            break;

        if (stcp_send(cb, buffer, num_read_bytes) == STCP_ERROR) {
            /* YOUR CODE HERE */
        }
    }

    /* Close the connection to remote receiver */
    if (stcp_close(cb) == STCP_ERROR) {
        /* YOUR CODE HERE */
    }

    return 0;
}
