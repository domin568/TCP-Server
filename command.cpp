#include <unistd.h>
#include <iostream>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <dirent.h>
#include <vector>

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
int main (int argc,char ** argv)
{

	bool targetSelected = false;
	while (1)
	{
		std::vector <char *> connections;
		std::cout << "\n\033[22;36m[*] Please select active connection: \033[0m" << std::endl;
		DIR *d;
    	struct dirent *dir;
    	d = opendir("./connections");
    	if (d)
    	{
    		int it = 0;
       	 	while ((dir = readdir(d)) != NULL)
       		{
       			if (strcmp(dir->d_name,".") != 0 && strcmp(dir->d_name,"..") != 0)
       			{
            		std::cout << "\033[22;36m[" << it << "]\033[0m " << dir->d_name << std::endl;
            		connections.push_back(dir->d_name);
            		it++;
            	}
        	}
        	closedir(d);
    	}
    	int selected;
    	std::cin >> selected;
    	if (std::cin.good())
    	{
    		targetSelected = true;
    	}
		while (targetSelected)
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
		 	char filepath [100] {0};
			strcpy (filepath,"./connections/");
			strcpy (filepath+14,connections[selected]);
		 	strncpy(addr.sun_path, filepath, sizeof(addr.sun_path)-1);
		
	 		if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
		 	{
		 		perror ("connect error");
		 		exit (-1);
		 	}
		 	std::cout << "[*] Connected with " << connections[selected] << std::endl;
		
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
						int r = read (fd,buf,0xffff);
						if (r > 0)
						{
							std::cout << buf;
						}
						else if (r == -1)
						{
							 perror ("cannot read from unix socket");
							 break;	
						}
						else if (r == 0)
						{
							std::cout << "[!] Connection closed by remote host" << std::endl;
							targetSelected = false;
							break;
						}
						memset (buf,0,0xffff);
					}
					else if (FD_ISSET(0, &readfds))
					{
						int r = read (0,buf,0xffff);
						if (write (fd,buf,strlen(buf)) == -1)
						{
							perror ("cannot send data to unix socket");
						}
						memset (buf,0,0xffff);
					}
				}
				else if (ret < 0)
				{
					perror ("error with select");
					// error handling for select errror
				}
			}
		}
	}
	return 0;
}
