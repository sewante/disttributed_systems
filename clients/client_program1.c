/*
	@Author: Mwesigye Robert
*/
/** 
	//program name
		client_program1.c
	//description
		program creates a client socket and a server/receiver socket address then exchanges messages with the server using that socket address
		client and server listen on port 5150
	//limitations
		communication is by string messages only
				
	EXPERIMENT TO TEST THE RELIABILITY OF DATAGRAMS
	PROCEDURE: 		*We connected three computers on the same network and submitted the request messages at once of various sizes
	OBSERVATION: 	*Unfortunately the server was able to respond to all the request messages no matter the size of the string 
					message submitted
	DEDUCTION:		*The above observation is the case because the network we used was not  congested so the server could not lose
					any datagrams.
					This doesn't mean that datagram communication is reliable as opposed to our experimental findings
	
*/

#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include<winsock2.h>

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
void initialize_winsocke2(void);															//initialize winsock2
void printSA(SocketAddress socket_address);											//print the socket address
void makeDestSA(struct sockaddr_in *socket_address, char *hostname, int port);		//make destination socket address
void makeLocalSA(struct sockaddr_in *socket_address);								//prepare the local socket address for the client
Status UDPreceive(int socket, Message *reply_msg, SocketAddress *origin);			//--receives datagrams using client's socket_address
Status UDPsend(int socket, Message *request_msg, SocketAddress *destination);		//--sends datagrams using client's socket_address
Status DoOperation(Message *request, Message *reply, int socket, SocketAddress serverSA);	//--do the operation

void prnt_msg(char *msg);										//print a message on the screen;
void prnt_err(char *error, int err_code);						//print an error on the screen and error code
void error_msg(char *err_msg);									//print error message on screen
void client_run(char **args);									//main client program

/** global variables */
SocketAddress server_sock_addr;									//destination/server's socket address
SocketAddress client_sock_addr;									//origin/client's socket address
int client_socket;												//socket of the server
int transmit_length = sizeof(server_sock_addr);					//size of the client socket address

/*******************************************************
 *******************************************************
	******* THE MAIN PROGRAM ********
 *******************************************************
 *******************************************************
*/
int main(int argc, char **argv){
	/*validate the arguments passed */
	if(argc <= 1){
		prnt_msg("Usage: name_of_client destination_machine");
		NEW_LN;
		exit(EXIT_FAILURE);
	}
	/* if the argument is supplied well run the client application */
	else if(argc == 2){	
		client_run(argv);
	}
	else{
		prnt_msg("Usage: name_of_client destination_machine");
		prnt_msg("run the client by putting the name of the server machine as an argument.");
	}
	
	return 0;
}
/******************************************/
//###################################################################################################################################################
//function for the main client application
void client_run(char **args){
	
	Message request;	//the request to the server
	Message reply;		//the response from the sever
	
	Status _status;		//status for DoOperation()
	
	
	/* initialize winsock2 */
	initialize_winsocke2();
	
	
	/** create a socket and bind it to the socket address*/
	client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	
	//check whether the socket was created successfully
	if(client_socket == INVALID_SOCKET){
		prnt_err("could not create client socket with error code",WSAGetLastError());
		
		//cleanup
		WSACleanup();
		exit(EXIT_FAILURE);
	}
	prnt_msg("client socket created successfully....");
	
	//prepare socket address for the client
	makeLocalSA(&client_sock_addr);
	printf("\n > client socket address is %s:%d",inet_ntoa(client_sock_addr.sin_addr), ntohs(client_sock_addr.sin_port));
	//prepare socket address in structure for the receiver/server.
	memset((char *)&server_sock_addr, 0, sizeof(server_sock_addr));
	makeDestSA(&server_sock_addr,args[1],DEFAULT_PORT);
	printSA(server_sock_addr);
	
	
	//send request to server and loop back to send more requests
	while(1){
		NEW_LN;
		memset(&request, '\0', sizeof(request));			//first clear the request structure before filling it with a new request message
		
		prnt_msg("Enter your request: ");					//get the message from the user
		gets(request.data);
		
		request.length = strlen(request.data);				//get the length of the request
		
		if(strcmp(request.data, "exit") == 0){
			prnt_msg("CLIENT IS CLOSING DOWN.");
			prnt_msg("bye...");
			exit(EXIT_SUCCESS);
		}
		/* send the request and block until response is received  */
		_status = DoOperation(&request, &reply, client_socket, server_sock_addr);
		if(_status == BAD){
			//message("It seems the server is not on.");
		}
		else if(_status == OK){
			 prnt_msg("send and receive OK");
		}
		
	}//##end while
	//cleanup
	closesocket(client_socket);
	WSACleanup();
}
//############################################################################################################################################
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
//function to DoOperation send request and receives reply
Status DoOperation(Message *request, Message *reply, int socket, SocketAddress serverSA){
	Status status;
	//check if send was ok
	status = UDPsend(socket, request, &serverSA);
	if( status == OK){
		prnt_msg("message sent.");
	}
	else{
		error_msg("failed to send");
		return BAD;
	}
	//check if receive was ok
	status = UDPreceive(socket, reply, &serverSA);
	if(status == OK){
		
		//print the reply
		printf("\n > Response: %s", reply->data);
		//clear the reply structure
		memset(reply, '\0', sizeof(reply));
	}
	else{
		error_msg("failed to receive reply");
		NEW_LN;
		status = BAD;
	}
	
	return status;
}

//function that receives datagrams using a UDP socket address uses recvfrom()
Status UDPreceive(int socket, Message *reply_msg, SocketAddress *origin){
	int received_size;				//size of the received data
	Status _status;					//status of the recvfrom()
	
	received_size = recvfrom(socket, (void *)reply_msg, sizeof(*reply_msg), 0, (struct sockaddr *)origin, &transmit_length);
	
	if(received_size == SOCKET_ERROR){
		_status = BAD;
	}
	else{
		_status = OK;
	}
	return _status;
}
//function to send the reply using sendto()
Status UDPsend(int socket, Message *request_msg, SocketAddress *destination){
	int sent_size;
	sent_size = sendto(socket, (const void *)request_msg, sizeof(*request_msg), 0, (struct sockaddr *)destination, transmit_length);
	if(sent_size == SOCKET_ERROR){
		prnt_err("sending datagram failed",WSAGetLastError());
		return BAD;
	}
	return OK;
	
}
//function to make a socket address for a destination whose machine and port are given as arguments 
void makeDestSA(struct sockaddr_in *socket_address, char *hostname, int port){
	struct hostent *host;
	socket_address->sin_family = AF_INET;
	host = gethostbyname(hostname);		//get the host's IP
	
	if(host == NULL){	//check to see if the host name is known
		error_msg("Unknown host name");
		exit(EXIT_FAILURE);
	}
	socket_address->sin_addr = *(struct in_addr*) (host->h_addr);
	socket_address->sin_port = htons(port);
	
}
//function to make a socket address using any of the addresses of this computer for a local socket on the DEFAULT_PORT 
void makeLocalSA(struct sockaddr_in *socket_address){
	socket_address->sin_family = AF_INET;
	socket_address->sin_port = htons(DEFAULT_PORT);
	socket_address->sin_addr.s_addr = htonl(INADDR_ANY);
}
//function to print a socket address 
void printSA(SocketAddress socket_address){
	NEW_LN;
	printf(" > Destination address %s:%d",inet_ntoa(socket_address.sin_addr), ntohs(socket_address.sin_port));
	
}
//####################################################################################################################################################

														/** UTILITY FUNCTIONS */
//####################################################################################################################################################
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
//###################################################################################################################################################