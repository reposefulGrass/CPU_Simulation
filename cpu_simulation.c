
#include <stdio.h>
#include <unistd.h> 
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define message_t           short

// programs
void    computer_system    (int pipe_cpu_bus[],  int pipe_bus_cpu[], int length);
void    system_bus         (int pipe_cpu_bus[],  int pipe_bus_cpu[], char *filename);
void    io_device          (int pipe_io_bus[],   int pipe_bus_io[],  char *filename);
void    transfer_device    (int pipe_tran_bus[], int pipe_bus_trans[]);
void    buffer             (int pipe_buf_bus[],  int pipe_bus_buf[]);

// piping utilities
message_t       receive_from_pipe   (int p1[], int p2[]);

// message utilities
message_t       create_message      (int is, int cd, int halt, int id, int data);
message_t       convert_to_message  (unsigned char buf[2]);
void            write_message       (int pipe[], message_t msg);
void            print_message       (message_t msg);
unsigned char   get_control         (message_t msg);
unsigned char   get_data            (message_t msg);
int             get_id              (message_t msg);
int             check_interrupt     (message_t msg);
int             check_carry_data    (message_t msg);
int             check_halt          (message_t msg);

#define MSGSIZE 2

#define CPU_ID              0
#define BUS_ID              1
#define IO_DEVICE_ID        2
#define TRANSFER_DEVICE_ID  3
#define BUFFER_ID           4

#define MODE_READ           0
#define MODE_WRITE          1

#define STATE_MODE          0
#define STATE_ADDRESS       1
#define STATE_DATA          2

