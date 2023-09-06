#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);
	char buffer[BUFF_SIZE] = {0};
	char *hello = "Hello from server";

	/* [S1]
	 * Explaint the following here.
	 This is the socket creation line. The AF_INET is the domain used for communicating which is IPv4 in this case. The second argument is the type in which SOCK_STREAM stands 	 for TCP (Transmission control protocol). The third argument is the protocol which is 0 here which implies Internet Protocol (IP). The LHS is the socket descriptor which is an integer. So, we are printing out an error if that value is less than 0.
	 */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	/* [S2]
	 * Explaint the following here.
	 setsockopt() is a function which sets options as given by the opt variable. The options SO_REUSEADDR and SO_REUSEPORT help in reuse of local addr and port. This is an optional function. If unsuccessful, we will print out an error mssg in this step. 
	 */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
		       &opt, sizeof(opt))) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	/* [S3]
	 * Explaint the following here.
	 We are setting the values for the address struct defined at the top. The protocol, addr (the IP addr) and port are defined as below. 
	 */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );

	/* [S4]
	 * Explaint the following here.
	 The bind function is used to "bind" (hence the name) the socket descriptor to the addr and the port defined in the S3. If unsuccessful, we will print out an error mssg in this step. 
	 */
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	/* [S5]
	 * Explaint the following here.
	 In this, the server is waiting for the client to make contact and establish a connection. The 3 is the maximum backlog length. If unsuccessful, we will print out error mssg in this step. 
	 */
	if (listen(server_fd, 3) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* [S6]
	 * Explaint the following here.
	 It takes the first requested connection request for listening based on the server file descriptor and the address. Here we have a successful connection between the server and client. If unsuccessful, we will print out error mssg in this step. 
	 */
	if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
				 (socklen_t*)&addrlen)) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	/* [S7]
	 * Explaint the following here.
	 We will type any key to continue and it will wait for our input. 
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [S8]
	 * Explaint the following here.
	 The read will read data from the new_socket and store it in the buffer. If data is not available, it will return -1 and we will throw an error. Then the data from the buffer is printed as mssg from client. 
	 */
	if (read( new_socket , buffer, 1024) < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	printf("Message from a client: %s\n",buffer );

	/* [S9]
	 * Explaint the following here.
	 We are sending the message stored in hello defined at the top. the length of it is given as strlen(hello). if it is successful, we will print message sent message. and exit the server program. 
	 */
	send(new_socket , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");
	return 0;
}
