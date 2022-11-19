/*
File: 	aesdsocket.c
Author: Allen Gonzalez
*/

//Includes
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

//Defines
#define     BACKLOG     (10)
#define     LOG_PATH        ("/var/tmp/aesdsocketdata")
#define     RECV_BUFF_LEN   (1024)
#define     PORT   "9000"

//Flag for Signal_Handler
int graceful_exit = 0;
int sck = 0;

//Sets the graceful_exit flag if SIGINT or SIGTERM signals are raised
void Signal_Handler(int signal_number){
//From slide 8 of signal management pdf
	
    if(signal_number == SIGINT || signal_number == SIGTERM)
    {
       syslog(LOG_INFO, "Caught signal, exiting");
    
       graceful_exit = 1;
    }
}

//Sets up the struct, calls socket() and bind()
void Socket_Bind_Setup(){
//From slides 11-20 of sockets pdf
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(NULL, PORT, &hints, &res) != 0)
    {
        syslog(LOG_ERR, "An error occurred setting up the socket.");
        exit(1);
    }
    //Create the socket file descriptor
    sck = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sck == -1)
    {
        syslog(LOG_ERR, "An error occurred setting up the socket: %s", strerror(errno));
        exit(1);
    }
    //Bind the socket to the addr+port specified in "getaddrinfo"
    if(bind(sck, res->ai_addr, res->ai_addrlen) == -1)
    {
        syslog(LOG_ERR, "An error occurred binding the socket: %s", strerror(errno));
        exit(1);        
    }
    freeaddrinfo(res);
};

//
void socketserver(int sck){
//From page 21 of sockets pdf and https://beej.us/guide/bgnet/html/#a-simple-stream-server
    if(listen(sck, BACKLOG) < 0)
    {
        syslog(LOG_ERR, "Error with listen(): %s", strerror(errno));
        exit(1);        
    }
    syslog(LOG_INFO, "Listening on port 9000");
    //Create the file that will log the messages received
    int file_fd = open(LOG_PATH, O_CREAT | O_RDWR | O_TRUNC, 0666);
    int file_size = 0;
    char *recv_data = NULL;
    while(!graceful_exit){
		
		        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof client_addr;

        //Accept a connection
        int connection_fd = accept(sck, (struct sockaddr *) &client_addr, &addr_size);
        if(connection_fd == -1)
        {
            if(errno == EINTR)
            {
                //The signal has set "graceful_exit" and the next while iteration will not happen
                break;
            }
            else
            {
                syslog(LOG_ERR, "An error occurred accepting a new connection to the socket: %s", strerror(errno));
                exit(1);
            }
        }
		
		//Get information from the client
		//Credits: https://stackoverflow.com/questions/1276294/getting-ipv4-address-from-a-sockaddr-structure
		if(client_addr.ss_family == AF_INET)
		{
			char addr[INET6_ADDRSTRLEN];
			struct sockaddr_in *addr_in = (struct sockaddr_in *)&client_addr;
			inet_ntop(AF_INET, &(addr_in->sin_addr), addr, INET_ADDRSTRLEN);
			syslog(LOG_INFO, "Accepted connection from %s", addr);
		}
		else if(client_addr.ss_family == AF_INET6)
		{
			char addr[INET6_ADDRSTRLEN];
			struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&client_addr;
			inet_ntop(AF_INET6, &(addr_in6->sin6_addr), addr, INET6_ADDRSTRLEN);
			syslog(LOG_INFO, "Accepted connection from %s", addr);
		}


        //Wait for data
        int recv_ret;
        int index = 0;
        //Keep track of how many RECV_BUFF_LEN chunks "recv_data" has
        int chunks = 1;
        
        //Start with a size, potentially increasing it if an entire packet cannot fit into it
        recv_data = malloc(sizeof(char)*RECV_BUFF_LEN*chunks);
        if(!recv_data)
        {
            syslog(LOG_ERR, "Could not malloc: %s", strerror(errno));
            exit(1);  
        }

        do
        {
            recv_ret = recv(connection_fd, &recv_data[index], RECV_BUFF_LEN, 0);
            if(recv_ret == -1)
            {
                syslog(LOG_ERR, "An error occurred reading from the socket: %s", strerror(errno));
                exit(1);  
            }
            index += recv_ret;

            if(index != 0)
            {
                //Check if the last value received is "\n"
                if(recv_data[index - 1] == '\n')
                {
                    //Put the contents into /var/tmp/aesdsocketdata
                    //Write the string to the file
                    //Send all the contents read from /var/tmp/aesdsocketdata back to the client
                    if(lseek(file_fd, 0, SEEK_END) == -1)
                    {
                        syslog(LOG_ERR, "Could not get to the end of the file: %s", strerror(errno));
                        exit(1);  
                    }

                    int written_bytes;
                    int len_to_write = index;
                    char *ptr_to_write = recv_data;
                    while(len_to_write != 0)
                    {
                        written_bytes = write(file_fd, ptr_to_write, len_to_write);
                        if(written_bytes == -1)
                        {
                            //If the error is caused by an interruption of the system call try again
                            if(errno == EINTR)
                                continue;

                            //Else, error occurred, print it to syslog and finish program
                            syslog(LOG_ERR, "Could not write to the file: %s", strerror(errno));
                            exit(1);
                        }
                        len_to_write -= written_bytes;
                        ptr_to_write += written_bytes; 
                    }

                    file_size += index;

                    //Send all the contents read from /var/tmp/aesdsocketdata back to the client
                    if(lseek(file_fd, 0, SEEK_SET) == -1)
                    {
                        syslog(LOG_ERR, "Could not get to the beginning of the file: %s", strerror(errno));
                        exit(1);  
                    }
                    
                    //Perform reads to send the file contents to the socket client
                    int to_be_sent = file_size;
                    char buff_read[RECV_BUFF_LEN];
                    while(to_be_sent)
                    {
                        int send_bytes = 0;
                        int read_bytes = read(file_fd, buff_read, RECV_BUFF_LEN);
                        if(read_bytes != 0)
                            send_bytes = read_bytes;

                        if(read_bytes == -1)
                        {
                            //If the error is caused by an interruption of the system call try again
                            if(errno == EINTR)
                                continue;

                            //Else, error occurred, print it to syslog and finish program
                            syslog(LOG_ERR, "Could not read from the file: %s", strerror(errno));
                            exit(1);
                        }

                        //Less bytes remaining
                        to_be_sent -= read_bytes;
                        
                        //Send the contents back to the client
                        int sent_bytes = -1;
                        int send_off = 0;
                        syslog(LOG_INFO, "Send bytes: %d", send_bytes);
                        while(sent_bytes != 0)
                        {
                            sent_bytes = send(connection_fd, &buff_read[send_off], send_bytes, 0);
                            if(sent_bytes == -1)
                            {
                                //If the error is caused by an interruption of the system call try again
                                if(errno == EINTR)
                                    continue;

                                //Else, error occurred, print it to syslog and finish program
                                syslog(LOG_ERR, "Could not read from the file: %s", strerror(errno));
                                exit(1);
                            }
                            send_bytes -= sent_bytes;
                            send_off += sent_bytes;
                        }
                        
                    }

                    //Reset index to use the malloc'ed buffer from the beginning
                    index = 0;
                }
                //Realloc the array if it got full without an '\n'
                else if(index == (RECV_BUFF_LEN*chunks))
                {
                    chunks++;
                    recv_data = realloc(recv_data, sizeof(char)*RECV_BUFF_LEN*chunks);
                    if(!recv_data)
                    {
                        syslog(LOG_ERR, "Could not realloc: %s", strerror(errno));
                        exit(1);  
                    }
                }
            }

        } while(recv_ret != 0 && !graceful_exit);

        //Free the used buffer
        free(recv_data);
        if(client_addr.ss_family == AF_INET)
		{
			char addr[INET6_ADDRSTRLEN];
			struct sockaddr_in *addr_in = (struct sockaddr_in *)&client_addr;
			inet_ntop(AF_INET, &(addr_in->sin_addr), addr, INET_ADDRSTRLEN);
			syslog(LOG_INFO, "Closed connection from %s", addr);
		}
		else if(client_addr.ss_family == AF_INET6)
		{
			char addr[INET6_ADDRSTRLEN];
			struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&client_addr;
			inet_ntop(AF_INET6, &(addr_in6->sin6_addr), addr, INET6_ADDRSTRLEN);
			syslog(LOG_INFO, "Closed connection from %s", addr);
		}
	   
		
	}
	close(sck);
	close(file_fd);
	remove(LOG_PATH);
	return;
}