int 
main (int argc, char **argv) {
    // a pipe from cpu to bus, 
    // use 1 within cpu to write to bus
    // use 0 within bus to read from cpu
    int pipe_cpu_bus[2];
    // a pipe from bus to cpu, 
    // use 1 within bus to write to cpu 
    // use 0 within cpu to read from bus
    int pipe_bus_cpu[2];
  
    // error checking for pipe 
    if (pipe(pipe_cpu_bus) < 0) 
        exit(1); 
    if (pipe(pipe_bus_cpu) < 0) 
        exit(1);
  
    // Set the pipe to non-blocking
    if (fcntl(pipe_cpu_bus[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
    if (fcntl(pipe_bus_cpu[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
  
    switch (fork()) { 
        // error 
        case -1: 
            exit(3); 
    
        // child process (COMPUTER SYSTEM)
        case 0: 
            if (argc >= 3) {
                int length = atoi(argv[2]);
                computer_system(pipe_cpu_bus, pipe_bus_cpu, length);
            }
            else {
                printf("Usage: %s <file_name> <number>", argv[0]);
            }
            break; 
    
        // parent process (SYSTEM BUS)
        default: 
            if (argc >= 3) {
                char* filename = argv[1];
                system_bus(pipe_cpu_bus, pipe_bus_cpu, filename);
            }
            else {
                printf("Usage: %s <file_name> <number>", argv[0]);
            }
            break; 
    } 

    return 0; 
}

void
computer_system (int pipe_cpu_bus[], int pipe_bus_cpu[], int length) {
    message_t msg;

    msg = create_message(0, 1, 0, TRANSFER_DEVICE_ID, (length & 0b1111111100000000) >> 8);
    write_message(pipe_cpu_bus, msg);

    msg1 = create_message(0, 1, 0, TRANSFER_DEVICE_ID, length & 0b11111111);
    write_message(pipe_cpu_bus, msg);

    close(pipe_cpu_bus[0]); // close read end of cpu_bus
    close(pipe_bus_cpu[1]); // close write end of bus_cpu

    char buffer[10000] = {0};
    int index = 0;

    while (1) {
        msg = receive_from_pipe(pipe_bus_cpu, pipe_cpu_bus);
        if (msg != 0) {
            if (get_id(msg) == CPU_ID) {
                if (check_interrupt(msg)) {
                    // the data has been fully stored in buffer
                    if (get_data(msg) == 1) {
                        // get the data from buffer and print it
                        for (int i = 0; i < 128; i++) {
                            msg = create_message(0, 1, 0, BUFFER_ID, MODE_READ);
                            write_message(pipe_cpu_bus, msg);

                            msg = create_message(0, 1, 0, BUFFER_ID, i);
                            write_message(pipe_cpu_bus, msg);
                        }

                        while (1) {
                            msg = receive_from_pipe(pipe_bus_cpu, pipe_cpu_bus);
                            if (msg != 0) {
                                if (get_id(msg) == CPU_ID) {
                                    if (check_interrupt(msg)) {
                                        if (get_data(msg) == 33) {
                                            break;
                                        }
                                    }
                                    else {
                                        buffer[index++] = get_data(msg);
                                    }
                                }
                            }
                        }
                        
                        // acknowledge the transfer device so that it can
                        // resume sending bytes to buffer
                        msg = create_message(1, 1, 0, TRANSFER_DEVICE_ID, 0);
                        write_message(pipe_cpu_bus, msg);
                    }
                    else if (get_data(msg) == 2) {
                        printf("%s", buffer);
                        memset(buffer, 0, sizeof buffer);

                        // send halt to all devices
                        msg = create_message(0, 0, 1, BUS_ID, 0);
                        write_message(pipe_cpu_bus, msg);

                        exit(0);
                    }
                }
            }
        }
    }

    exit(0);
}

void
system_bus (int pipe_cpu_bus[], int pipe_bus_cpu[], char *filename) {

    // ======== CREATE IO DEVICE PROCESS ========

    int pipe_io_bus[2];
    int pipe_bus_io[2];
  
    // error checking for pipe 
    if (pipe(pipe_io_bus) < 0) 
        exit(1); 
    if (pipe(pipe_bus_io) < 0) 
        exit(1);
  
    // Set the pipe to non-blocking
    if (fcntl(pipe_io_bus[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
    if (fcntl(pipe_bus_io[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
  
    switch (fork()) { 
        // error 
        case -1: 
            exit(3); 
    
        // child process (IO DEVICE)
        case 0: 
            io_device(pipe_io_bus, pipe_bus_io, filename);
            return; // end the child process (io device)
            break; 

        default:
            break;
    } 
    // ======== CREATE TRANFER DEVICE PROCESS ========

    int pipe_tran_bus[2];
    int pipe_bus_tran[2];
  
    // error checking for pipe 
    if (pipe(pipe_tran_bus) < 0) 
        exit(1); 
    if (pipe(pipe_bus_tran) < 0) 
        exit(1);
  
    // Set the pipe to non-blocking
    if (fcntl(pipe_tran_bus[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
    if (fcntl(pipe_bus_tran[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
  
    switch (fork()) { 
        // error 
        case -1: 
            exit(3); 
    
        // child process (TRANSFER DEVICE)
        case 0: 
            transfer_device(pipe_tran_bus, pipe_bus_tran);
            return; // end the child process (transfer device)
            break; 

        default:
            break;
    } 
    // ======== CREATE BUFFER PROCESS ========
    int pipe_buf_bus[2];
    int pipe_bus_buf[2];
  
    // error checking for pipe 
    if (pipe(pipe_buf_bus) < 0) 
        exit(1); 
    if (pipe(pipe_bus_buf) < 0) 
        exit(1);
  
    // Set the pipe to non-blocking
    if (fcntl(pipe_buf_bus[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
    if (fcntl(pipe_bus_buf[0], F_SETFL, O_NONBLOCK) < 0) 
        exit(2); 
  
    switch (fork()) { 
        // error 
        case -1: 
            exit(3); 
    
        // child process (BUFFER)
        case 0: 
            buffer(pipe_buf_bus, pipe_bus_buf);
            return; // end the child process (buffer)
            break; 

        default:
            break;
    }
    // =======================================

    close(pipe_cpu_bus[1]); // close write end of cpu_bus
    close(pipe_bus_cpu[0]); // close read end of bus_cpu

    close(pipe_io_bus[1]);  // close write end of io_bus
    close(pipe_bus_io[0]);  // close read end of bus_io

    close(pipe_tran_bus[1]); // close write end of tran_bus
    close(pipe_bus_tran[0]); // close read end of bus_tran

    close(pipe_buf_bus[1]);  // close write end of buf_bus
    close(pipe_bus_buf[0]);  // close read end of bus_buf

    message_t msg = 0;

    // receive a message from each device and 
    // broadcast it to all other devices. 
    while (1) {
        msg = receive_from_pipe(pipe_cpu_bus, pipe_bus_cpu);
        if (msg != 0) {
            if (check_halt(msg)) {
                message_t msg_halt1 = create_message(0, 0, 1, IO_DEVICE_ID, 0);
                write_message(pipe_bus_io, msg_halt1);
                message_t msg_halt2 = create_message(0, 0, 1, TRANSFER_DEVICE_ID, 0);
                write_message(pipe_bus_tran, msg_halt2);
                message_t msg_halt3 = create_message(0, 0, 1, BUFFER_ID, 0);
                write_message(pipe_bus_buf, msg_halt3);
                exit(0);
            }

            // broadcast the msg to all devices
            write_message(pipe_bus_cpu, msg);
            write_message(pipe_bus_io, msg);
            write_message(pipe_bus_tran, msg);
            write_message(pipe_bus_buf, msg);
        }
        
        msg = receive_from_pipe(pipe_io_bus, pipe_bus_io);
        if (msg != 0) {
            // broadcast the msg to all devices
            write_message(pipe_bus_cpu, msg);
            write_message(pipe_bus_io, msg);
            write_message(pipe_bus_tran, msg);
            write_message(pipe_bus_buf, msg);
        }

        msg = receive_from_pipe(pipe_tran_bus, pipe_bus_tran);
        if (msg != 0) {
            // broadcast the msg to all devices
            write_message(pipe_bus_cpu, msg);
            write_message(pipe_bus_io, msg);
            write_message(pipe_bus_tran, msg);
            write_message(pipe_bus_buf, msg); 
        }

        msg = receive_from_pipe(pipe_buf_bus, pipe_bus_buf);
        if (msg != 0) {
            // broadcast the msg to all devices
            write_message(pipe_bus_cpu, msg);
            write_message(pipe_bus_io, msg);
            write_message(pipe_bus_tran, msg);
            write_message(pipe_bus_buf, msg);
        }
    }
}

void 
io_device (int pipe_io_bus[], int pipe_bus_io[], char *filename) {
    char byte;
    int wait_time;
    FILE *fp;

    int waiting = 0;
    int reading_line = 0;

    message_t msg = 0;

    close(pipe_io_bus[0]); // close read end of bus_io
    close(pipe_bus_io[1]); // close write end of io_bus

    fp = fopen(filename, "r");

    while (1) {
        if (!waiting) {
            byte = fgetc(fp);
            
            if (byte == -1) { // EOF
                exit(0);
            }

            if (!reading_line) {
                switch (byte) {
                    case 't':
                        byte = fgetc(fp); // skip space
                        reading_line = 1;
                        break;

                    case 'n':
                        byte = '\n';
                        waiting = 1;

                        char throw_away = fgetc(fp); // advance to char after newline
                        break;

                    case 'd':
                        fscanf(fp, " %d", &wait_time);
                        usleep(wait_time * 1000);

                        byte = fgetc(fp); // advance to char after newline
                        break;
                }
            }
            // read a char until newline is reached, once the char is read, 
            // wait for it to be sent to transfer device, then resume.
            else {
                if (byte == '\n') {
                    reading_line = 0;
                }
                else {
                    waiting = 1;
                }
            }
        }

        msg = receive_from_pipe(pipe_bus_io, pipe_io_bus);

        if (msg != 0) {
            if (get_id(msg) == IO_DEVICE_ID) {
                if (check_halt(msg)) {
                    exit(0);
                }
                if (get_data(msg) == 1) { 
                    // write character to transfer device
                    message_t msg1 = create_message(0, 1, 0, TRANSFER_DEVICE_ID, byte);
                    write_message(pipe_io_bus, msg1);

                    waiting = 0;
                }
            }
        }
    }
}

//  protocol for communication between transfer device and IO device:
//  transfer sends IO a msg 
//  if data == 1
//      then the IO should send back the next character 
//      via a msg with that character as the data

void 
transfer_device (int pipe_tran_bus[], int pipe_bus_tran[]) {
    int MAX_LENGTH = 128;

    message_t msg = 0;
    int length_has_been_read = 0;
    int length = 0;
    int index = 0;

    close(pipe_tran_bus[0]); // close read end of bus_io
    close(pipe_bus_tran[1]); // close write end of io_bus

    while (1) {
        msg = receive_from_pipe(pipe_bus_tran, pipe_tran_bus);

        if (msg != 0) {   
            if (get_id(msg) == TRANSFER_DEVICE_ID) {
                if (check_halt(msg)) {
                    exit(0);
                }

                // read the length passed by 2 messages
                if (length_has_been_read < 2) {
                    if (length_has_been_read == 0) {
                        length = get_data(msg) << 8;
                    }
                    else if (length_has_been_read == 1) {
                        int len = get_data(msg);
                        length += len;
                    
                        // tell the IO device to start sending a msg containing 
                        // a character to the transfer device
                        message_t msg1 = create_message(0, 1, 0, IO_DEVICE_ID, 1);
                        write_message(pipe_tran_bus, msg1);
                    }

                    length_has_been_read++;
                } 
                else {
                    char character = get_data(msg);

                    // send a chain of messages to the buffer to 
                    // store 'character' at address 'index'
                    message_t msg1 = create_message(0, 1, 0, BUFFER_ID, MODE_WRITE);
                    write_message(pipe_tran_bus, msg1);

                    message_t msg2 = create_message(0, 1, 0, BUFFER_ID, index);
                    write_message(pipe_tran_bus, msg2);

                    message_t msg3 = create_message(0, 1, 0, BUFFER_ID, character);
                    write_message(pipe_tran_bus, msg3);

                    // if the number of letters send to buffer is == 128,
                    // tell the CPU to read from the BUFFER, the come back
                    index++;
                    if (index == MAX_LENGTH) {
                        message_t msg6 = create_message(1, 1, 0, CPU_ID, 1);
                        write_message(pipe_tran_bus, msg6);

                        
                        // wait for acknowledge from CPU
                        while (1) {
                            msg = receive_from_pipe(pipe_bus_tran, pipe_tran_bus);
                            if (msg != 0) {
                                if (get_id(msg) == TRANSFER_DEVICE_ID) {
                                    if (check_interrupt(msg)) {
                                        break;
                                    }
                                }
                            }
                        }
                        
                        length = length - MAX_LENGTH;
                        index = 0;
                    }
                    // while there is still stuff in the IO, grab the next character
                    if (index < length) {
                        message_t msg4 = create_message(0, 1, 0, IO_DEVICE_ID, 1);
                        write_message(pipe_tran_bus, msg4);
                    }
                    else {
                        message_t msg6 = create_message(1, 1, 0, CPU_ID, 1);
                        write_message(pipe_tran_bus, msg6);

                        while (1) {
                            msg = receive_from_pipe(pipe_bus_tran, pipe_tran_bus);
                            if (msg != 0) {
                                if (get_id(msg) == TRANSFER_DEVICE_ID) {
                                    if (check_interrupt(msg)) {
                                        break;
                                    }
                                }
                            }
                        }

                        // send the CPU a message telling it the data 
                        // has been read to buffer
                        message_t msg5 = create_message(1, 1, 0, CPU_ID, 2);
                        write_message(pipe_tran_bus, msg5);
                    }
                }
            }
        }
    }
}

void 
buffer (int pipe_buf_bus[], int pipe_bus_buf[]) {
    message_t msg = 0;

    close(pipe_buf_bus[0]); // close read end of bus_io
    close(pipe_bus_buf[1]); // close write end of io_bus

    char buf[128] = {0};

    int mode = -1;
    int address = 0;
    int data = 0;

    int curr_state = STATE_MODE;

    while (1) {
        msg = receive_from_pipe(pipe_bus_buf, pipe_buf_bus);

        if (msg != 0) {
            if (get_id(msg) == BUFFER_ID) {
                if (check_halt(msg)) {
                    //printf("==== BUFFER HAS HALTED ====\n");
                    exit(0);
                }
                if (curr_state == STATE_MODE) {
                    mode = get_data(msg);

                    curr_state = STATE_ADDRESS;
                }
                else if (curr_state == STATE_ADDRESS) {
                    address = get_data(msg);

                    if (mode == MODE_WRITE) {
                        curr_state = STATE_DATA;
                    }
                    else if (mode == MODE_READ) {
                        data = buf[address];

                        //printf("============ BUFFER SENDING ['%c' @ %d] ============\n", data, address);
                        message_t msg1 = create_message(0, 1, 0, CPU_ID, data);
                        write_message(pipe_buf_bus, msg1);

                        if (address == 127 || data == 0) {
                            message_t msg9 = create_message(1, 1, 0, CPU_ID, 33);
                            write_message(pipe_buf_bus, msg9);
                            memset(buf, 0, sizeof buf);
                        }

                        mode = -1;
                        address = 0;
                        data = 0;

                        curr_state = STATE_MODE;
                    }
                }
                else if (curr_state == STATE_DATA) {
                    data = get_data(msg);

                    //printf("==== STORED ['%c'] IN BUFFER ====\n", data);
                    buf[address] = data;
                    mode = -1;
                    address = 0;
                    data = 0;

                    curr_state = STATE_MODE;
                }
            }
        }
    }
}

message_t
receive_from_pipe (int p1[], int p2[]) {
    int nread;
    unsigned char buf[MSGSIZE];

    memset(buf, 0, sizeof buf);
    nread = read(p1[0], buf, MSGSIZE);
    switch (nread) {
        // case -1 means pipe is empty and errno set EAGAIN
        case -1:
            if (errno == EAGAIN) {
                //printf("  (pipe empty)\n");
                break;
            }
            else {
                perror("recieving message from pipe");
                exit(4);
            }

        
        // case 0 means that all the bytes have been read, 
        // EOF has been reached
        case 0:
            close(p1[0]);
            close(p2[1]);
            exit(0);
        

        default:
            return convert_to_message(buf);
    }

    return 0;
}


// A message is comprised of the following:
// bit  | description
// ========================================
// 0    | interrupt signal
// 1    | carrying data
// 2    | halt?
// 3-7  | ID of device to receive message
// 8-15 | data
//
// ID # | device
// ===================
//   0  | CPU
//   1  | Bus
//   2  | IO
//   3  | Transfer 
//   4  | Buffer
message_t
create_message (int is, int cd, int halt, int id, int data) {
    message_t message = 0;

    if (is > 1 || is < 0) {
        printf("Interrupt Signal Bit [%d] must range from 0 to 1", is);
        exit(1);
    }

    if (cd > 1 || halt < 0) {
        printf("Carry Data Bit [%d] must range from 0 to 1", cd);
        exit(1);
    }

    if (halt > 1 || halt < 0) {
        printf("Halt Bit must [%d] range form 0 to 1", halt);
        exit(1);
    }

    if (id > 4 || id < 0) {
        printf("ID must range [%d] from 0 to 4", id);
        exit(1);
    }

    if (data > 255 || data < 0) {
        printf("Data [%d] must range from 0 to 255.", data);
        exit(1);
    }

    message += is << 15;
    message += cd << 14;
    message += halt << 13;
    message += id << 8;
    message += data << 0;

    return message;
}

message_t
convert_to_message (unsigned char buf[2]) {
    message_t msg = 0;
    msg += buf[0] << 8;
    msg += buf[1];
    return msg;
}

void
write_message (int pipe[], message_t msg) {
    unsigned char buf[MSGSIZE] = {(unsigned char) (msg >> 8), (unsigned char) msg};;
    write(pipe[1], buf, MSGSIZE);
}

void
print_message (message_t msg) {
    int mask = 1 << 15;
    for (int i = 15; i >= 0; i--, mask /= 2) {
        if (msg & mask) 
            printf("1");
        else
            printf("0");
    }
}

unsigned char 
get_control (message_t msg) {
    message_t leftside = msg & 0b1111111100000000;
    unsigned char c = leftside >> 8;
    return c;
}

unsigned char 
get_data (message_t msg) {
    message_t leftside = msg & 0b0000000011111111;
    unsigned char c = leftside;
    return c;
}

int
get_id (message_t msg) {
    int id = msg >> 8;
    id = id & 0b00011111;
    return id;
}

int
check_interrupt (message_t msg) {
    return (msg & (1 << 15)) != 0;
}

int
check_carry_data (message_t msg) {
    return (msg & (1 << 14)) != 0;
}

int
check_halt (message_t msg) {
    return (msg & (1 << 13)) != 0;
}