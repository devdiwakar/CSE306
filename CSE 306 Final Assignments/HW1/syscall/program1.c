#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>


int main(int argc, char* argv[])
{
   
   int x;
   int key;
   char *string;
   while( (x = getopt (argc,argv,"s:k:") ) != -1)
   {
   	switch(x)
   	{
   		case('s'):
   			  string = optarg;
   		          break;
   		case('k'):
   			  key = atoi(optarg);
   			  break;
     	}
   }
   
   syscall(449,string,key);
   //printf(string, key);
}
