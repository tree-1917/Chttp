// ==================== ==> HTTP SERVER <== ==================== // 
// Author : Gamal Moussa					 // 
// Date   : 7.31.2025						 // 
// ab Rate:							 // 
// ============================================================= // 

// => libs 
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define PORT 8110  
#define BUFFER_SIZE 1024
#define THREAD_POOL_SIZE 5 
#define QUEUE_SIZE 100

// => Data Structures 
typedef struct {
    int client_sockets[QUEUE_SIZE]; 
    int front; 
    int rear ;
    int count; 
} SocketQueue;

// => Global vars for threading 
SocketQueue socket_queue; 
pthread_t thread_pool[THREAD_POOL_SIZE]; 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER; 

// => prototyes 
void handle_hello(int);
void handle_headers(int, char*); 
void handle_request(int);
void* thread_function(void*); 
void enqueue(int); 
int  dequeu(); 
void log_request(const char*, const char*, double); 

// => main  
int 
main() {
    int server_socket, client_socket; 
    struct sockaddr_in server_addr, client_addr; 
    socklen_t addr_len = sizeof(client_addr);
    // Initialize socket queue 
    socket_queue.front = 0; 
    socket_queue.rear = -1; 
    socket_queue.count = 0; 

    // Create thread pool 
    for (int i = 0; i < THREAD_POOL_SIZE; i++ ) {
	pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }
    // = Setup a Socket   
    server_socket = socket(AF_INET, SOCK_STREAM, 0); 
    if ( server_socket == -1 ){
	perror("Socket Creation failed");
	exit(EXIT_FAILURE);
    }
    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT); 
    
    // = Bind a Socket 
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
	perror("Bind failed");
	exit(EXIT_FAILURE); 
    }

    // = Listen to Comming Connection 
    if (listen(server_socket, 10) <0 ) {
	perror("Listen failed");
	exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT); 

    while (1) {
	   client_socket = accept(server_socket, (struct sockaddr*) &client_addr, &addr_len); 
	   if (client_socket < 0) {
	       perror("Accept failed");
	       continue;
	   }
	   // Add client socket to queue 
	   pthread_mutex_lock(&mutex);
	   enqueue(client_socket); 
	   pthread_cond_signal(&condition_var); 
	   pthread_mutex_unlock(&mutex); 
    }
    close(server_socket); 
    return 0; 
}

// ==== thread pool ==== // 
// => thread function 
void* 
thread_function(void *arg) {
    while(1){
	int client_socket; 

	pthread_mutex_lock(&mutex);

	while (socket_queue.count == 0) {
	    pthread_cond_wait(&condition_var, &mutex);
	}
	client_socket = dequeu();
	pthread_mutex_unlock(&mutex);
	handle_request(client_socket);
    }
    return NULL; 
}
// ==== queue of socket ==== // 
// => Queue Qperations 
void 
enqueue(int client_socket){
    if (socket_queue.count >= QUEUE_SIZE ){
	fprintf(stderr, "Queue is full\n");
	close(client_socket); 
	return; 
    }
    socket_queue.rear = (socket_queue.rear + 1) % QUEUE_SIZE ; 
    socket_queue.client_sockets[socket_queue.rear] = client_socket; 
    socket_queue.count++; 
}

int 
dequeu() {
    if (socket_queue.count <= 0) {
	return -1; 
    }
    int client_socket = socket_queue.client_sockets[socket_queue.front]; 
    socket_queue.front = (socket_queue.front + 1) % QUEUE_SIZE; 
    socket_queue.count--; 
    return client_socket; 
}
// ==== utils ==== // 
// => logging handler 
void log_request(const char *method,const char *path, double duration_ms){
    time_t now = time(NULL); 
    struct tm *tm_info = localtime(&now);
    char timestamp[20]; 
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S",tm_info);

    printf("[%s] %s %s - %.3f ms\n", timestamp, method, path, duration_ms); 
}
// ==== Routes ==== // 
// => handle request 
void 
handle_request(int client_socket) {
    char buffer[BUFFER_SIZE]; 
    struct timeval start,end; 
    
    gettimeofday(&start, NULL); // start time 
    
    // take request from client socket 
    recv(client_socket, buffer, sizeof(buffer), 0);
    // extract method and path for logging 
    char method[16], path[256]; 
    sscanf(buffer, "%s %s", method, path);
    // routes 
    if (strncmp(buffer, "GET /hello", 10) == 0){
	handle_hello(client_socket);
    } else if (strncmp(buffer, "GET /headers", 12) == 0 ){
	handle_headers(client_socket, buffer);	
    } else {
	char *res = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found";
	send(client_socket, res, strlen(res), 0); 
    }

    // write log in cli 
    gettimeofday(&end, NULL); // find the end time 
    double duration = (end.tv_sec - start.tv_sec) * 1000.0; 
    duration += (end.tv_usec - start.tv_usec) / 1000.0; 
    log_request(method, path, duration);

    // close the connection 
    close(client_socket); 
}

// => handle headers 
void 
handle_headers(int client_socket, char *req){
    char res[BUFFER_SIZE]; 
    char *header_start = strstr(req, "\r\n") + 2; 
    char *header_end   = strstr(header_start, "\r\n\r\n"); 
    // write a response to stream 
    snprintf(res, sizeof(res),"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    // send a response to client socket 
    send(client_socket, res, strlen(res),0 ); 

    char *header = strtok(header_start, "\r\n"); 
    while ( header != NULL && header < header_end ) {
	snprintf(res,sizeof(res), "%s\n", header);
	send(client_socket, res, strlen(res), 0);
	header = strtok(NULL, "\r\n");
    }
}

// => handle hello 
void 
handle_hello(int client_socket) {
    char *res = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nhello\n"; 
    send(client_socket, res, strlen(res), 0);
}

