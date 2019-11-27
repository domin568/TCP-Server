#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <sys/un.h>
#include <fcntl.h>
#include <mutex>
#include <memory>
#include <dirent.h>
#include <signal.h>
#include <map>
#include <sstream>
#include <algorithm>

const std::string conn ("./connections/");

struct clientData
{
	int remotefd = -1;
	int unixserverfd = -1;
	int unixremotefd = -1;
	std::string filename;
};

std::mutex loggerMutex;
std::shared_ptr<std::ofstream> logger;
std::vector <clientData> clients;

void enable_keepalive(int sock)
{

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int));
    int idle = 75;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int));
    int interval = 40;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int));
    int maxpkt = 10;
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int));
}
const char * getActualTime ()
{
	static char conntime [100];
	time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    memset (conntime,0,sizeof(conntime));
	sprintf(conntime,"[%d-%02d-%02d %02d:%02d:%02d]", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return conntime;
}
void logTime (std::shared_ptr<std::ofstream>&logger)
{
	time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char date [300];
    std::lock_guard<std::mutex> lck(loggerMutex);
  	sprintf(date,"[%d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    *logger << date;
}
int createUnixSoc (std::string name)
{
	unlink (name.c_str());

	int fd = socket (AF_UNIX, SOCK_STREAM, 0);

	struct sockaddr_un addr;
	memset (&addr, 0 ,sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy (addr.sun_path, name.c_str());
	if (bind (fd, (struct sockaddr*) & addr, sizeof(addr)) < 0)
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
		*logger << "[!!!] ";
		logTime(logger);
		*logger << "Cannot bind unix socket " << strerror(errno) << std::endl;
		exit(2);
	}

	if (listen(fd,1) != 0)
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
		*logger << "[!!!] ";
		logTime(logger);
		*logger << "Problem with listen unix socket " << strerror(errno) << std::endl;
	}
	return fd;
}
void remoteHandle ()
{
	std::map <std::string, int> ipsoccurrences;
	unsigned int clilen;
	int portno = 57932;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	int serverfd = socket (AF_INET,SOCK_STREAM,0);
	int clifd {0};
	int maxsd {0};
	fd_set readfds;
	if (serverfd < 0)
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
		*logger << "[!!!] ";
		logTime(logger);
        *logger << "Cannot create socket ! " << strerror(errno) << std::endl;
		exit(1);
	}

	bzero ((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons (portno);
	const int       optVal = 1;
	const socklen_t optLen = sizeof(optVal);
	int rtn = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen); // problems with binding
	if (rtn == -1)
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
		*logger << "[!!!] ";
		logTime(logger);
		*logger << "Cannot set SO_REUSEADDR on socket ! : " << strerror(errno) << std::endl;	
	} 
	if (bind (serverfd, (struct sockaddr *) & serv_addr,sizeof(serv_addr)) < 0 )
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
		*logger << "[!!!] ";
		logTime(logger);
		*logger << "Cannot bind server to port " << portno << " ! : " << strerror(errno) << std::endl;
		exit(2);
	}
	if (listen(serverfd,1) != 0)
	{
		std::lock_guard<std::mutex> lck(loggerMutex);
		*logger << "[!!!] ";
		logTime(logger);
		*logger << "Problem with listen for new connetions " << strerror(errno) << std::endl;
	}
	clilen = sizeof(cli_addr);

	while (1)
	{
		clientData client;

		FD_ZERO (&readfds);

		FD_SET (serverfd,&readfds);

		maxsd = serverfd; // to make 'select' life easier
		int i = 0;
		for (auto const & s : clients) // add socket descriptors to be checked by select
		{
			//std::cout << "i = " << i << std::endl;
			FD_SET (s.remotefd, &readfds);
			FD_SET (s.unixserverfd, &readfds);
			//std::cout << "s.remotefd = " << s.remotefd << std::endl;
			//std::cout << "s.unixserverfd = " << s.unixserverfd << std::endl;
			//std::cout << "s.unixremotefd = " << s.unixremotefd << std::endl;
			//std::cout << "s.filename = " << s.filename << std::endl;
			if (s.unixremotefd != -1)
			{
				FD_SET(s.unixremotefd,&readfds);
			}
			
			if (s.remotefd > maxsd)
			{
				maxsd = s.remotefd;
			}
			if (s.unixserverfd > maxsd)
			{
				maxsd = s.unixserverfd;
			}
			if (s.unixremotefd > maxsd)
			{
				maxsd = s.unixremotefd;
			}
			i++;
		}
		int ret = select (maxsd + 1, &readfds, NULL , NULL, NULL);
		*logger << "SELECT RETURNED : " << ret << std::endl;
		if (ret < 0)
		{
			std::lock_guard<std::mutex> lck(loggerMutex);
			*logger << "[!!!] ";
			logTime(logger);
			*logger << "Problem with select " << strerror(errno) << std::endl;
		}

		if (FD_ISSET(serverfd,&readfds))
		{
			if ((clifd = accept (serverfd, (struct sockaddr *) &cli_addr, &clilen)) == -1)
			{
				std::lock_guard<std::mutex> lck(loggerMutex);
				*logger << "[!!!] ";
				logTime(logger);
				*logger << "Problem with accepting new connection " << strerror(errno) << std::endl;
			}
			client.remotefd = clifd;
			//enable_keepalive(clifd);

			std::string actualIP (inet_ntoa(cli_addr.sin_addr));

			if (ipsoccurrences.find(actualIP) == ipsoccurrences.end())
			{
				ipsoccurrences[actualIP] = 1;
			}
			else 
			{
				ipsoccurrences[actualIP]++;
			}

			{
				std::lock_guard<std::mutex> lck(loggerMutex);
				*logger << "[*] ";
				logTime(logger);
				*logger << "New connection from " << actualIP << " [" << ipsoccurrences[actualIP] <<"]" << std::endl;
			}
			struct stat st = {0};
			if (stat(conn.c_str(), &st) == -1) 
			{
    			mkdir(conn.c_str(), 0700);
			}

			std::stringstream stream;
			stream << " [" << ipsoccurrences[actualIP] << "] ";
			
			client.filename = actualIP;
			client.filename.append(stream.str());
			std::string filepath = conn + client.filename;

			int unixsocfd = createUnixSoc(filepath);
			client.unixserverfd = unixsocfd;
			clients.push_back(client);
		}
		int clientsIt = 0 ;
		for (auto & s : clients)
		{
			char buf [0xffff];
			if (FD_ISSET(s.remotefd,&readfds))
			{
				int ret = recv(s.remotefd,buf,0xffff,0);
				if (ret == -1)
				{
					std::lock_guard<std::mutex> lck(loggerMutex);
					*logger << "[!!!] ";
					logTime(logger);
					*logger << "Problem with receiving data from client " << strerror(errno) << std::endl;	
				}
				else if (ret == 0)
				{
					{
						std::lock_guard<std::mutex> lck(loggerMutex);
						*logger << "[!] ";
						logTime(logger);
						*logger << s.filename << " disconnected !" << std::endl;	
					}
					std::string filepath = conn + s.filename;
					int idx = s.filename.find_first_of(" ");
					remove (filepath.c_str());
					s.filename.resize(idx);
					ipsoccurrences[s.filename]--;
					close(s.remotefd);
					close(s.unixserverfd);
					close(s.unixremotefd);
					clients.erase(std::begin(clients) + clientsIt);
				}
				else if (s.unixremotefd != -1)
				{
					if (write(s.unixremotefd,buf,strlen(buf)) == -1)
					{
						std::lock_guard<std::mutex> lck(loggerMutex);
						*logger << "[!!!] ";
						logTime(logger);
						*logger << "Problem with writing to pipe remote socket descriptor " << strerror(errno) << std::endl;		
					}
				}
				
				memset (buf,0,0xffff);
			}
			else if (FD_ISSET (s.unixserverfd, &readfds))
			{
				int unixremote;
				if ((unixremote = accept (s.unixserverfd, (struct sockaddr *) &cli_addr, &clilen)) == -1)
				{
					std::lock_guard<std::mutex> lck(loggerMutex);
					*logger << "[!!!] ";
					logTime(logger);
					*logger << "Problem with accepting new unix socket " << strerror(errno) << std::endl;
				}
				//std::cout << "Unix remote socket : " << unixremote << std::endl;
				s.unixremotefd = unixremote;
			}
			else if (FD_ISSET(s.unixremotefd, &readfds))
			{
				int ret = read(s.unixremotefd,buf,0xffff);
				if (ret == -1)
				{
					std::lock_guard<std::mutex> lck(loggerMutex);
					*logger << "[!!!] ";
					logTime(logger);
					*logger << "Cannot read from remote unix socket " << strerror(errno) << std::endl;
				}
				else if (ret == 0)
				{
					std::lock_guard<std::mutex> lck(loggerMutex);
					*logger << "[*] ";
					logTime(logger);
					*logger << "Command terminal disconnected" << std::endl;
					close (s.unixremotefd);
					s.unixremotefd = -1;
					FD_CLR(s.unixremotefd,&readfds);
				}
				if (send(s.remotefd,buf,ret,0) == -1)
				{
					std::lock_guard<std::mutex> lck(loggerMutex);
					*logger << "[!!!] ";
					logTime(logger);
					*logger << "Cannot send to remote socket " << strerror(errno) << std::endl;
				}
				memset(buf,0,0xffff);
			}

			clientsIt++;
		}
	}
}
void flushConnections ()
{
	DIR *d;
    struct dirent *dir;
    d = opendir(conn.c_str());
    if (d)
    {
       	while ((dir = readdir(d)) != NULL)
       	{
       		std::string filepath;
       		std::string dirstr (dir->d_name);
       		filepath = conn + dirstr;
			remove(filepath.c_str());
        }
        closedir(d);
    }
}
int main (int argc,char ** argv)
{
	
	int pid = fork();
	if (pid < 0)
	{
		exit(-1);
	}
	if (pid > 0)
	{
		printf ("[*] New process : %i\n",pid);
		exit(0);
	}

	umask(0);

	int sid = setsid();
	if (sid < 0)
	{
		exit(-2);
	}

	if ((chdir("/home/pi/Daemons")) < 0)
	{
		exit (-3);
	}

	close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);
	
	// remove old not valid connections
	
	flushConnections();
	logger = std::make_shared<std::ofstream>();
	logger->rdbuf()->pubsetbuf(0, 0);
	logger->open("/home/pi/Daemons/log.txt",std::ios::app);
	*logger << "[*] ";
	logTime(logger);
	*logger <<"Started !\n"; // only one thread no need to use mutex

	remoteHandle ();

	logger->close();
	return 0;
}