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

// hello
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
    unsigned int next_seq_num;
    unsigned int last_ack_num;
    unsigned short window_size;

} stcp_send_ctrl_blk;

typedef struct packet_node {
    packet pkt;
    unsigned int seq;
    int retransmission_count;
    unsigned long sent_time;
    struct packet_node *next;
} packet_node;

packet_node *outstanding_head = NULL;
/* ADD ANY EXTRA FUNCTIONS HERE */

unsigned long get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000UL + tv.tv_usec / 1000UL;
}

void addOutstanding(packet_node **head, const packet *pkt, unsigned int seq, unsigned long sent_time) {
    packet_node *new_node = malloc(sizeof(packet_node));
    if (new_node == NULL) {
        perror("malloc");
        return;
    }
    new_node->pkt = *pkt;
    new_node->seq = seq;
    new_node->sent_time = sent_time;
    new_node->retransmission_count = 0;
    new_node->next = NULL;
    
    if (*head == NULL) {
        *head = new_node;
    } else {
        packet_node *current = *head;
        while (current->next != NULL)
            current = current->next;
        current->next = new_node;
}
}


void removeOutstanding(packet_node **head, unsigned int ack) {
    while (*head != NULL && (*head)->seq < ack) {
        packet_node *temp = *head;
        *head = (*head)->next;
        free(temp);
    }
    
    packet_node *current = *head;
    while (current != NULL && current->next != NULL) {
        if (current->next->seq < ack) {
            packet_node *temp = current->next;
            current->next = current->next->next;
            free(temp);
        } else {
            current = current->next;
        }
    }
}

packet_node *findPacketNode(packet_node *head, unsigned int seq) {
    while (head != NULL) {
        if (head->seq == seq)
            return head;
        head = head->next;
    }
    return NULL;
}

void checkAndRetransmit(packet_node **head, int fd) {
    unsigned long now = get_current_time();
    packet_node *node = *head;
    while (node != NULL) {
        int timeout;

        if (node->retransmission_count == 0)
            timeout = 1000;
        else if (node->retransmission_count == 1)
            timeout = 2000;
        else
            timeout = 4000;
        
        if (now - node->sent_time >= timeout) {
            
            logLog("segment", "Retransmitting data packet");
            send(fd, node->pkt.data, node->pkt.len, 0);
            
            node->sent_time = now;
            node->retransmission_count++;
        }
        node = node->next;
    }
}

void freeOutstandingList(packet_node **head) {
    while (*head != NULL) {
        packet_node *temp = *head;
        *head = (*head)->next;
        free(temp);
    }
}

void createDataSegment(packet *pkt, int flags, unsigned short rwnd, unsigned int seq, unsigned int ack, unsigned char *data, int len) {
    createSegment(pkt, flags, rwnd, seq, ack, data, len);
    memcpy(pkt->data+sizeof(tcpheader), data, len);
}

