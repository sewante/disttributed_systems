/* Windows API for sockets */
/* Socket server */
/*
	@Author: Mwesigye Robert
*/
/** 
	//program name
		server_program2.c
	//description
		program creates a client socket and a server/receiver socket address then receives arguments marshaled in 
		RPCMessage datagram, then choses the procedureId and performs the required arithmetic operation and sands the answer 
		*client and server listen on port 9090
		 NOTE in this assignment we shall strictly use arg1 to hold the returned value and arg2 for status of the answer 
	//limitations
		PROBLEM expressions involving the second argument as negative e.g. (2+-3, 34*-4, 344/-6 e.t.c) are
		considered as subtraction expression {this problem is NOT yet resolved}
		
	
*/

#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<winsock2.h>

#pragma comment(lib,"ws2_32.lib")		//call the Winsock library

#define DEFAULT_PORT 9090			 	//the port for the server process

#define DEFAULT_BUFFER_LENG 1024		//the buffer size in bytes

#define NEW_LN {printf("\n");}			//new line  macro

//the massage type
typedef struct {
	unsigned int length;							/*length of the message */
	unsigned char data[DEFAULT_BUFFER_LENG];		/* the data itself */
	
} Message;

//the RPC message for the arithmetic server
typedef struct{
	
	enum{	/*this enum is to identify the message if request or response (same size as unsigned int)*/
			Request,	/*for request messages */
			Reply		/* for replies */
		}messageType;
	unsigned int RPCId;				/* unique identifier (which reply belongs to which request) */
	unsigned int procedureId;		/* e.g (1,2,3,4) for (+, -, *, /) */
	int arg1;						/* argument / return parameter */
	int arg2;						/* argument / return parameter */
	char msg[DEFAULT_BUFFER_LENG];	/* the string message sent */
	unsigned int length;			/* the length of the string message */
	
}RPCMessage;

//the status after an execution
typedef enum{
	OK,							/* operation successful */
	BAD,						/* unrecoverable error */
	WRONG_LENGTH,				/* bad message length supplied */
	DivZero,					/* in case the denominator is zero */
	TIMEOUT						/*in case of timeout */
} Status;

//new type for the internet socket address 
typedef struct sockaddr_in SocketAddress;

/* The function prototypes */

void initialize_winsock2(void);											//--initialize winsock2
void makeReceiverSA(struct sockaddr_in *socket_address,int port);		//--create the receiver's  socket_address
void printSA(struct sockaddr_in socket_address);		//--print the socket address
void marshal(RPCMessage *rm, Message *message);			//--perform host to network byte ordering on the message to be sent over the network
void unMarshal(RPCMessage *rm, Message *message);		//--perform network to host byte ordering on the message after being received
void stop(int socket, SocketAddress destination, RPCMessage *reply, RPCMessage *request);	//stop() procedure for stopping the client
void ping(int socket, SocketAddress destination, RPCMessage *reply, RPCMessage *request);	//ping() procedure for continuing the client

Status UDPsend(int socket, RPCMessage *reply, SocketAddress destination);			//--sends datagrams using client's socket_address
Status UDPreceive(int socket, RPCMessage *request, SocketAddress *origin);			//--receives datagrams using 	its socket_addres

Status add(int operand1, int operand2, int *result);					//--get the sum of the operands
Status subtract(int operand1, int operand2, int *result);				//--get the difference between operands i.e operand1 - operand2
Status multiply(int operand1 , int operand2, int *result);				//--get the product of the operands
Status divide(int operand1, int operand2, int *result);					//--get the quotient of operand1 and operand2 i.e operand1/operand2

Status arithmeticService(RPCMessage *request_args, RPCMessage *outcome, Message *request, int socket, SocketAddress clientSA);//--the Arithmetic service
Status arithmeticSend(Status state, RPCMessage *outcome,RPCMessage *request_args, Message *request, SocketAddress clientSA, int socket, int *answer);//arithmetic send
Status dispatcher(int operand1, int operand2, int *answer, int procedureId);	//--the dispatcher that decide on which operation to call

void prnt_msg(char *msg);										//print a message on the screen;
void prnt_err(char *error, int err_code);						//print an error on the screen and error code
void error_msg(char *err_msg);									//print error message on screen
void server_run(void);											//--main server application

