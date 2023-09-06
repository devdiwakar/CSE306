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


static int newpagesize;
enum {page_size = 4096};
static int remote_sock, new_socket;
unsigned long totalpages = 0;
//enum state {M, S, I};

static const int readoperation = 0;
static const int invalidate = 1;
static const int markshared = 2;

static char *baseaddr;

static char msi_array[64];
static char newmessage[page_size];

/*
-----------------------------------------------------------------------------------------------------
		Communication with the responder function
-----------------------------------------------------------------------------------------------------
*/

char * commwithresponder(int request_page, int operation) 
{
     char * retvalue;
    sprintf(newmessage, "%d-%d", request_page , operation);
    send(remote_sock, newmessage, strlen(newmessage), 0);
    int bytes = 0;
    memset(newmessage, '\0', page_size);
    bytes = read(remote_sock, newmessage, page_size);
    if (bytes < 0) 
    {
        perror("Error in reading");
        exit(EXIT_FAILURE);
    } 
    else if (bytes == 0) 
    {
        printf("Read 0 bytes \n");
        newmessage[0] = '\0';
    }
    retvalue = newmessage;
    return retvalue;
}


/*
-----------------------------------------------------------------------------------------------------
		Fault Handler Thread Function
-----------------------------------------------------------------------------------------------------
*/
static void *fault_handler_thread(void *arg) 
{
    static struct uffd_msg msg;
    long uffd;
    static char *page = NULL;
    struct uffdio_copy uffdio_copy;
    ssize_t nread;

    uffd = (long) arg;
    if (page == NULL) 
    {
        page = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
            errExit("mmap");
    }

    for (;;) 
    {
		struct pollfd pollfd;
		int nready;

		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
		    errExit("poll");

		printf(" [x] PAGEFAULT\n");

		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) 
		{
		    printf("EOF on userfaultfd!\n");
		    exit(EXIT_FAILURE);
		}

		if (nread == -1)
		    errExit("read");

		if (msg.event != UFFD_EVENT_PAGEFAULT) 
		{
		    fprintf(stderr, "Unexpected event on userfaultfd\n");
		    exit(EXIT_FAILURE);
		}

		memset(page, '\0', page_size);
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address & ~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
		    errExit("ioctl-UFFDIO_COPY");
    }
}

/*
-----------------------------------------------------------------------------------------------------
		responder function for handling communication from other process
-----------------------------------------------------------------------------------------------------
*/

static void *responder(void *arg) 
{
    char buffer[page_size];
    int bytesRead = 0;
    for (;;) 
    {
        memset(buffer, '\0', page_size);
        bytesRead = read(new_socket, buffer, page_size);
        if (bytesRead < 0) 
        {
            perror("Error in reading");
            exit(EXIT_FAILURE);

        } 
        
        else if (bytesRead > 0) 
        {
            char message[page_size];

            char *token;
            token = strtok(buffer, "-");
            int request_page = atoi(token);
            token = strtok(NULL, "-");
            int read_invalidate = atoi(token);
            
            if (read_invalidate == invalidate) 
            {
                
                printf("\n Received Request for Page: %d for operation invalidate \n", request_page);
                msi_array[request_page] = 'I';
                char * page_addr = baseaddr + (request_page * page_size);
                if (madvise(page_addr, page_size, MADV_DONTNEED)) 
                {
                    errExit("Fail to madvise");
                }
                snprintf(message, page_size, "Invalidated");

            }
            else if (read_invalidate == readoperation) 
            {
                    printf("\n Received Request for Page: %d for operation read \n", request_page);
                    char * new;   
                    if ( msi_array[request_page] == 'M' )
		    {
			    msi_array[request_page] = 'S';
			    new = baseaddr + (request_page * page_size);
		    }
		    else if ( msi_array[request_page] == 'S' ) {new = baseaddr + (request_page * page_size);}
		    else if ( msi_array[request_page] == 'I' ) {new = "NULL";}
		    else 				       {new = '\0';}	
                
		    msi_array[request_page] = 'S';
		    snprintf(message, page_size, "%s", new);
            }
            
            
            else if (read_invalidate == markshared)
            {
	    	printf("\n Received Request for Page: %d for operation markshared \n", request_page);
	    	msi_array[request_page] = 'S';
	    	char * new = "MarkedShared";
	    	snprintf(message, page_size, "%s", new);
            } 
            
            
            printf(" Responding with : %s\n", message);
            send(new_socket, message, strlen(message), 0);
        }

    }
}

/*
-----------------------------------------------------------------------------------------------------
		input write function
-----------------------------------------------------------------------------------------------------
*/

void inputwrite(char *addr, int current_page, const char *message) 
{
    int l;
    for (int i = ((current_page == -1) ? 0 : current_page); i <= ((current_page == -1) ? totalpages - 1 : current_page); ++i) 
    {
        l = 0x0 + (i * page_size);
        for (int j = 0; message[j] != '\0'; ++j) {  addr[l++] = message[j]; }
        addr[l] = '\0';
    }
}