//Sets up the signals handler
void Signal_Handler_Setup(){
//From slide 8 of signal management pdf
    struct sigaction GracefulExit; //the action will set the graceful_exit flag
    GracefulExit.sa_handler = Signal_Handler;
    GracefulExit.sa_flags = 0;
    sigset_t empty;
    if(sigemptyset(&empty) == -1)
    {
        syslog(LOG_ERR, "Could not set up empty signal set: %s.", strerror(errno));
        exit(1); 
    }
    GracefulExit.sa_mask = empty;
    if(sigaction(SIGINT, &GracefulExit, NULL) != 0)
    {
        syslog(LOG_ERR, "Could not set up handle for SIGINT: %s.", strerror(errno));
        exit(1);
    }
    if(sigaction(SIGTERM, &GracefulExit, NULL)!= 0)
    {
        syslog(LOG_ERR, "Could not set up handle for SIGTERM: %s.", strerror(errno));
        exit(1);
    }
	
}

//program description
int main(int c, char **argv)
{    
	//Open the log to write to the default "/var/log/syslog" and set the LOG_USER facility
    openlog(NULL, 0, LOG_USER);
	
	Signal_Handler_Setup();
	
	Socket_Bind_Setup();
	
	//Check if -d has been provided
    if(c == 1)
    {
        syslog(LOG_INFO, "Server running in no-daemon mode.");
        //Handle server
        socketserver(sck);
    }
    else if(c == 1 + 1)
    {
        //The argument must be -d
        if(strcmp(argv[1], "-d") != 0)
        {
            syslog(LOG_ERR, "Invalid number of arguments");
            exit(1);
        }

        //Start server as a daemon
        syslog(LOG_INFO, "Server running in daemon mode.");
        int fork_ret = fork();
        if(fork_ret == -1)
        {
            syslog(LOG_ERR, "Unable to perform fork that creates daemon: %s.", strerror(errno));
            exit(1);
        }
        else if(fork_ret == 0)
        {
            //Child process
            //Create new session and process group to prevent Terminal signals mixing with the Daemon
            if(setsid() == -1)
            {
                syslog(LOG_ERR, "Unable to create a new session and process group: %s", strerror(errno));
                exit(1);
            }
            //Set the working directory to the root directory
            if(chdir("/") == -1)
            {
                syslog(LOG_ERR, "Unable to change working directory: %s", strerror(errno));
                exit(1);
            }

            //stdin, stdout and stderr could be redirected but I will not do it 
            //to align to the demonstration from Coursera, where killing the daemon outputs in stdout.

            //Handle server
            socketserver(sck);
            
            exit(0);
        }
        //Exit the parent process to actually make the child process be a daemon
        exit(0);
    }
    else
    {
        syslog(LOG_ERR, "Invalid number of arguments");
        exit(1);
    }
	
	
	return 0;
}


