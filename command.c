#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
int main (int argc,char ** argv)
{
	int pipe = open ("pipe.txt",O_RDWR);
	int retval = fcntl( pipe, F_SETFL, fcntl(pipe, F_GETFL) | O_NONBLOCK);
	char buf [1000]; // of course BO
	memset (buf,0,1000);
	while (1)
	{
		write (1,"> ",2);
		read (0,buf,1000);

		int size = strlen (buf);
		write (pipe,buf,size);
		memset(buf,0,1000);
	}
	return 0;
}
