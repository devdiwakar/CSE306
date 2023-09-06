/* userfaultfd_demo.c

   Licensed under the GNU General Public License version 2 or later.
*/
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
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

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE);	\
	} while (0)

static int page_size;

static void *
fault_handler_thread(void *arg)
{
	static struct uffd_msg msg;   /* Data read from userfaultfd */
	static int fault_cnt = 0;     /* Number of faults so far handled */
	long uffd;                    /* userfaultfd file descriptor */
	static char *page = NULL;
	struct uffdio_copy uffdio_copy;
	ssize_t nread;

	uffd = (long) arg;

	/* [H1]
	   The mmap() function is used for mapping between a process address space and either files or devices.  mmap : (void *address, size_t length, int protect, int flags, 
	   int filedes, off_t offset). The first argument is address, since we have passed NULL the mmap function will create the addr as it sees fit. page_size is the memory size that needs to be    
	   allocated. PROT_READ | PROT_WRITE gives read and write permissions for the pages. MAP_PRIVATE: the mapping will not be seen by any other processes, and the changes made will not be written 		
	   to the file. MAP_ANONYMOUS : This flag is used to create an anonymous mapping. Anonymous mapping means the mapping is not connected to any files. This mapping is used as the basic primitive 	
	   to extend the heap. -1 is the file descriptor and last is the offset. On success, the mmap() returns 0; for failure, the function returns MAP_FAILED, so we exit. 
	 */
	if (page == NULL) {
		page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page == MAP_FAILED)
			errExit("mmap");
	}

	/* [H2]
	 This is an infinite loop. It will run indefinitely. Here it checks for page faults and executes the instructions inside in that case. 
	 */
	for (;;) {

		/* See what poll() tells us about the userfaultfd */

		struct pollfd pollfd;
		int nready;

		/* [H3]
		 Created a poll file descriptor to manage poll system call. poll : int (struct pollfd *ufds, unsigned int nfds, int timeout)
		 ufds is the poll file descriptor struct. Using POLLIN, ordinary information data can be read. Here the poll function will return the no of file descriptors ready to be read. The last 		
		 argument is negative which is timeout which indicates an infinite time. If return value is -1, we exit else print the details in case of successful call.
		 */
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1)
			errExit("poll");

		printf("\nfault_handler_thread():\n");
		printf("    poll() returns: nready = %d; "
                       "POLLIN = %d; POLLERR = %d\n", nready,
                       (pollfd.revents & POLLIN) != 0,
                       (pollfd.revents & POLLERR) != 0);

		/* [H4]
		 We use the read function to read out contents of the user fault file descriptor (uffd) upto the length of msg bytes. The data from uffd is stored in the msg varibale which is of 	 
		 struct uffd_msg defined at the top. In case of return value of -1 (error) we return by saying err in read. else we print "EOF on userfaultfd!\n" as given below. 
		 */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1)
			errExit("read");

		/* [H5]
		 UFFD_EVENT_PAGEFAULT indicates a page fault event. In the msg read above, event flag is compared with the flag for page fault. If it is not a page fault
		 then the program exits by printing "Unexpected event on userfaultfd\n".
		 */
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		/* [H6]
		 If the event is actually of the page fault, we print the values of the flag and the addr from the corresponding fields of the msg variable. 
		 */
		printf("    UFFD_EVENT_PAGEFAULT event: ");
		printf("flags = %llx; ", msg.arg.pagefault.flags);
		printf("address = %llx\n", msg.arg.pagefault.address);

		/* [H7]
		 * Explain following in here.
		 The memset function copies the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str in 
		 void *memset(void *str, int c, size_t n).
		 Here, we store the no of page faults in fault_cnt (increment by one when page fault occurs) and add strings starting from A to the memory location pointed to by page.
		 It will reset after every 20 page faults because of mod 20 operator used. 
		 */
		memset(page, 'A' + fault_cnt % 20, page_size);
		fault_cnt++;

		/* [H8]
		 Storing the details of page fault in the uddf IO copy struct. The src and the dst and len equal to page, addr and page_size respectively. We need to count in terms of page_size hence 
		 rounding the address is required which is done through & with ~(page_size - 1)
		 */
		uffdio_copy.src = (unsigned long) page;
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
			~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;

		/* [H9]
		 The ioctl function is the IO control function which we can use to access some complex types of files. int ioctl(fd, cmd, argp);
		 The first argument is the user fault file descriptor which is uffd.
	         UFFDIO_COPY copies a continuous memory chunk into the userfault registered range. 
	         uffdio_copy variable is pointed to by argp given in the third argument. 
	         If return value is -1, we exit.
		 */
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
			errExit("ioctl-UFFDIO_COPY");

		/* [H10]
		 If ioctl() call is successful we print the values corresponding to the uffdio_copy struct. 
		 */
		printf("        (uffdio_copy.copy returned %lld)\n",
                       uffdio_copy.copy);
	}
}

