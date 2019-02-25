/*
	@Author: Mwesigye Robert
*/
/** 
	//program name
		server_program1.c
	//description
		*program creates a server socket and a server/receiver socket address and client socket address then receives a message
		from the client and echoes the same message back to the client that sent it
		*server listen on port 5150
	//limitations
		communication is by string messages only
		
*/

#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include<winsock2.h>			//the header file for sockets in windows

#define BUFFER_SIZE 1024		//the size of the buffer in bytes
#define DEFAULT_PORT 5150		//the port for communication
#define NEW_LN {printf("\n");}	//the new line macro

//the status after an execution
typedef enum{
	OK,							/* operation successful */
	BAD,						/* unrecoverable error */
	WRONG_LENGTH				/* bad message length supplied */
} Status;

//message datagram
typedef struct{
	unsigned int length;				//length of the message
	unsigned char data[BUFFER_SIZE];	//the data itself
}Message;

typedef struct sockaddr_in SocketAddress;	//the new type to represent sockaddr_in

/** function prototypes */
void initialize_winsocke2(void);												//initialize winsock2
void makeReceiverSA(SocketAddress *socket_address,int port);					//create the receiver's  socket_address
void printSA(SocketAddress socket_address);										//print the socket address
Status UDPreceive(int socket, Message *request_msg, SocketAddress *origin);		//--receives datagrams using client's socket_address
Status UDPsend(int socket, Message *reply_msg, SocketAddress *destination);		//--sends datagrams using client's socket_address

Status GetRequest(Message *callMessage, int socket, SocketAddress *clientSA);	//get the request and close the sever if only 'q' is received
Status SendReply(Message *replyMessage, int socket, SocketAddress clientSA);	//send the reply to the specified socket_address

void prnt_msg(char *msg);										//print a message on the screen;
void prnt_err(char *error, int err_code);						//print an error on the screen and error code
void error_msg(char *err_msg);									//print error message on screen

void server_run(void);											//main server application

/** global variables */
SocketAddress client_sock_addr;									//destination/sender's/ client socket address
SocketAddress server_sock_addr;									//origin/receiver's/ sever socket address

int server_socket;												//socket of the server
int transmit_length = sizeof(client_sock_addr);					//size of the client socket address

