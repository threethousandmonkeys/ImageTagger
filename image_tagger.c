/*
    This source code is created and compiled for 2019 Semester 1 COMP30023
    Project 1.
    Author:         Qini Zhang
    Student Number: 901051
    Login Name:     qiniz
    Date:           April, 2019
*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*----------------------------------Macros------------------------------------*/
#define BUFFER_SIZE 2049
#define MAX_PLAYER 2
#define LIST_LEN 50
#define WORD_LEN 30 + 1
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\n\
Content-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\n\
Content-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;
static char const * const HTTP_200_FORMAT_WITH_COOKIE = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\
Set-Cookie: %s\r\n\r\n";
static int PAGE_NUM = 1;
/*----------------------------------------------------------------------------*/

/*------------------------------Data Structures-------------------------------*/
// represents the types of method
typedef enum{
    GET,
    POST,
    UNKNOWN
} METHOD;

// Four states for this game
typedef enum{
    NOT_READY,
    READY,
    QUIT,
    WIN
} STATE;

typedef struct{
    int palyer_1_socket;
    int palyer_2_socket;
}socket_t;
/*----------------------------------------------------------------------------*/

// add guess word into wordlist
void add_into_wordlist(char wordlist[LIST_LEN][WORD_LEN], char* keyword){
    for(int i = 0; i < LIST_LEN; i++){
        if(wordlist[i][0]=='\0'){
            strcpy(wordlist[i],keyword);
            printf("%s\n", wordlist[i]);
            return;
        }
    }
}

// init wordlist for player
void init_wordlist(char wordlist[MAX_PLAYER][LIST_LEN][WORD_LEN]){
    int i, j;
    for (i = 0; i < MAX_PLAYER; i++){
        for(j = 0; j < LIST_LEN; j++){
            wordlist[i][j][0] = '\0';
        }
    }
}

// convert wordlist to a word
char* to_list(char wordlist[LIST_LEN][WORD_LEN]){
    int length = 0;
    int i;
    for(i=0; wordlist[i][0] != '\0'|| i<LIST_LEN; i++){
        length += strlen(wordlist[i]);
    }
    char *words = (char*) malloc(2 * i + length + 1);
    strcat(words, wordlist[0]);
    i = 1;
    while (wordlist[i][0] != '\0'){
        strcat(words, ", ");
        strcat(words, wordlist[i]);
        i++;
    }
    return words;
}

// identify player using socket number (assume it won't change for one player)
int identify_player(socket_t *socket_num, int sockfd){
    if(socket_num->palyer_1_socket == sockfd){
        return 0;
    }else if(socket_num->palyer_2_socket == sockfd){
        return 1;
    }else if(socket_num->palyer_1_socket == -1){
        socket_num->palyer_1_socket = sockfd;
        return 0;
    }else if(socket_num->palyer_2_socket == -1){
        socket_num->palyer_2_socket = sockfd;
        return 1;
    }
    return 0;
}

// check if one player's word is in the other's wordlist
bool compare_keyword(char wordlist[LIST_LEN][WORD_LEN], char* keyword){
    for(int i = 0; i < LIST_LEN; i++){
        if(wordlist[i] == NULL){
            return false;
        }
        if(strcmp(wordlist[i], keyword) == 0){
            return true;
        }
    }
    return false;
}

bool send_header(int sockfd, char* html, char* cookie, char* data){
    char buff[BUFFER_SIZE];

    //get the size of the file
    struct stat st;
    stat(html, &st);
    long size = st.st_size;

    if (data){
        int data_len = strlen(data);

        long added_length = data_len; 
        // increase file size to accommodate the username
        size = st.st_size + added_length;
    }
    
    int n;
    if (cookie)
        n = sprintf(buff, HTTP_200_FORMAT_WITH_COOKIE, size, cookie);
    else
        n = sprintf(buff, HTTP_200_FORMAT, size);
    
    // send the header first
    if (write(sockfd, buff, n) < 0){
        perror("write");
        return false;
    }
    return true;
}