int verifyPacketIntegrity(packet *pkt, int len) {
    
    unsigned short original_checksum = pkt->hdr->checksum;

    pkt->hdr->checksum = 0;

    unsigned short calculated_checksum = ipchecksum(pkt->data, len);

    pkt->hdr->checksum = original_checksum;
    return (original_checksum == calculated_checksum);
}

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
    // int local_unacked_bytes = 0;


    // while there is still data to send
    while (bytes_sent < length) {

        int local_unacked_bytes = 0;

        // while there is still data to send and the window is not full
        while (bytes_sent < length && local_unacked_bytes < stcp_CB->window_size) {

            int chunk_size = min(STCP_MSS, length - bytes_sent);
            packet data_packet;
            createDataSegment(&data_packet, ACK, stcp_CB->window_size, stcp_CB->next_seq_num, stcp_CB->last_ack_num + 1, data + bytes_sent, chunk_size);
            htonHdr(data_packet.hdr);

            data_packet.hdr->checksum = ipchecksum(data_packet.data, sizeof(tcpheader) + chunk_size);

            logLog("segment", "Sending data packet");

            dump('s', data_packet.data, data_packet.len);

            if (send(stcp_CB->fd, data_packet.data, data_packet.len, 0) < 0) {
                logPerror("send");
                return STCP_ERROR;
            }

            addOutstanding(&outstanding_head, &data_packet, stcp_CB->next_seq_num, get_current_time());
            bytes_sent += chunk_size;
            stcp_CB->next_seq_num += chunk_size;
            local_unacked_bytes += chunk_size;
        }



        packet ack_packet;
        // initialize ack_packet
        initPacket(&ack_packet, NULL, STCP_MTU);
        // reading ack_packet
        int ack_length = readWithTimeout(stcp_CB->fd, ack_packet.data, STCP_INITIAL_TIMEOUT); 
        if (ack_length > 0) {
            if (!verifyPacketIntegrity(&ack_packet, ack_length)) {
                logLog("error", "Checksum mismatch in ACK packet");
                continue;
            } else {
            ntohHdr(ack_packet.hdr);
            logLog("segment", "Received ACK packet blah blah");
            dump('r', ack_packet.data, ack_length);
            
            
            unsigned int received_ack = ack_packet.hdr->ackNo;
            int newly_acked = received_ack - stcp_CB->last_ack_num;
            stcp_CB->last_ack_num = received_ack;
            local_unacked_bytes -= newly_acked;

            removeOutstanding(&outstanding_head, received_ack);
            }

        } else if (ack_length == STCP_READ_TIMED_OUT) {
            logLog("error", "Timeout waiting for ACK packet");
            checkAndRetransmit(&outstanding_head, stcp_CB->fd);
        } else {
            logPerror("read");
            return STCP_ERROR;
            
        }


        int last_duplicate_ack = 0;
        int duplicate_ack_count = 0;

        while ((ack_length = readWithTimeout(stcp_CB->fd, ack_packet.data, 0)) > 0) {
            if (!verifyPacketIntegrity(&ack_packet, ack_length)) {
                logLog("error", "Checksum mismatch in ACK packet");
                continue;
            }
            ntohHdr(ack_packet.hdr);
            logLog("segment", "Draining ACK packet");
            dump('r', ack_packet.data, ack_length);
            unsigned int received_ack = ack_packet.hdr->ackNo;

            if (received_ack == last_duplicate_ack) {
                duplicate_ack_count++;
            } else {
                last_duplicate_ack = received_ack;
                duplicate_ack_count = 1;
            }

            if (duplicate_ack_count == 3) {
                logLog("segment", "Fast retransmission triggered for seq: %u", received_ack);
                int temp_ack_length;
                while ((temp_ack_length = readWithTimeout(stcp_CB->fd, ack_packet.data, 0)) > 0) {
                    ntohHdr(ack_packet.hdr);
                    logLog("segment", "Draining additional ACK during fast retransmission, ackNo: %u", ack_packet.hdr->ackNo);
                    dump('r', ack_packet.data, temp_ack_length);
                    
                }
                
                packet_node *node = findPacketNode(outstanding_head, received_ack);
                if (node != NULL) {
                    logLog("segment", "Fast retransmitting packet with seq: %u", node->seq);
                    send(stcp_CB->fd, node->pkt.data, node->pkt.len, 0);
                    node->sent_time = get_current_time();
                    node->retransmission_count++;
                }
                
                duplicate_ack_count = 0;
            }


            int newly_acked = received_ack - stcp_CB->last_ack_num;
            stcp_CB->last_ack_num = received_ack;
            local_unacked_bytes -= newly_acked;
            removeOutstanding(&outstanding_head, received_ack);
            //hello
        }

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
    if (fd < 0) {
        logLog("error", "Failed to open UDP connection");
        return NULL;
    }
    // (void) fd;
    /* YOUR CODE HERE */

    // for reference:
        // #define STCP_SENDER_CLOSED 0
        // #define STCP_SENDER_SYN_SENT 1
        // #define STCP_SENDER_ESTABLISHED 2
        // #define STCP_SENDER_CLOSING 3
        // #define STCP_SENDER_FIN_WAIT 4


    stcp_send_ctrl_blk *cb = (stcp_send_ctrl_blk *) malloc(sizeof(stcp_send_ctrl_blk));
    if (cb == NULL) {
        logPerror("malloc");
        return NULL;
    }
    // TODO: handle malloc error


    cb->fd = fd;
    cb->state = STCP_SENDER_CLOSED;
    cb->isn = rand();
    cb->next_seq_num = cb->isn + 1;
    cb->window_size = STCP_MAXWIN;
    
    
    packet syn_packet;
    createSegment(&syn_packet, SYN, STCP_MAXWIN, cb->isn, 0, NULL, 0);
    htonHdr(syn_packet.hdr);

    

    syn_packet.hdr->checksum = ipchecksum(syn_packet.data, sizeof(tcpheader));
    logLog("segment", "Sending SYN packet");

    
    if (send(cb->fd, syn_packet.data, syn_packet.len, 0) < 0) {
        logPerror("send");
        return NULL;
    }

    addOutstanding(&outstanding_head, &syn_packet, cb->isn, get_current_time());

    cb->state = STCP_SENDER_SYN_SENT;

    packet ack_packet;
    initPacket(&ack_packet, NULL, STCP_MTU);

    int ack_length = 0;
    // if (ack_length > 0) {
    //     ntohHdr(ack_packet.hdr);
    //     logLog("segment", "Connection Established: Received ACK packet");
    //     dump('r', ack_packet.data, ack_length);
    //     cb->window_size = ack_packet.hdr->windowSize;
    //     cb->last_ack_num = ack_packet.hdr->seqNo;
    // } else if (ack_length == STCP_READ_TIMED_OUT) {
    //     logLog("error", "Timeout waiting for ACK packet");
    //     checkAndRetransmit(&outstanding_head, cb->fd);
    // } else {
    //     logPerror("read");
    //     return NULL;
    // }

    while (1) {
        ack_length = readWithTimeout(cb->fd, ack_packet.data, STCP_INITIAL_TIMEOUT);
        if (ack_length < 0) {
            if (ack_length == STCP_READ_PERMANENT_FAILURE) {
                logLog("error", "Permanent failure reading ACK packet");
                return NULL;
            }
            
            logLog("error", "Timeout waiting for ACK packet");
            logLog("segment", "Retransmitting SYN packet");
            
            checkAndRetransmit(&outstanding_head, cb->fd);
        } else {

            unsigned short original_checksum = ack_packet.hdr->checksum;
            
            ack_packet.hdr->checksum = 0;
            
            unsigned short calculated_checksum = ipchecksum(ack_packet.data, ack_length);
            
            if (original_checksum != calculated_checksum) {
                logLog("error", "Checksum mismatch; Ignoring ACK packet (original %u, calculated %u)", 
                    original_checksum, calculated_checksum);
                continue;
            }    
            
        
            ntohHdr(ack_packet.hdr);    
            logLog("segment", "Connection Established: Received ACK packet");
            dump('r', ack_packet.data, ack_length);
            cb->window_size = ack_packet.hdr->windowSize;
            cb->last_ack_num = ack_packet.hdr->seqNo;
            break;

        }
    }
    // while ((ack_length = readWithTimeout(cb->fd, ack_packet.data, STCP_INITIAL_TIMEOUT)) < 0) {

    //     if (ack_length == STCP_READ_PERMANENT_FAILURE) {
    //         logLog("error", "Permanent failure reading ACK packet");
    //         return NULL;
    //     }
        
    //     logLog("error", "Timeout waiting for ACK packet");
    //     logLog("segment", "Retransmitting SYN packet");
        
    //     checkAndRetransmit(&outstanding_head, cb->fd);
        
    // }

    // if (ack_packet.hdr->checksum != ipchecksum(ack_packet.data, sizeof(tcpheader))) {
    //     logLog("error", "Checksum mismatch; Ignoring ACK packet");
     
    // } else {
    //     ntohHdr(ack_packet.hdr);
    //     logLog("segment", "Connection Established: Received ACK packet");
    //     dump('r', ack_packet.data, ack_length);
    //     cb->window_size = ack_packet.hdr->windowSize;
    //     cb->last_ack_num = ack_packet.hdr->seqNo;
    // }
    

    //three way handshake
    packet ack_packet2;
    initPacket(&ack_packet2, NULL, STCP_MTU);
    createSegment(&ack_packet2, ACK, cb->window_size, cb->next_seq_num, cb->last_ack_num + 1, NULL, 0);
    htonHdr(ack_packet2.hdr);
    ack_packet2.hdr->checksum = ipchecksum(ack_packet2.data, sizeof(tcpheader));
    logLog("segment", "Sending ACK packet (3-way handshake)");
    dump('s', ack_packet2.data, ack_packet2.len);
    if (send(cb->fd, ack_packet2.data, ack_packet2.len, 0) < 0) {
        logPerror("send");
        return NULL;
    }


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

    packet fin_packet;
    createSegment(&fin_packet, FIN, cb->window_size, cb->next_seq_num, cb->last_ack_num + 1, NULL, 0);
    htonHdr(fin_packet.hdr);
    fin_packet.hdr->checksum = ipchecksum(fin_packet.data, sizeof(tcpheader));
    logLog("segment", "Sending FIN packet");
    dump('s', fin_packet.data, fin_packet.len);
    if (send(cb->fd, fin_packet.data, fin_packet.len, 0) < 0) {
        logPerror("send");
        return STCP_ERROR;
    }
    addOutstanding(&outstanding_head, &fin_packet, cb->next_seq_num, get_current_time());

    cb->state = STCP_SENDER_CLOSING;

    packet ack_packet;
    initPacket(&ack_packet, NULL, STCP_MTU);
    // int ack_length = readWithTimeout(cb->fd, ack_packet.data, STCP_INITIAL_TIMEOUT);
    int ack_length;
    while ((ack_length = readWithTimeout(cb->fd, ack_packet.data, STCP_INITIAL_TIMEOUT)) < 0) {
        if (ack_length == STCP_READ_PERMANENT_FAILURE) {
            logLog("error", "Permanent failure reading ACK packet");
            return STCP_ERROR;
        }
        
        logLog("error", "Timeout waiting for ACK packet");
        logLog("segment", "Retransmitting FIN packet");
        
        checkAndRetransmit(&outstanding_head, cb->fd);
    }

    ntohHdr(ack_packet.hdr);
    logLog("segment", "Received ACK packet");
    dump('r', ack_packet.data, ack_length);
    cb->window_size = ack_packet.hdr->windowSize;
    cb->last_ack_num = ack_packet.hdr->seqNo;


    cb->state = STCP_SENDER_CLOSED;

    // if (ack_length < 0) {
    //     logLog("error", "Timeout waiting for ACK packet");
    // } else {
    //     ntohHdr(ack_packet.hdr);
    //     logLog("segment", "Received ACK packet");
    //     dump('r', ack_packet.data, ack_length);
    //     cb->window_size = ack_packet.hdr->windowSize;
    //     cb->last_ack_num = ack_packet.hdr->seqNo;
    // }
    
    
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