/*******************************************************
 *******************************************************
	******* THE MAIN PROGRAM ********
 *******************************************************
 *******************************************************
*/
int main(void){
	server_run();
	return 0;
}
/******************************************/
//###################################################################################################################################################
//function for the main server application
void server_run(void){
	//some variables
	Message request;				//request message
	
	Status recv_status;				//status for receive
	Status send_status;				//status for send
	
	/* initialize winsock */
	initialize_winsocke2();
	
	/* create server socket */
	prnt_msg("creating server socket...");
	server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	
	if(server_socket == INVALID_SOCKET){
		prnt_err("failed to create server socket", WSAGetLastError());
		WSACleanup();
		return;
	}
	prnt_msg("server socket created successfully");
	
	/* prepare server socket address */
	makeReceiverSA(&server_sock_addr, DEFAULT_PORT);
	
	/* bind the server socket */
	prnt_msg("binding the server socket to its socket address...");
	if(bind(server_socket, (struct sockaddr *)&server_sock_addr, sizeof(server_sock_addr)) == SOCKET_ERROR){
		prnt_err("failed to bind the sever socket to socket address", WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		return;
	}
	prnt_msg("server socket bound successfully to its socket address");
	printf("\n > server socket address is %s:%d",inet_ntoa(server_sock_addr.sin_addr), ntohs(server_sock_addr.sin_port));
	NEW_LN;
	prnt_msg("server started running...");
	/* loop */
	while(1){
		//do the work man!!!
		NEW_LN;
		prnt_msg("waiting for data....");
		fflush(stdout);
		
		/* call receive */
		GetRequest(&request, server_socket, &client_sock_addr);
		/* call transmit */
		SendReply(&request, server_socket, client_sock_addr);
	}
}
//#################################################################################################################################################
											/** SOCKET AND CONNECTION FUNCTIONS */
//###################################################################################################################################################
//initialize winsock2
void initialize_winsocke2(void){
	WSADATA wsa_data;
	
	prnt_msg("initializing winsock version2....");
	int wsa_startup = WSAStartup(MAKEWORD(2,2), &wsa_data);		//for winsock vesion 2
	if(wsa_startup != NO_ERROR){
		prnt_err("Winsock2 startup failed",WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	prnt_msg("Winsock2 startup OK");
}
//function that receives a request from the client
Status GetRequest(Message *callMessage, int socket, SocketAddress *clientSA){
	/*calls UDPreceive() to receive a message from a socket address,
	 checks if the string is q so that it exits the server
	 then calls SendReply() to send back the reply about the server's closure
	 */
	 memset(callMessage, '\0', sizeof(*callMessage));		//first clear the receive buffer before receiving
	
	int index = 0;											//the index for counting
	
	/*receive the request */
	 Status _status = UDPreceive(socket, callMessage, clientSA);
	 
	 /* check if the request =  'q'  then close the server if it is q*/
	 if(strcmp(callMessage->data,"q") == 0){
		 //notify the client about the close up
		 Message close_msg;
		 char mess[BUFFER_SIZE]	= "OOOOppppsss!!!! Server has closed down.....";
		 for(index = 0; index < strlen(mess); index++){
			 close_msg.data[index] = mess[index];
		 }
		 close_msg.length = strlen(mess);
		 SendReply(&close_msg, socket, *clientSA);
		 //clean where you ate from before leaving
		 closesocket(socket);
		 WSACleanup();
		 NEW_LN;
		 prnt_msg("the server was closed down by the client....");
		 prnt_msg("SERVER IS CLOSING DOWN");
		 prnt_msg("bye....");
		 exit(EXIT_SUCCESS);
	 }
	 //display the request message
	 NEW_LN;
	 printSA(*clientSA);
	 NEW_LN;
	 printf(" > request message: %s",callMessage->data);
}
//function that sends replies to the client
Status SendReply(Message *replyMessage, int socket, SocketAddress clientSA){
	Status _status;
	
	/* calls UDPsend to send the response/reply to a specified socket address */
	_status = UDPsend(socket, replyMessage, &clientSA);
	if(_status == OK){
		prnt_msg("reply sent.");
	}
	else if(_status == BAD){
		error_msg("send failed, reply not sent.");
	}
	
	return _status;
}
//function that receives datagrams using a UDP socket address uses recvfrom()
Status UDPreceive(int socket, Message *request_msg, SocketAddress *origin){
	int received_size;				//size of the received data
	Status _status;					//status of the recvfrom()
	
	received_size = recvfrom(socket, (void *)request_msg, sizeof(*request_msg), 0, (struct sockaddr *)origin, &transmit_length);
	
	if(received_size == SOCKET_ERROR){
		_status = BAD;
	}
	else{
		_status = OK;
	}
	return _status;
}
//function to send the reply using sendto()
Status UDPsend(int socket, Message *reply_msg, SocketAddress *destination){
	int sent_size;
	sent_size = sendto(socket, (const void *)reply_msg, sizeof(*reply_msg), 0, (struct sockaddr *)destination, transmit_length);
	if(sent_size == SOCKET_ERROR){
		prnt_err("sending datagram failed",WSAGetLastError());
		return BAD;
	}
	return OK;
	
}
//function to make a socket address using any of the addresses of this computer for a local socket on given port 
void makeReceiverSA(SocketAddress *socket_address, int port){
	
	socket_address->sin_family = AF_INET;
	socket_address->sin_port = htons(port);
	socket_address->sin_addr.s_addr = htonl(INADDR_ANY);
}
//function to print a socket address 
void printSA(SocketAddress socket_address){
	NEW_LN;
	printf(" > Received datagrams from socket_address %s:%d",inet_ntoa(socket_address.sin_addr), ntohs(socket_address.sin_port));
}
//##################################################################################################################################################
												/** UTILITY FUNCTIONS */
//##################################################################################################################################################
//print text
void prnt_msg(char *msg){
	NEW_LN;
	printf(" > %s",msg);
}
//print and error
void prnt_err(char *error, int err_code){
	NEW_LN;
	printf(" > ERROR: %s with error code, %d",error,err_code);
	NEW_LN;
}
//function to print error message
void error_msg(char *err_msg){
	NEW_LN;
	printf(" > ERROR: %s",err_msg);
}
//####################################################################################################################################################