#include <unistd.h>
#include <iostream>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdlib.h>

/*
void int_handler(int s)
{
	std::cout << "HANDLER" << std::endl;
	intNum++;
	pipeMutex.lock();
	write (p,"exit\n",0x5);
	pipeMutex.unlock();
	if (intNum >= 2)
	{
		std::cout << " >= 2 ! " << std::endl;
		exit(0xbeef);
	}
}
*/
void shellSession ()
{
	/*
	struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = int_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
	*/

	/*
	std::thread r ([=]()
	{
		char buf [0x1000];
		while (1)
		{
			write (1,"> ",2);
			if (read (0,buf,0x1000) > 0)
			{ 
				// read from user and redirect it to pipe
				pipeMutex.lock();
				std::cout << "Piszemy do pipe" << std::endl;
				write (p,buf,0x1000);  // write and read are atomic to 4 kbytes, later on we need mutex
				memset(buf,0,0x1000);
				pipeMutex.unlock();
			}
		}
	});
	std::thread w ([=]()
	{
		char buf [0x1000];
		while (1)
		{
			std::cout << "0xbeef" << std::endl;
			pipeMutex.lock();
			if (read (p,buf,0x1000) > 0)
			{
				std::cout << buf;
			}
			memset(buf,0,0x1000);
			pipeMutex.unlock();
		}

	});
	r.join();
	w.join();
	*/
}

int main (int argc,char ** argv)
{
	while (1)
	{
		fd_set readfds;
		char buf [0xffff];
	
		struct sockaddr_un addr;
	 	int fd,rc;
	
 		if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	 	{
	 		perror("socket error");
	 		exit (-1);
	 	}
	
 		memset (&addr, 0, sizeof(addr));
	 	addr.sun_family = AF_UNIX;
	 	strncpy(addr.sun_path, "unixsoc", sizeof(addr.sun_path)-1);
	
 		if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
	 	{
	 		perror ("connect error");
	 		exit (-1);
	 	}
	 	std::cout << "[*] Connected" << std::endl;
	
		while (1)
		{
			FD_ZERO(&readfds);
			FD_SET(fd, &readfds);
			FD_SET(0, &readfds);	
			int ret = select(FD_SETSIZE, &readfds, NULL, NULL, NULL); // select unix socket or stdin to read from	
			if (ret > 0)
			{
				if (FD_ISSET(fd, &readfds))
				{
					std::cout << "Cos przyszlo z unix socketa" << std::endl;
					int r = read (fd,buf,0xffff);
					if (r > 0)
					{
						std::cout << buf << std::endl;
					}
					else if (r == -1)
					{
						 perror ("cannot read from unix socket");
						 break;	
					}
					else if (r == 0)
					{
						perror ("unix socket closed connection");
						break;
					}
					memset (buf,0,0xffff);
				}
				else if (FD_ISSET(0, &readfds))
				{
					std::cout << "Wpisalismy cos z palca, wysylam to do unix socketa" << std::endl;
					int r = read (0,buf,0xffff);
					if (write (fd,buf,strlen(buf)) == -1)
					{
						perror ("cannot send data to unix socket");
					}
					memset (buf,0,0x1000);
				}
			}
			else if (ret < 0)
			{
				perror ("error with select");
				// error handling for select errror
			}
		}
	}
	return 0;
}