bool send_page(int sockfd, char* html, char* data, int page_number){
    char buff[BUFFER_SIZE];

    // read the content of the HTML file
    int filefd = open(html, O_RDONLY);
    int n = read(filefd, buff, BUFFER_SIZE);
    if (n < 0){
        perror("read");
        close(filefd);
        return false;
    }
    close(filefd);
    buff[n] = '\0';

    struct stat st;
    stat(html, &st);
    // increase file size to accommodate the username
    long size = st.st_size;

    if(page_number != -1){
        char* page = strstr(buff, "image-")+6;
        page[0] = page_number + '0';
    }
    
    if(data){
        int added_length = strlen(data);
        // increase file size to accommodate the username
        size = st.st_size + added_length;

        int p1, p2;
        for (p1 = size - 1, p2 = p1 - added_length; p1 >= size - 25; --p1, --p2)
            buff[p1] = buff[p2];
        ++p2;

        // copy the username
        strncpy(buff + p2, data, strlen(data));
    }

    if (write(sockfd, buff, size) < 0){
        perror("write");
        return false;
    }
    return true;
}

static bool handle_http_request(int sockfd, STATE* state, int* connection_count,
         socket_t *socket_num, char wordlist[MAX_PLAYER][LIST_LEN][WORD_LEN]){

    // try to read the request
    char buff[BUFFER_SIZE];
    int n = read(sockfd, buff, BUFFER_SIZE);
    if (n <= 0){
        if (n < 0)
            perror("read");
        else
            printf("socket %d close the connection\n", sockfd);
        return false;
    }

    // terminate the string
    buff[n] = 0;

    char * curr = buff;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0){
        curr += 4;
        method = GET;
    }
    else if (strncmp(curr, "POST ", 5) == 0){
        curr += 5;
        method = POST;
    }
    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0){
        perror("write");
        return false;
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;

    /*
        Not_ready state: 
        responsible for scenario that not both player are prepared 
    */
    if(*state == NOT_READY){
        // if find start in buff then go ahead and star
        if (strstr(buff, "?start=Start")){
            if(method == GET){
                send_header(sockfd, "3_first_turn.html", NULL, NULL);
                send_page(sockfd, "3_first_turn.html", NULL, PAGE_NUM);
                *connection_count+=1;
                printf("this is connection_count: %d\n", *connection_count);

                if(*connection_count >= MAX_PLAYER){
                    init_wordlist(wordlist);
                    *state = READY;
                } 
            }
            else if (strstr(buff, "quit")){
                if(strstr(buff, "img")){
                    *connection_count -= 1;
                }
                send_header(sockfd, "7_gameover.html", NULL, NULL);
                send_page(sockfd, "7_gameover.html", NULL, -1);
                return false;
            }
            else if(strstr(buff, "guess")){
                send_header(sockfd, "5_discarded.html", NULL, NULL);
                send_page(sockfd, "5_discarded.html", NULL, PAGE_NUM);
            }
        }

        else if (method == GET){
            char *username = NULL;
            username = strstr(buff, "Cookie: name=");
            if(username != NULL){
                char* name;
                char *temp = strtok(username + 13, "\r\n");
                name = (char*) malloc(strlen(temp) + 1);
                strcpy(name, temp);

                send_header(sockfd, "2_start.html", NULL, name);
                send_page(sockfd, "2_start.html", name, -1);

                free(name);
            }
            else{
                send_header(sockfd, "1_intro.html", NULL, NULL);
                send_page(sockfd, "1_intro.html", NULL, -1);
            } 
        }
        else if (method == POST){

            // When received Quit signal, jump to Quit page
            if(strstr(buff, "quit")){
                send_header(sockfd, "7_gameover.html", NULL, NULL);
                send_page(sockfd, "7_gameover.html", NULL, -1);

                // terminate the process 
                return false;
            }

            char * username = strstr(buff, "user=") + 5;

            // init cookie:
            char* user_cookie = (char*) malloc((6 + strlen(username) * sizeof(char)));
            sprintf(user_cookie, "name=%s", username);

            send_header(sockfd, "2_start.html", user_cookie, username);
            send_page(sockfd, "2_start.html", username, -1);

            free(user_cookie);
        }
        /*
            READY:
            responsible for scenario that both players are ready to input words
            and handle to guess part
        */
    }else if(*state == READY){
        //guess the word
        if(strstr(buff, "guess")){
            int player = identify_player(socket_num, sockfd);
            char* keyword;
            char *word_temp = strstr(buff, "keyword=") + 8;
            word_temp = strtok(word_temp, "&");
            keyword = (char*) malloc(strlen(word_temp) + 1);
            strcpy(keyword, word_temp);

            // when one player successd work out the word:
            if(compare_keyword(wordlist[(player+1)%2], keyword)){
                send_header(sockfd, "6_endgame.html", NULL, NULL);
                send_page(sockfd, "6_endgame.html", NULL, -1);
                PAGE_NUM = (PAGE_NUM + 1) % 4 + 1;
                *state = WIN;
                *connection_count -= 1;
            }

            // haven't reach the end stage.
            else{
                // store this word into current player's wordlist:
                add_into_wordlist(wordlist[player], keyword);

                char *words = to_list(wordlist[player]);
                send_header(sockfd, "4_accepted.html", NULL, words);
                send_page(sockfd, "4_accepted.html", words, PAGE_NUM);

                free(words);
            }
            free(keyword);
        }
        else if(strstr(buff, "quit")){
            send_header(sockfd, "7_gameover.html", NULL, NULL);
            send_page(sockfd, "7_gameover.html", NULL, -1);
            *connection_count = 0;
            *state = QUIT;
            return false;
        }
    }
    /*
            WIN:
            responsible for scenario that player success guess the word
            and handle to quit part
     */
    else if(*state == WIN){
        if(strstr(buff, "quit")){
            send_header(sockfd, "7_gameover.html", NULL, NULL);
            send_page(sockfd, "7_gameover.html", NULL, -1);
            *connection_count-=1;
            return false;
        }else if(strstr(buff, "guess")){
            send_header(sockfd, "6_endgame.html", NULL, NULL);
            send_page(sockfd, "6_endgame.html", NULL, -1);
            *connection_count-=1;
        }
        *state = NOT_READY;
    }
    /*
            QUIT:
            responsible for scenario that player quits
    */
    else if(*state == QUIT){
        send_header(sockfd, "7_gameover.html", NULL, NULL);
        send_page(sockfd, "7_gameover.html", NULL, -1);

        *state = NOT_READY;
    }
    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0){
        perror("write");
        return false;
    }
    else
        // never used, just for completeness
        fprintf(stderr, "no other methods supported");

    return true;
}

