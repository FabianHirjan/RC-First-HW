#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <utmpx.h>

#define FIFO_READ "myReadFifo"
#define FIFO_WRITE "mySendFifo"
#define FORKERROR "FORK ERROR\n"

int checkLogin(char user[100], char password[100])
{
    FILE *file;
    char file_user[100], file_password[100];

    int found = 0;

    file = fopen("credentials.txt", "r");
    if (file == NULL)
    {
        printf("Could not open file");
        return 0;
    }

    while (fscanf(file, "%s %s", file_user, file_password) != EOF)
    {
        if (strcmp(user, file_user) == 0 && strcmp(password, file_password) == 0)
        {
            found = 1;
            break;
        }
    }
    fclose(file);

    return found;
}

int main()
{
    char s[300];
    int num, fd[2];
    char user[100], password[100];
    int loggedIn = 0;
    char received_message[300];

    mkfifo(FIFO_READ, 0666);
    mkfifo(FIFO_WRITE, 0666);

    printf("Waiting for someone to write...\n");
    fd[0] = open(FIFO_WRITE, O_RDONLY); // Open for reading
    fd[1] = open(FIFO_READ, O_WRONLY);  // Open for writing
    printf("Someone has arrived:\n");

    do
    {
        if ((num = read(fd[0], s, sizeof(s))) == -1)
            perror("Error reading from FIFO!");
        else if (num > 0)
        {
            s[num] = '\0';
            // printf("Read %d bytes from FIFO: %s \n", num, s);
            if (strcmp(s, "exit\n") == 0)
                break;
            pid_t pid = fork();
            if (pid == 0)
            {
                // Child process
                // process_command(s);
                printf("Command received: %s", s);
                fflush(stdout);

                if (strcmp(s, "login\n") == 0)
                {

                    pid_t child = fork();
                    if (child == 0)
                    {
                        write(fd[1], "Enter username: ", strlen("Enter username: "));
                        read(fd[0], user, sizeof(user));
                        user[strcspn(user, "\n")] = 0; // scoatem newline
                        write(fd[1], "Enter password: ", strlen("Enter password: "));
                        read(fd[0], password, sizeof(password));
                        password[strcspn(password, "\n")] = 0; // rm newline

                        // Check login
                        if (checkLogin(user, password))
                        {
                            loggedIn = 1;
                            printf("User %s logged in\n", user);
                            write(fd[1], "Login successful!\n", strlen("Login successful!\n"));
                        }
                        else
                        {
                            write(fd[1], "Login failed!\n", strlen("Login failed!\n"));
                        }
                    }
                    else if (child > 0)
                    {
                        wait(NULL);
                    }
                    else
                    {
                        perror(FORKERROR);
                    }
                }
                else if (strcmp(s, "get-logged-users\n") == 0)
                {
                    pid_t child = fork();

                    if (child == 0)
                    {
                        // Child process
                        struct utmpx *entry;
                        setutxent();
                        char raspuns[1000] = "Useri Conectati:\n"; // Fixed typo

                        while ((entry = getutxent()) != NULL)
                        {
                            if (entry->ut_type == USER_PROCESS)
                            {
                                printf("User: %s\n", entry->ut_user);
                                printf("Host: %s\n", entry->ut_host);
                                strcat(raspuns, entry->ut_user);
                                strcat(raspuns, " ");
                                strcat(raspuns, entry->ut_host);
                                strcat(raspuns, "\n");
                            }
                        }

                        endutxent();
                        // Write the response to the client
                        write(fd[1], raspuns, strlen(raspuns));

                        exit(0);
                    }
                    else if (child > 0)
                    {
                        // Parent process
                        // Optionally, you can wait for the child process to complete
                        wait(NULL);
                    }
                    else
                    {
                        // Error handling for fork
                        perror(FORKERROR);
                    }
                }

                else if (strcmp(s, "get-proc-info") == 0)
                {
                    int myPipe[2];
                    if (pipe(myPipe) < 0)
                    {
                        perror("Pipe error");
                        exit(1);
                    }

                    pid_t child = fork();

                    if (child > 0)
                    {
                        close(myPipe[1]);

                        char aux[256];
                        int auxL;
                        auxL = read(myPipe[0], aux, 256);

                        aux[auxL] = '\0';

                        write(fd[1], aux, auxL);
                    }
                    else if (child == 0)
                    {
                        close(myPipe[0]);

                        char *childComanda = strtok(s, " ");
                        for (int i = 1; i <= 2; i++)
                        {
                            childComanda = strtok(NULL, " \n");
                        }

                        char sursa[50];
                        snprintf(sursa, sizeof(sursa), "/proc/%s/status", childComanda);

                        FILE *fp = fopen(sursa, "r");
                        if (fp == NULL)
                        {
                            perror("Error opening file");
                            exit(1);
                        }

                        char line[256];
                        while (fgets(line, sizeof(line), fp) != NULL)
                        {
                            if (strstr(line, "Name") || strstr(line, "State") || strstr(line, "Ppid") || strstr(line, "Vmsize") || strstr(line, "Uid"))
                            {
                                write(myPipe[1], line, strlen(line));
                            }
                        }
                        fclose(fp);
                        exit(0);
                    }
                    else
                    {
                        perror("Fork error");
                    }
                }

                else if (strcmp(s, "logout\n") == 0)
                {
                    pid_t child = fork();
                    if (child == 0)
                    {
                        loggedIn = 0;
                        write(fd[1], "Logout successful!\n", strlen("Logout successful!\n"));
                        printf("A user logged out\n");
                    }
                    else if (child > 0)
                    {
                        wait(NULL);
                    }
                    else
                    {
                        perror(FORKERROR);
                    }
                }
                else if (strcmp(s, "exit\n") == 0)
                {
                    write(fd[1], "Exiting...\n", strlen("Exiting...\n"));
                    break;
                }
                else
                {
                    write(fd[1], "Invalid command!\n", strlen("Invalid command!\n"));
                }
                exit(0); // Child process exits after completing its task
            }
            else if (pid > 0)
            {
                // Parent process
                // Optionally, you can wait for the child process to complete
                wait(NULL);
            }
            else
            {
                // Error handling for fork
                perror(FORKERROR);
            }
        }
    } while (num > 0);

    close(fd[0]);
    close(fd[1]);

    return 0;
}
