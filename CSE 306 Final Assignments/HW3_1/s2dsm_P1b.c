#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>

#define BUFF_SIZE 4096

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);    \
    } while (0)

static int page_size;

int main(int argc, char *argv[]) {
    
    /* Socket code is taken from the server.c and client.c files given for Assignment 1 in code-socket folder 
       Naming convention is so that local is server side and remote is client side. Since here the client doesnt ask for any input.
    */
    
    int server_fd, remote_sock, new_socket;
    struct sockaddr_in local_addr, remote_addr;
    
    int opt = 1;
    bool local_server_flag = false; /* indicates if this is local or remote side i.e server or client side. if true it is local/server side. */
    int addrlen = sizeof(local_addr);
    char buff[BUFF_SIZE] = {0};
    char *addr;

    unsigned long local_port, remote_port; 
    unsigned long page_num = 0, len = 0;

    /* If not given local port and_remote port i.e server and client then throw an error and exit */
    if (argc != 3) 
    {
        printf("FAILURE: Need to enter two arguments\n");
        exit(EXIT_FAILURE);
    }
    local_port = strtoul(argv[1], NULL, 0);
    remote_port = strtoul(argv[2], NULL, 0);
    printf("Local Port , Remote Port : %lu %lu \n", local_port, remote_port);

    /* Creating the socket for local and remote sides i.e server and client sides respectively */
 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    	{
		perror("Unable to create local socket \n");
		exit(EXIT_FAILURE);
	}

    if ((remote_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		errExit("Unable to create remote socket \n");
	}
	    
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		       &opt, sizeof(opt))) 
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

    /* addr for local and remote sides */ 
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);

    memset(&remote_addr, '0', sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);

    
    if (bind(server_fd, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) 
    {
		perror("bind failed");
		exit(EXIT_FAILURE);
    }

    if (inet_pton(AF_INET, "127.0.0.1", &remote_addr.sin_addr) <= 0) 
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (listen(server_fd, 3) < 0) 
    {
        	perror("listen");
		exit(EXIT_FAILURE);
    }

    /* local_server_flag will determine if it is the server or client side. 
       since we writing the code in the same program for both sides, initally when we run server side, 
       connection wont be possible since client has not run. so there we set the flag to be True. 
       When we run same code from client side, the connection will be established hence this flag will remain false. 
       This is how we distinguish from server and client in the same program. 
    */
    if (!local_server_flag) 
    {
        if (connect(remote_sock, (struct sockaddr *) &remote_addr, sizeof(remote_addr)) < 0) 
        {
            local_server_flag = true;
        } 
    }

    printf("Waiting to accept connection  \n");
    
    if ((new_socket = accept(server_fd, (struct sockaddr *) &local_addr, (socklen_t *) &addrlen)) < 0) 
    {
        perror("accept");
	exit(EXIT_FAILURE);
    }
    printf("Connection Established \n");

    if (local_server_flag) 
    {
        if (connect(remote_sock, (struct sockaddr *) &remote_addr, sizeof(remote_addr)) < 0) 
        {
            printf("Connection attempt failed from server \n");
    	} 
        else 
        {
            printf("Connection attempt successful from server \n");
        }
    }
    
    page_size = sysconf(_SC_PAGE_SIZE);

    /* local_server_flag indicates this is server side */
    if (local_server_flag) 
    {
        printf("How many pages would you like to allocate (greater than 0)? : ");
        if (scanf("%lu", &page_num) == EOF) { perror("end of file"); }
        len = page_num * page_size;
        addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (addr == MAP_FAILED) 
        {
            perror("Error in address allocation in mmap \n");
            exit(EXIT_FAILURE);
        }
        printf("Address mapping created at : %p\n", addr); /* return value of mmap */
        printf("mmapped memory size : %lu\n", len);	   /* mmap size */
        
        snprintf(buff, BUFF_SIZE, "%lu-%p", len, addr); /* len and addr separated by - */
        send(remote_sock, buff, strlen(buff), 0); /* sends a message including the mmaped address and size to the second process over a socket communication */

    } 
    
    else 
    {
        if (read(new_socket, buff, 1024) < 0) 
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        char *token;
        token = strtok(buff, "-");		/* decoding buff by - */
        len = strtoul(token, NULL, 0);
        
        page_num = len / page_size;
        token = strtok(NULL, "-");
        addr = token;
        printf("Address received from server/local : %s \n", token);
        sscanf(token, "%p", &addr);
        addr = mmap(addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (addr == MAP_FAILED) 
        {
            perror("Error in address allocation in mmap \n");
            exit(EXIT_FAILURE);
        }
        
        printf("Address returned by mmap()         : %p \n", addr);
        printf("mmapped size %lu\n", len);

    }

    return 0;
}