/*---------------------------------MAIN FUNTION-------------------------------*/
int main(int argc, char * argv[]){
    STATE state = NOT_READY;

    socket_t *socket_num = (socket_t*)malloc(sizeof(*socket_num));
    socket_num->palyer_1_socket = -1;
    socket_num->palyer_2_socket = -1;

    char wordlist[MAX_PLAYER][LIST_LEN][WORD_LEN];

    // count the nnumber of connected clients
    int connection_count = 0;

    if (argc < 3){
        fprintf(stderr, "usage: %s ip port\n", argv[0]);
        return 0;
    }

    // create TCP socket which only accept IPv4 - welcome socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // reuse the socket if possible
    int const reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // create and initialise address we will listen on
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // if ip parameter is not specified
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // bind address to socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // listen on the socket
    listen(sockfd, 5);

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);
    // record the maximum socket number
    int maxfd = sockfd;

    // print IP address and port number
    printf("image_tagger server is now running at IP: %s on port %s\n", 
            argv[1],argv[2]);

    while (1){
        // monitor file descriptors
        fd_set readfds = masterfds;
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0){
            perror("select");
            exit(EXIT_FAILURE);
        }

        // loop all possible descriptor
        for (int i = 0; i <= maxfd; ++i){
            // determine if the current file descriptor is active
            if (FD_ISSET(i, &readfds)){
                // create new socket if there is new incoming connection request
                if (i == sockfd){
                    struct sockaddr_in cliaddr;
                    socklen_t clilen = sizeof(cliaddr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
                    if (newsockfd < 0)
                        perror("accept");
                    else{
                        // add the socket to the set
                        FD_SET(newsockfd, &masterfds);
                        // update the maximum tracker
                        if (newsockfd > maxfd)
                            maxfd = newsockfd;
                        // print out the IP and the socket number
                        char ip[INET_ADDRSTRLEN];
                        printf(
                            "new connection from %s on socket %d\n",
                            // convert to human readable string
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, ip, INET_ADDRSTRLEN),
                            newsockfd
                        );
                    }
                }
                // a request is sent from the client
                else if (!handle_http_request(i, &state, &connection_count, socket_num, wordlist)){
                    close(i);

                    FD_CLR(i, &masterfds);
                }
            }
        }
    }
    free(socket_num);
    return 0;
}