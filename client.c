#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIFO_READ "myReadFifo"
#define FIFO_WRITE "mySendFifo"

int main()
{
    char s[300];
    int num, fd[2];
    char input[100];
    char received_message[300]; // Buffer to store the received message

    mkfifo(FIFO_READ, 0666);
    mkfifo(FIFO_WRITE, 0666);

    printf("Waiting for readers...\n");
    fd[1] = open(FIFO_WRITE, O_WRONLY); // Open for writing
    fd[0] = open(FIFO_READ, O_RDONLY);  // Open for reading
    printf("Got a reader....Enter something:\n");

    while (1)
    {
        printf("Enter your command: ");
        fgets(s, sizeof(s), stdin);
        if ((num = write(fd[1], s, strlen(s))) == -1)
        {
            perror("Error writing to FIFO!");
        }
        else
        {

            if ((num = read(fd[0], received_message, sizeof(received_message))) == -1)
            {
                perror("Error reading from FIFO!");
            }
            else
            {
                received_message[num] = '\0';
                printf("Received message: %s", received_message);
            }
        }
    }

    close(fd[0]);
    close(fd[1]);

    return 0;
}