int
main(int argc, char *argv[])
{
	long uffd;          /* userfaultfd file descriptor */
	char *addr;         /* Start of region handled by userfaultfd */
	unsigned long len;  /* Length of region handled by userfaultfd */
	pthread_t thr;      /* ID of thread that handles page faults */
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	int s;
	int l;

	/* [M1]
	 It checks the no of arguments passed to the program. We need to input the no of pages. In case of incorrect no of arguments or no argument, 
	 it prints as given below and exits through FAILURE. 
	 */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s num-pages\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* [M2]
	 sysconf - gets the configuration information at run time. sysconf(_SC_PAGE_SIZE) returns the size of the page (memory allocation) in bytes at runtime. 
	 strtoul is a function which is 'str'ing 'to' 'u'nsigned 'l'ong whoch converts a string to long. Multiplying the size of the page with the no of pages (given as input), 
	 we get the total length and allocate that for mmap in subsequent steps.
	 */
	page_size = sysconf(_SC_PAGE_SIZE);
	len = strtoul(argv[1], NULL, 0) * page_size;

	/* [M3]
	 This creates and enables a userfaultfd file descriptor.
	 We input flags O_CLOEXEC and O_NONBLOCK which are given below.
	 O_CLOEXEC  : enables the close-on-exec flag for the new file descriptor.
	 O_NONBLOCK : allows opening the file descriptor in a non blocking way.
	 In case of error, the syscall returns -1, and exits the program.
	 */
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1)
		errExit("userfaultfd");

	/* [M4]
	 UFFDIO_API enables operation of the userfaultfd and perform API handshake.
	 The ioctl function is the IO control function which we can use to access some complex types of files. int ioctl(fd, cmd, argp);
	 The first argument is the user fault file descriptor which is uffd.
	 UFFD_API sets the API protocol is set to UFFD_API. 
	 uffdio_api variable is pointed to by argp given in the third argument. 
	 If return value is -1, we exit.
	 */
	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
		errExit("ioctl-UFFDIO_API");

	/* [M5]
	   The mmap() function is used for mapping between a process address space and either files or devices.  
	   mmap : (void *address, size_t length, int protect, int flags, int filedes, off_t offset). 
	   The first argument is address, since we have passed NULL the mmap function will create the addr as it sees fit. 
	   len is the memory size that needs to be allocated. We gake
	   PROT_READ | PROT_WRITE gives read and write permissions for the pages. 
	   MAP_PRIVATE: the mapping will not be seen by any other processes, and the changes made will not be written to the file. 
	   MAP_ANONYMOUS : This flag is used to create an anonymous mapping. Anonymous mapping means the mapping is not connected to any files. This mapping is used as the basic primitive 	
	   to extend the heap. 
	   -1 is the file descriptor and last is the offset. 
	   On success, the mmap() returns 0; for failure, the function returns MAP_FAILED, so we exit.
	 */
	addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		errExit("mmap");

	printf("Address returned by mmap() = %p\n", addr);

	/* [M6]
	 The ioctl operation here Registers the memory address range with uffd. 
	 The range field defines a memory range starting at start (addr given by the mmap output) and for len bytes (total memory allocated) that should be handled by the userfaultfd.
	 UFFDIO_REGISTER_MODE_MISSING tracks page faults on missing pages.
	 */
	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = len;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
		errExit("ioctl-UFFDIO_REGISTER");

	/* [M7]
	 pthread_create() function will start a new thread, here it calls fault_handler_thread() routine here. 
	 passing the pointer to uffd which will be used fault_handler_thread. 
	 In this program, we will create a child thread to poll the fd and check page faults. 
	 So basically, in the program context we are spawning a thread to asynchronously poll the file descriptor and check for page faults in the background.
	 For error, we exit if pthread_create returns -1. 	
	 */
	s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (s != 0) {
		errno = s;
		errExit("pthread_create");
	}

	/*
	 * [U1]
         After creating a thread of fault_handler_thread, mmap function created a memory address in virtual address, but physical memory has nothing.
	 When we try to access addr[l], it causes a page fault. 
	 From c(), since this is the first page fault, the fault_cnt is 0 , 'A' is set to all the memory addresses ( doing 'A'+0 ).
	 l is as 0, 1024, 2048, 3072 and it increments by 1024 everytime and then reading the character at those locations, print the value and set l to the new value.
	 This code prints the addr and then the character stored there and then increments the addr by 1024  
	 The PAGESIZE is set as 4096 ( thus printing out uffdio_copy.copy returned 4096) and while running it for 1 page, we get character A printed 4 times (values of l given above).
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#1. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U2]
	 We are checking the same addr written at addr[0], addr[1024], addr[2048] and addr[3072].
	 Since the addr variable has not changed, we will print the same addr and the letter 'A' again 4 times as above.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#2. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U3]
	 The madvise() system call is used to give advice or directions to the kernel about the address range beginning at address addr and with len bytes. 
	 MADV_DONTNEED implies do not expect access in the near future. For the time being, the application is finished with the given range,
         so the kernel can free resources associated with it. SO kernel frees up the address and clears addr variable. 
         In case of failure, it will report fail to madvise. 
         
	Since addr is cleared, we try to access it again results in page fault. Thus from fault_handler_thread(), fault_cnt is incremented from 0 to 1. 
	Also memset will set 'A' + fault_cnt which is 'B' to all the memory addresses. 
	Similar to what happened before, we have l variable taking values of 0,1024,2048 and 3072
	This code prints the addr and then the character stored there and then increments the addr by 1024 
	Thus we get B printed 4 times. 
	 
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#3. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U4]
	 We are checking the same addr written at addr[0], addr[1024], addr[2048] and addr[3072].
	 Since the addr variable has not changed, we will print the same addr and the letter 'B' again 4 times as above.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#4. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U5]
	 we have another madvise call which again instructs the kernel to free the pages allocated. 
	 Because of the MADV_DONTNEED, the kernel is instructed to not to expect access in the near future.
	 Again a page fult is triggered and fault_handler_thread is called. fault_cnt is incremented to 2 and memory locations are set with 'C'
	 However we call memset again and overwrite the addr by '@' character. So all memory locations will have '@'
	 Similar to what happened before, we have l variable taking values of 0,1024,2048 and 3072
	 This code prints the addr and then the character stored there and then increments the addr by 1024 
	 Thus we get @ printed 4 times. 
	 Since we are writing explicity using memset outside the fault_handler, in the printf we give "write address"
	 */
	printf("-----------------------------------------------------\n");
	if (madvise(addr, len, MADV_DONTNEED)) {
		errExit("fail to madvise");
	}
	l = 0x0;
	while (l < len) {
		memset(addr+l, '@', 1024);
		printf("#5. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U6]
	 We are checking the same addr written at addr[0], addr[1024], addr[2048] and addr[3072].
	 Since the addr variable has not changed, we will print the same addr and the char @ again 4 times as above.
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#6. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	/*
	 * [U7]
	 there is no madvise call. so no freeing up of the memory so no invoking of fault_handler_thread.
	 We have a simple while loop which will run for 4 times. inside we are using memset to have '^' set to memoy locationss. 
	 Similar to what happened before, we have l variable taking values of 0,1024,2048 and 3072
	 This code prints the addr and then the character stored there and then increments the addr by 1024 
	 Thus we get ^ printed 4 times. 
     	 Since we are writing explicity using memset outside the fault_handler, in the printf we give "write address"
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		memset(addr+l, '^', 1024);
		printf("#7. write address %p in main(): ", addr + l);
		printf("%c\n", addr[l]);
		l += 1024;
	}

	/*
	 * [U8]
	 We are checking the same addr written at addr[0], addr[1024], addr[2048] and addr[3072].
	 Since the addr variable has not changed, we will print the same addr and the char ^ again 4 times as above.
	 Post it we exit thhe main function through successful exit. 
	 */
	printf("-----------------------------------------------------\n");
	l = 0x0;
	while (l < len) {
		char c = addr[l];
		printf("#8. Read address %p in main(): ", addr + l);
		printf("%c\n", c);
		l += 1024;
	}

	exit(EXIT_SUCCESS);
}