/*
-----------------------------------------------------------------------------------------------------
		input read function
-----------------------------------------------------------------------------------------------------
*/
void inputread(char *addr, int current_page, bool fetch_from_remote) 
{
    char output[page_size + 1];
    char * message;
    for (int j = ((current_page == -1) ? 0 : current_page); j <= ((current_page == -1) ? (totalpages - 1) : current_page); ++j) 
    {
    	sprintf(output, "%s", addr + (j * page_size)); // output has contents of the curr page
        /*      If msi_array[j] is M or S then we can just print the contents and be done with it. 
        	But for I, we need to check the contents from the other process cache. 
	        After checking, since both the process are in sync, we set both status to shared.
        */
        
        if (msi_array[j] == 'I')			
        {
                printf(" Status is invalid for Page %d. Getting the message from peer \n",j);
                char message2[] = "";
                if(fetch_from_remote) 
                {
		            message = commwithresponder(j, readoperation);
		            if(strcmp("NULL", message)!=0) 
		            {
		                inputwrite(addr, j, message);
		            }
		            
		            else 
		            {
		                message = message2;
		            }
		            msi_array[j] = 'S';
                }
                
                else message = message2;
                sprintf(output, "%s", message);
        }
        
        
	else if ( ( msi_array[j] == 'M' ) && fetch_from_remote)
	
	{
	
		message = commwithresponder(j, markshared);
		msi_array[j] = 'S';
	
	}        
        
        
               printf(" [*] Page %d:\n%s\n", j, output);
    }

}



/*
-----------------------------------------------------------------------------------------------------
		Main Function
-----------------------------------------------------------------------------------------------------
*/
int main(int argc, char *argv[]) {
    
    /* Socket code is taken from the server.c and client.c files given for Assignment 1 in code-socket folder 
       Naming convention is so that local is server side and remote is client side. Since here the client doesnt ask for any input.
    */

/*
-----------------------------------------------------------------------------------------------------
		Socket Programming Code
-----------------------------------------------------------------------------------------------------
*/
    
    int server_fd;
    struct sockaddr_in local_addr, remote_addr;
    
    int opt = 1;
    bool local_server_flag = false; /* indicates if this is local or remote side i.e server or client side. if true it is local/server side. */
    int addrlen = sizeof(local_addr);
    char buff[BUFF_SIZE] = {0};
    char *addr;

    unsigned long local_port, remote_port; 
    unsigned long len = 0;

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
    
//    page_size = sysconf(_SC_PAGE_SIZE);
//  #define  page_size sysconf(_SC_PAGE_SIZE);
    newpagesize = sysconf(_SC_PAGE_SIZE);
    
    /* local_server_flag indicates this is server side */
    if (local_server_flag) 
    {
        printf("How many pages would you like to allocate (greater than 0)? : ");
        if (scanf("%lu", &totalpages) == EOF) { perror("end of file"); }
        len = totalpages * page_size;
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
        
        totalpages = len / page_size;
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
        baseaddr = addr;
    
/*
-----------------------------------------------------------------------------------------------------
		Userfaultfd code 
-----------------------------------------------------------------------------------------------------
*/

    
    char read_write_view;
    long uffd;
    int currpage, s;
    int a; /* return value for scanf else it throws compilation error */
    
    pthread_t thr,resp;
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;
    
    char message[page_size];

    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1) {
        errExit("userfaultfd");
    }
    uffdio_api.api = UFFD_API;
    uffdio_api.features = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
        errExit("ioctl-UFFDIO_API");
    }
    uffdio_register.range.start = (unsigned long) addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
        errExit("ioctl-UFFDIO_REGISTER");

    s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }

/*
-----------------------------------------------------------------------------------------------------
		 Initializing the MSI ARRAY with invalid state I
-----------------------------------------------------------------------------------------------------
*/


    for (int j = 0; j <= totalpages; ++j) 
    {
        msi_array[j] = 'I';
    }
     
    
    s = pthread_create(&resp, NULL, responder, (void *) uffd);
    if (s != 0) {
        errno = s;
        errExit("pthread_create");
    }
     
     
/*
-----------------------------------------------------------------------------------------------------
		 infinite while loop for user inputs
-----------------------------------------------------------------------------------------------------
*/
 
    while(1) 
    {
        printf("> Which command should I run? (r:read, w:write, v:view msi array): ");
        a = scanf(" %c", &read_write_view);
        
        if (a != 1) 
        {
        	printf("Incorrect no of arguments. please enter r (read), w (write) or v (view) : ");
        }
        
        
        if (read_write_view != 'v' )
        {
        	printf("\n> For which page? (0-%lu, or -1 for all): ", totalpages - 1);
	        a = scanf(" %d", &currpage);	
        }
        
/*
-----------------------------------------------------------------------------------------------------
		 read logic
-----------------------------------------------------------------------------------------------------
*/
 
        if (read_write_view == 'r') 
        { inputread(addr, currpage, true);} 

/*
-----------------------------------------------------------------------------------------------------
		 write logic
-----------------------------------------------------------------------------------------------------
*/
 
        else if (read_write_view == 'w') 
        {
            printf("> Type your new message: ");
            a = scanf(" %[^\n]", message);
            
            inputwrite(addr, currpage, message);
            
            for (int i = ((currpage == -1) ? 0 : currpage); i <= ((currpage == -1) ? totalpages - 1 : currpage); ++i) 
            {
            	    msi_array[i] = 'M';
		    printf("Writing to Page %d, Status changed to M, Invalidated Peer cache \n", i);
		    commwithresponder(i, invalidate);
	    }

            inputread(addr, currpage, false);

        }
        
/*
-----------------------------------------------------------------------------------------------------
		 view logic
-----------------------------------------------------------------------------------------------------
*/
        
        else if (read_write_view == 'v') 
        {
        	for (int i = 0; i < totalpages; ++i)
        	{
        		printf(" [*] Page %d : %c \n", i, msi_array[i]);
        	}
        } 
        
        else 
        {
            printf("Invalid Argument : please enter r (read), w (write) or v (view) : ");
        }
    
    }
    

    return 0;
}


