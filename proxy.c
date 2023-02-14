#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>

/*******************************************************************************************   CPSC 441 Assignment 01 ****************************************************************************************
 * DESCRIPTION: This program uses basic socket functionality in C to create a proxy server that can dynamically censor HTTP webpages using a keyword that it matches to the header of the webpage. The user
 *              Specifies the portnumber as the second command line argument when running the program. It uses the HTTP GET method to fetch HTTP webpages from the server and sends them back to the client.
 *              
 *              AUTHOR: Jarin Thundathil
 *              DATE: October 02, 2021
*/

// DEFINE CONSTANT VALUES
#define TRUE 1                      // Boolean
#define FALSE 0                     // Boolean
#define LOWPORT 10000               // The lowest port number allowed
#define HIGHPORT 99999              // The highest port number allowed
#define COMMAND_PORT 8688           // Mandatory command port number
#define SERVERPORT 80               // Port to connect proxy to the server
#define BLOCKLISTSIZE 5             // The maximum size of the list of items to block
#define BUFFERSIZE 4096             // Standard memory allocation for individual messages
#define BIGBUFFER 65536             // Memory allocation for largest message sizes
#define STRINGSIZE 32               // Size of strings for the block list

// GLOBAL VARIABLES
int forkedSocket, tempSocket; // CAN YOU MAKE THESE INSTANCE VARIABLES??