/* Global variables */
int server_socket;									//the server socket
SocketAddress server_socket_addr;					//socket address for the server/receiver;
SocketAddress client_socket_addr;					//socket address for the sender/client
int from_length = sizeof(client_socket_addr);		//from_length = size of the client socket address
char operator;										//the operator set by the procedureId, only for display purposes at the server terminal

/*******************************************************
 *******************************************************
	******* THE MAIN PROGRAM ********
 *******************************************************
 *******************************************************/
int main(void){
	
	server_run();				//run the server application
	
	return 0;
}
/*********************************************************/
/*********************************************************/
//#############################################################################################################################################
//the main server application
void server_run(void){
	
	Message request;		//the request
	RPCMessage arguments;	//the rpc packet with arguments
	RPCMessage outcome;		//the rpc packet that will hold the result of the computation
	
	initialize_winsock2();	/** initialize winsock2 */
	
	/** create server socket and bind it */
	/** create a socket and bind it to the socket address*/
	server_socket = socket(AF_INET, SOCK_DGRAM,0);
	
	//check whether the socket was created successfully
	if(server_socket == INVALID_SOCKET){
		prnt_err("could not create server socket",WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		return;
	} else{
		prnt_msg("server socket created successfully....");
	}
	
	//prepare sockaddr_in structure
	makeReceiverSA(&server_socket_addr,DEFAULT_PORT);
	
	//do bind the socket to the socket address
	prnt_msg("binding the server socket to the socket address....");
	if(bind(server_socket ,(struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) == SOCKET_ERROR){
		prnt_err("bind failed with error code", WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		exit(EXIT_FAILURE);
	}
	prnt_msg("binding successfully done....");
	NEW_LN;
	printf(" > server socket address %s:%d",inet_ntoa(server_socket_addr.sin_addr), ntohs(server_socket_addr.sin_port));
	prnt_msg("server started....");
	
	/* receive request and then send response then loop back to pick more datagrams */
	while(1){
		NEW_LN;
		prnt_msg("Waiting for data....");
		fflush(stdout);
		/********** call the services of the server from here***********
		***************************************************************/
		
		arithmeticService(&arguments, &outcome, &request, server_socket, client_socket_addr);	//call the arithmetic service
		
		//call other services
	}//end while
	
	/* clean up */
	prnt_msg("server shutting down....");
	closesocket(server_socket);
	WSACleanup();
}
//##############################################################################################################################################
												/********* SOCKET AND CONNECTION FUNCTIONS ****************************/
//##############################################################################################################################################							
//function to initialize winsock2
void initialize_winsock2(void){
	WSADATA wsa_data;
	prnt_msg("Initializing Winsock....");
	int wsaStartupResult = WSAStartup(MAKEWORD(2,2),&wsa_data);
	if(wsaStartupResult != NO_ERROR){	//when wsaStartupResult != 0 then something went wrong
		prnt_err("WSAStartup failed with error code",wsaStartupResult);
		return;
	}
	else{
		prnt_msg("WSAStartup() is OK!");
		prnt_msg("Winsock initialized successfully....");
	}
}
//function to make a socket address using any of the addresses of this computer for a local socket on given port 
void makeReceiverSA(struct sockaddr_in *socket_address, int port){
	
	socket_address->sin_family = AF_INET;
	socket_address->sin_port = htons(port);
	socket_address->sin_addr.s_addr = htonl(INADDR_ANY);
}
//function to print a socket address 
void printSA(struct sockaddr_in socket_address){
	NEW_LN;
	printf(" >Received datagrams from socket_address %s:%d",inet_ntoa(socket_address.sin_addr), ntohs(socket_address.sin_port));
	
}
//function that sends the datagrams using a UDP socket address uses sendto()
Status UDPsend(int socket, RPCMessage *reply, SocketAddress destination){
	int send_length;	//size of the data sent to the server as request
	
	/* send the reply */
	send_length = sendto(socket, (const void *)reply, sizeof(*reply), 0, (struct sockaddr *)&destination, from_length);
	if(send_length == SOCKET_ERROR){
		return BAD;
	}
	return OK;
}
//function that receives datagrams using a UDP socket address uses recvfrom()
Status UDPreceive(int socket, RPCMessage *request, SocketAddress *origin){ 
		
	if(recvfrom(socket, (void *)request, sizeof(*request), 0, (struct sockaddr *)origin, &from_length) == SOCKET_ERROR){
		
		return BAD;
	}
	/* print the details of the client and the message received/request */
	printSA(*origin);
	NEW_LN;
	return OK;
}
//function to stop the client by the server 
void stop(int socket, SocketAddress destination, RPCMessage *reply, RPCMessage *request){
	
	Message stp_msg;			//should not have done this
	int index = 0;
	char stop_msg[] = "OK";	//stop message to the client
	
	while(index <= 2){
		reply->msg[index] = stop_msg[index];
		if(index == 2){
			reply->msg[index++] = '\0';		//terminate the string
			break;
		}
		index++;
	}
	/* set the dat fields of the reply */
	reply->length = strlen(stop_msg);
	stp_msg.length = strlen(stop_msg);
	reply->RPCId = request->RPCId;					//copy the requestId from the request message to the reply message
	reply->messageType = Reply;						//set the messageType to reply
	reply->procedureId = request->procedureId;		//set the procedureId
	reply->arg1 = request->arg1;					//maintain the same arg1 as the request had
	reply->arg2 = request->arg2;					//maintain the same arg2 as the request had
	/* marshal */
	marshal(reply, &stp_msg);
	//send back
	if(UDPsend(socket, reply, destination) == BAD){
		prnt_err("stop procedure failed", WSAGetLastError());
	} else{
		prnt_msg("stop() procedure successful.");
		prnt_msg("Client successfully closed down");
	}
}
//function to do ping
void ping(int socket, SocketAddress destination, RPCMessage *reply, RPCMessage *request){
	
	Message pg_msg;			//should not have done this
	int index = 0;
	char ping_msg[] = "OK";	//stop message to the client
	
	while(index <= 2){
		reply->msg[index] = ping_msg[index];
		if(index == 2){
			reply->msg[index++] = '\0';		//terminate the string
			break;
		}
		index++;
	}
	/* set the data fields of the reply */
	reply->length = strlen(ping_msg);
	pg_msg.length = strlen(ping_msg);
	reply->RPCId = request->RPCId;					//copy the requestId from the request message to the reply message
	reply->messageType = Reply;						//set the messageType to reply
	reply->procedureId = request->procedureId;		//set the procedureId
	reply->arg1 = request->arg1;					//maintain the same arg1 as the request had
	reply->arg2 = request->arg2;					//maintain the same arg2 as the request had
	/* marshal */
	marshal(reply, &pg_msg);
	//send back
	if(UDPsend(socket, reply, destination) == BAD){
		prnt_err("ping() procedure failed", WSAGetLastError());
	} else{
		prnt_msg("ping() procedure successful.");
		return;
	}
}
//function to perform marshaling 
void marshal(RPCMessage *rm, Message *message){
	rm->messageType = htonl(rm->messageType);
	rm->RPCId 	    = htonl(rm->RPCId );
	rm->procedureId = htonl(rm->procedureId);
	rm->arg1 		= htonl(rm->arg1);
	rm->arg2 		= htonl(rm->arg2);
	
	message->length = htonl(message->length);
}
//function to perform unmarshaling
void unMarshal(RPCMessage *rm, Message *message){
	rm->messageType = ntohl(rm->messageType);
	rm->RPCId 	    = ntohl(rm->RPCId );
	rm->procedureId = ntohl(rm->procedureId);
	rm->arg1 		= ntohl(rm->arg1);
	rm->arg2 		= ntohl(rm->arg2);
	
	message->length = ntohl(message->length);
}
//###############################################################################################################################################
												/****** ARITHMETIC OPERATIONS OF THE SERVER (server services) *******/
//###############################################################################################################################################	
/* the arithmetic service */
Status arithmeticService(RPCMessage *request_args, RPCMessage *outcome, Message *request, int socket, SocketAddress clientSA){
	//calls the dispatcher then dispatcher decides which arithmetic operation to call
	
	Status recv_status = UDPreceive(socket, request_args, &clientSA);
	if(recv_status == BAD){
		prnt_err("receive failed with error code",WSAGetLastError());
		return BAD;
	}
	else{
		//unMarshal
		unMarshal(request_args, request);
		//test the conditions, set the new conditions, call dispatcher, get the result, marshal send response
		if(request_args->messageType == Request){
			int result = 0;							//the result of the operation
			Status state;							//status of previous operation
			//check if request->msg is equal to stop or ping ######
			if(strcmp(request_args->msg, "stop") == 0){
				stop(socket, clientSA, outcome, request_args);
			}//####
			else if(strcmp(request_args->msg, "ping") == 0){
				ping(socket, clientSA, outcome, request_args);
				return OK;
			}
			else{
				state = dispatcher(request_args->arg1, request_args->arg2, &result, request_args->procedureId);	//do the calculation
				if(state == OK){
					arithmeticSend(OK, outcome, request_args, request, clientSA, socket, &result);				//send the reply datagram
				}
				else if(state == BAD){
					arithmeticSend(BAD, outcome, request_args, request, clientSA, socket, &result);				//send the reply datagram
					prnt_msg("Not arithmetic parameters");
				}
				else if(state == DivZero){
					arithmeticSend(DivZero, outcome, request_args, request, clientSA, socket, &result);			//send the reply datagram
					prnt_msg("the denominator was a zero");
				}
			}
			
		}
		else{
			prnt_msg("No requests Yet.");
			return OK;
		}
		/* clear the structures */
		memset(outcome, '\0', sizeof(*outcome));
		memset(request_args, '\0', sizeof(*request_args));
		return OK;
	}
}
//function to send the reply datagrams for the arithmetic service
Status arithmeticSend(Status state, RPCMessage *outcome,RPCMessage *request_args, Message *request, SocketAddress clientSA, int socket, int *answer){
	
	outcome->RPCId = request_args->RPCId;				//copy the requestId from the request message to the reply message
	outcome->messageType = Reply;						//set the messageType to reply
	outcome->procedureId = request_args->procedureId;	//set the procedureId
	
	Status send_status;									//status of UDPsend()
	
	if(state == OK){
		outcome->arg2 = 0;			//set the second argument to 0 to indicate the success of the operation, -1 will be for division by zero
		outcome->arg1 = *answer;	//set the result in the reply message
	}
	else if(state == DivZero){
		outcome->arg1 = 0;
		outcome->arg2 = -1;			//for divide by zero i.e denominator = 0
	}
	else if(state == BAD){
		outcome->arg2 = 1;			//for wrong arguments i.e expression not arithmetic
	}
	//marshal
	marshal(outcome, request);
	
	//do the send
	send_status = UDPsend(socket, outcome, clientSA);
	if(send_status == BAD){
		prnt_err("sending arithmetic result failed with error code",WSAGetLastError());
		return BAD;
	}
	else{
		//display the operation on the  server terminal
		NEW_LN;
		if(request_args->arg2 != 0){	
			printf(" >> %d %c %d  = %d",request_args->arg1, operator, request_args->arg2, *answer);
			prnt_msg("arithmetic operation performed successfully.");
		}
	}
	//cleanup the sending container send
	memset(outcome, 0, sizeof(*outcome));
}
//the dispatcher choose the operation to call depending on the procedureId and also sets the operator
Status dispatcher(int operand1, int operand2, int *answer, int procedureId){
	Status _status;
	//addition
	if(procedureId == 1){
		_status = add(operand1, operand2, answer);
		operator = '+';
	}
	//subtraction
	else if(procedureId == 2){
		_status = subtract(operand1, operand2, answer);
		operator = '-';
	}
	//multiplication
	else if(procedureId == 3){
		_status = multiply(operand1, operand2, answer);
		operator = '*';
	}
	//division
	else if(procedureId == 4){
		_status = divide(operand1, operand2, answer);
		operator = '/';
	}
	//non of them
	else{
		_status = BAD;		//there was no arithmetic expression found
	}
	return _status;
}	
//operation to add the arguments
Status add(int operand1, int operand2, int *result){
	*result = operand1 + operand2;
	return OK;
}
//operation to subtract the arguments
Status subtract(int operand1, int operand2, int *result){
	*result =  operand1 - operand2;
	return OK;
}
//operation to multiply the arguments
Status multiply(int operand1, int operand2, int *result){
	*result = operand1 * operand2;
	return OK;
}
//operation to divide the arguments
Status divide(int operand1, int operand2, int *result){
	
	//is the denominator a zero?
	if(operand2 == 0){
		return DivZero;
	}
	else{
		*result = operand1 / operand2;
		return OK;
	}
	
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