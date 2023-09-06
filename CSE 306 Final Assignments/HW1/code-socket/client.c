#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 5984
#define BUFF_SIZE 4096

int main(int argc, const char *argv[])
{
	int sock = 0;
	struct sockaddr_in serv_addr;
	char *hello = "Hello from client";
	char buffer[BUFF_SIZE] = {0};

	/* [C1]
	 * Explaint the following here.
	 This is the socket creation line. The AF_INET is the domain used for communicating which is IPv4 in this case. The second argument is the type in which SOCK_STREAM stands 	 for TCP (Transmission control protocol). The third argument is the protocol which is 0 here which implies Internet Protocol (IP). The LHS is the socket descriptor which is an integer. So, we are printing out an error if that value is less than 0.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	/* [C2]
	 * Explaint the following here.
	 We are setting the values for the address struct defined at the top. The protocol, addr (the IP addr) and port are defined as below. 
	 */
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	/* [C3]
	 * Explaint the following here.
	 We are converting the addresses from text to binary form. if they are invalid, we throw an error. 
	 */
	if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	/* [C4]
	 * Explaint the following here.
	 The connect function calls the descriptor defined above with the values of the addr struct defined above. If the function returns an error, we throw an error mssg. 
	 */
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}


	/* [C5]
	 * Explaint the following here.
	 We will type any key to continue and it will wait for our input. 
	 */
	printf("Press any key to continue...\n");
	getchar();

	/* [C6]
	 * Explaint the following here.
	 We are sending the message stored in hello defined at the top. the length of it is given as strlen(hello). if it is successful, we will print message sent message. 
	 */
	send(sock , hello , strlen(hello) , 0 );
	printf("Hello message sent\n");

	/* [C7]
	 * Explaint the following here.
	 The read will read data from the sock descriptor and store it in the buffer. If data is not available, it will return -1 and we will throw an error. Then the data from the buffer is printed as mssg from server. 
	 */
	if (read( sock , buffer, 1024) < 0) {
		printf("\nRead Failed \n");
		return -1;
    }
	printf("Message from a server: %s\n",buffer );
	return 0;
}