// MAIN
int main(int argc, char *argv[])
{
    // DECLARE LOCAL VARIABLES
    int i,j,k;                                      // Some counters
    int port;                                       // The port you are connecting your proxy on
    int hostSocket;                                 // Main listening socket
    int commandSocket;                              // Socket listening on command port for settings changes
    int serverSocket;                               // Socket that will connect proxy to the server
    int commandReceived = FALSE;                    // An indicator variable that tells the program whether a command has been received on the command socket
    char blockrequest[BUFFERSIZE];                  // A char to hold any requests from the command port to block a new keyword
    int incommand;                                  // int to hold data from command port receives.
    char errorPage[BUFFERSIZE];                     // String that will be used for user redirect to error.html page
    char clientHTTPrequest[BUFFERSIZE];             // Stores the HTTP request from the client received on the child socket
    int message;                                    // Value of the message received from the client
    char blockedArray[BLOCKLISTSIZE][STRINGSIZE];   // 2D array to hold the blocked content set on the command socket
    int blockedWords = 0;                           // Number of words already in the array of blocked words
    int newProcess;                                 // Where the process ID is stored
    char FIRSTHALF[BUFFERSIZE];                     // Host name of client GET request
    char SECONDHALF[BUFFERSIZE];                    // Path name of client GET request
    char get[256], URL[256], version[256];          // Variables for storing path name for HTTP request
    int censored;                                   // Boolean to flag whether an object is censored or not
    char GETrequest[BUFFERSIZE];                    // The get request sent to the HTTP server
    int dataStream;                                 // Stream of incoming data from the server after sending it the GET request
    char HTTPResponse[BUFFERSIZE];                  // Holds the HTTP response message from the server


    //Revision for second half when needed.
    strcpy(errorPage, "/~carey/CPSC441/ass1/error.html");

    // PARSE COMMAND LINE ARGUMENTS
    if (argc < 2 || argc > 2)
    {
        printf("Usage: ./proxy <desired_port_number>\n");
        exit(-1);
    } else {
        port = 0;
        i = 0;
        while(argv[1][i] != '\0'){
            if (isdigit(argv[1][i])) {
                port = port*10 + (argv[1][i] - '0');
                i = (i + 1);
            } else {
                printf("Your port is not an integer, see user doc for correct usage.\n");
                exit(-1);
            }
        }
        // CHECK THAT PORT IS IN CORRECT RANGE
        if (port < LOWPORT || port > HIGHPORT)
        {
            printf("Your port number must be between 10000 and 99999\n");
            exit(-1);
        }
    }

    // INITIALIZE THE CONNECTION SOCKET
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;


    // CREATE, BIND & LISTEN HOST SOCKET
    hostSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (hostSocket == -1){
        perror("Could not create a host socket!\n");
        exit(-1);
    }

    if (bind(hostSocket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Could not bind the host socket\n");
        exit(-1);
    }

    if(listen(hostSocket, 5) == -1){
        perror("Unable to listen() with host socket\n");
        exit(-1);
    }


    // INITIALIZE THE CONTROL SOCKET
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(COMMAND_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    //CREATE, BRIND & LISTEN COMMAND SOCKET
    if ((commandSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Could not create a command socket!\n");
        exit(-1);
    }

    if (bind(commandSocket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Could not bind the command socket.\n");
        exit(-1);
    }

    if(listen(commandSocket, 5) == -1){
        perror("Unable to listen() with command socket\n");
        exit(-1);
    } else {
        printf("Please connect command socket on port %d\n", COMMAND_PORT);

        if ((tempSocket = accept(commandSocket, NULL, NULL )) == -1)
        {
            perror("Could not accept() connection to command socket!\n");
        } else {
            printf("Connected to command port!\n");
            commandReceived = TRUE;

            // NOW THAT WE ARE CONNECTED TO COMMAND PORT - ENSURE IT TIMES OUT IF NO COMMANDS ARE RECEIVED
            struct timeval commandTimeout;
            commandTimeout.tv_sec = 1;
            commandTimeout.tv_usec = 0;

            if (setsockopt (tempSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&commandTimeout, sizeof(commandTimeout)) < 0)
            {
                perror("Could not set timeout for command socket.\n");
            }
        }

        while(1){
            // IF THE COMMAND PORT HAS CONNECTED CHECK FOR NEW COMMANDS
            if (commandReceived){
                printf("Checking the command socket for new instructions.\n");
                //READ THIS COMMAND (IF RECEIVED) IN THE TEMPORARY SOCKET
                if (incommand = recv(tempSocket, blockrequest, BUFFERSIZE, 0) > 0) {
                    printf("A new block request has been found! Processing.\n");
                    //CHECK FOR 'BLOCK' or 'UNBLOCK' COMMANDS HERE
                    if (blockrequest[0] == 'B' && blockrequest[1] == 'L' && blockrequest[2] == 'O' && blockrequest[3] == 'C' && blockrequest[4] == 'K'){
                        if (blockedWords < BLOCKLISTSIZE){
                            j = 0;
                            while(blockrequest[j+6] != '\n' && blockrequest[j+6] != '\r'){
                                j = (j + 1);
                                blockedArray[blockedWords][j-1] = blockrequest[j+5];
                            }
                            blockedArray[blockedWords][j] = '\0';
                            blockedWords = (blockedWords + 1);
                            printf("Just added '%s' to the list of blocked keywords. There are %d keywords in the list\n", &blockedArray[blockedWords-1][0], blockedWords);
                        } else {
                            printf("You can only block 5 items at a time! Use 'UNBLOCK' to free up the block list\n");
                        }
                    }
                    if (blockrequest[0] == 'U' && blockrequest[1] == 'N' && blockrequest[2] == 'B' && blockrequest[3] == 'L' && blockrequest[4] == 'O' && blockrequest[5] == 'C' && blockrequest[6] == 'K'){
                        if (blockedWords > 0){
                            blockedWords = (blockedWords - 1);
                            printf("Unblocked the last keyword in the list. Your list of blocked keywords now has %d entries\n", blockedWords);
                        }
                        if (blockedWords == 0){
                            printf("Your blocked keyword list is empty.\n");
                        }
                        if (blockedWords < 0){
                            printf("That shouldn't be possible...\n");
                        }
                    } else {
                        printf("Unrecognized command. Usage: 'BLOCK <keyowrd_to_block>' or 'UNBLOCK'\n");
                    }
                }
            }

            printf("Listening on port %d\n", port);

            // ACCEPT ANY INCOMING CONNECTION ON FORK SOCKET
            if ((forkedSocket = accept(hostSocket, NULL, NULL)) == -1) {
                perror("could not accept() .\n");
                exit(-1);
            }

            // FORK AND ACCEPT NEW PROCESS SO HOST SOCKET CAN CONTINUE LISTENING
            newProcess = fork();

            // THREE FORK CASES
            if (newProcess < 0){
                //case 1: Fork failed
                printf("Fork was not successful\n");
            }
            if (newProcess > 0){
                //case 2: Returned to parent or caller. The value contains process ID of newly created child process.
                close(forkedSocket);
                printf("Fork created and nothing was there - returning to host socket\n");
            }
            if (newProcess == 0){
                //case 3: Returned to the newly created child process.
                close(hostSocket);

                while ((message = recv(forkedSocket, clientHTTPrequest, BUFFERSIZE, 0)) > 0){
                    printf("Client says: %s\n", clientHTTPrequest);

                    // ISOLATE THE GET REQUEST FROM THE CLIENT
                    char *path = strtok(clientHTTPrequest, "\r\n");
                    printf("Received HTTP request: %s\n", path);
                    if (sscanf(path, "%s http://%s %s", get, URL, version) == 3) {

                        printf("The URL is: %s\n", URL);
                        printf("The command is: %s\n", get);
                        printf("The version is: %s\n", version);

                        // ISOLATE THE HOST NAME FROM THE URL AND STORE IN THE FIRST HALF
                        for (i = 0; i < strlen(URL); i++) {
                            if (URL[i] == '/') {
                                strncpy(FIRSTHALF, URL, i); //copy out hostname
                                FIRSTHALF[i] = '\0';
                                break;
                            }
                        }

                        //THIS IS WHERE WE CHECK AGAINST THE LIST OF CENSORED ITEMS
                        printf("Checking if URL needs to be censored.\n");
                        censored = FALSE;
                        if (blockedWords > 0) {
                            for (j = 0; j < blockedWords; j++)
                            {
                                int lenkeyword = strlen(&blockedArray[j][0]);
                                int matchlen = 0;
                                int cursor = 0;
                                for (int k = i; k < strlen(URL); k++) {
                                    if (URL[k] == blockedArray[j][cursor]) {
                                        matchlen = (matchlen + 1);
                                        cursor = (cursor + 1);
                                    } else {
                                        matchlen = 0;
                                        cursor = 0;
                                    }
                                    if (matchlen == lenkeyword) {
                                        censored = TRUE;
                                        break;
                                    }
                                }
                            }
                        } else {
                            printf("No keywords in list to block right now\n");
                        }
                        
                        // COPY PATHNAME FROM THE URL AND STORE IN SECOND HALF
                        bzero(SECONDHALF, 500);
                        for (; i < strlen(URL); i++)
                        {
                            strcat(SECONDHALF, &URL[i]);
                            break;
                        }

                        //REVISE SECOND HALF IF CENSORED
                        if(!censored) {
                            printf("This is G rated content!\n");
                        }
                        if (censored) {
                            printf("This URL will need to be censored.\n");
                            strcpy(SECONDHALF, errorPage);
                            printf("Will redirect to %s\n", &errorPage);
                        } else {
                            printf("This is G rated content!\n");
                        }

                        printf("The host name is: %s\n", FIRSTHALF);
                        printf("The path name is: %s\n", SECONDHALF);

                        // CONNECT TO THE WEB SERVER SEND YOUR GET REQUEST
                        struct sockaddr_in server_addr;
                        struct hostent *server;
                        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
                        if (serverSocket < 0) {
                            printf("Failed to open server socket.\n");
                        } else {
                            printf("Opened server socket!\n");
                        }
                        server = gethostbyname(FIRSTHALF);
                        if (server == NULL) {
                            printf("Failed to fetch web server host\n");
                        } else {
                            printf("Web server = %s\n", server->h_name);
                        }
                        bzero((char *) &server_addr, sizeof(server_addr));
                        server_addr.sin_family = AF_INET;
                        bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
                        server_addr.sin_port = htons(SERVERPORT);
                        
                        if (connect(serverSocket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
                            perror("Could not connect to server socket\n");
                        } else {
                            printf("Connected to server socket!\n");
                        }

                        //SEND HTTP GET REQUEST TO SERVER
                        if (serverSocket > 0) {
                            bzero(GETrequest, BUFFERSIZE);
                            sprintf(GETrequest, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", SECONDHALF, FIRSTHALF);
                            if (send(serverSocket, GETrequest, strlen(GETrequest), 0) < 0) {
                                perror("Could not send HTTP request\n");
                            } else {
                                printf("The generated GET request looks like:\n%s\n", GETrequest);
                                printf("GET request sent to host %s, awaiting response!\n", FIRSTHALF);
                            }
                        } else {
                            printf("Something went wrong, could not send HTTP GET request\n");
                        }

                        //HANDLE HTTP RESPONSE FROM SERVER
                        while ((dataStream = read(serverSocket, HTTPResponse, BUFFERSIZE)) > 0 ){
                            
                            //SEND IT BACK TO THE CLIENT FOR WEBPAGE DISPLAY
                            printf("Got %d bytes of information from the server\n", dataStream);
                            send(forkedSocket, HTTPResponse, dataStream, 0);
                            printf("Sent that response back to the client!\n");
                        }
                        bzero(HTTPResponse, BUFFERSIZE);
                    }
                }
                close(forkedSocket);
                exit(0);
            }
        }
    }
    return 0;
}