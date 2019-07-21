#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <fstream>
#include <mutex>
#include <iostream>
pid_t pid,sid;
std::mutex loggerMutex;
void enable_keepalive(int sock)
{
    int optval;
    socklen_t optlen = sizeof(optval);

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int));
    int idle = 75;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int));
    int interval = 40;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int));
    int maxpkt = 10;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int));
}
void getActualTime (char * buf)
{
	time_t t = time(NULL);
        struct tm tm = *localtime(&t);
	sprintf(buf,"[%d-%d-%d %d:%d:%d]", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}
void logTime (sockaddr_in cli_addr,std::shared_ptr<std::ofstream>&logger)
{
	time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char date [300];
        std::lock_guard<std::mutex> lck(loggerMutex);
  	sprintf(date,"[%d-%d-%d %d:%d:%d][%s] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,inet_ntoa(cli_addr.sin_addr));
        *logger << date;
}
int remotefd = 0;
int  connfd = 0;
int portno = 0;
struct sockaddr_in cli_addr;
char conntime [100];
void remote (std::shared_ptr<std::ofstream> &logger)
{
	while (1)
	{
		unsigned int clilen;
		char buf [256];
		struct sockaddr_in serv_addr;
		remotefd = socket (AF_INET,SOCK_STREAM,0);
		if (remotefd < 0)
		{
			std::lock_guard<std::mutex> lck(loggerMutex);
                        *logger << "Cannot create socket ! \n";
			exit(1);
		}
		bzero ((char *) &serv_addr, sizeof(serv_addr));
		portno = 57932;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons (portno);
		const int       optVal = 1;
		const socklen_t optLen = sizeof(optVal);

		int rtn = setsockopt(remotefd, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen); // problems with binding 
		if (bind (remotefd, (struct sockaddr *) & serv_addr,sizeof(serv_addr)) < 0 )
		{
			std::lock_guard<std::mutex> lck(loggerMutex);
			*logger << "Cannot bind server to port ! \n";
			printf ("bind error %i : %s",errno,strerror(errno));
			exit(2);
		}

		listen(remotefd,1); // block
		clilen = sizeof(cli_addr);
		printf ("Laczymy sie ! \n");
		connfd = accept (remotefd, (struct sockaddr *) &cli_addr, &clilen);
		getActualTime (conntime);
		if (connfd < 0)
		{
			std::lock_guard<std::mutex> lck(loggerMutex);
                        *logger << "Cannot accept ! \n";
			exit(3);
		}
		enable_keepalive(connfd);

		memset (buf,0,256);

		logTime(cli_addr,logger);
		//std::lock_guard<std::mutex> lck(mtx);
		*logger << "Connected \n";

		printf ("Connected !\n");

		while (1)
		{
			if (recv(connfd,buf,256,0) > 0)
			{
				logTime(cli_addr,logger);
				{
					std::lock_guard<std::mutex> lck(loggerMutex);
                        		*logger << buf << std::endl;
				}
				memset (buf,0,256);
			}
			else
			{
				printf ("recv returned <0 !\n");
				close (connfd);
				close (remotefd);
				break;
			}
		}
		printf ("Out of recv loop ! \n");
	}
}
void local(std::shared_ptr<std::ofstream> &logger)
{
	char buf [1000];
	unlink ("pipe.txt");
	if ((mkfifo("pipe.txt",0777)) < 0)
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
        *logger << "Cannot create pipe !\n";
		exit (-1);
	}
	int pipe = open ("pipe.txt",O_RDWR);

	while (1)
	{
		memset (buf,0,1000);
		read (pipe,buf,1000);
		printf ("connfd %i \n",connfd);
		if (connfd != 0)
		{
			logTime(cli_addr,logger);
            *logger << "Command executed : " << buf << std::endl;

			if (strcmp (buf,"time\n") == 0)
			{
				std::lock_guard<std::mutex> lck(loggerMutex);
                *logger << conntime <<std::endl;
				continue;
			}
			if (send (connfd,buf,strlen(buf),0) <= 0)
			{
				std::lock_guard<std::mutex> lck(loggerMutex);
				*logger << "[!]Cannot send command ! \n";
			}
		}
		else
		{
			std::lock_guard<std::mutex> lck(loggerMutex);
			*logger << "[!] You are not connected to any host ! \n";
		}
	}
}
int main (int argc,char ** argv)
{
	pid = fork();
	if (pid < 0)
	{
		exit(-1);
	}
	if (pid > 0)
	{
		printf ("New process : %i\n",pid);
		exit(0);
	}
	umask(0);

	sid = setsid();
	if (sid < 0)
	{
		exit(-2);
	}

	close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);
	memset (conntime,0,100);
	std::shared_ptr<std::ofstream> logger = std::make_shared<std::ofstream>();
	logger->rdbuf()->pubsetbuf(0, 0);
	logger->open("log.txt",std::ios::app);

	logTime(cli_addr,logger);
	*logger <<"Started !\n"; // only one thread no need to use mutex

	std::thread r (remote,std::ref(logger));
	std::thread l (local,std::ref(logger)); // writing log file according to actions
	r.join();
	l.join();

	logTime(cli_addr,logger);
	std::cout << "Closing daemon !" << std::endl;

	logger->close();
	return 0;
}
