/* Windows API for sockets */
/* Socket client */

/*
	@Author: Mwesigye Robert
*/
/**
	*client_program2.c
	*user types in mathematical expression
	*program pick the arguments and the operator
	*program packages the datagram and sends it to the server
	*
	*program receives response from server
	*NOTE in this assignment we shall strictly use arg1 to hold the returned value and arg2 for status of the answer 
	*arg2 = (-1,0,1) of the RPCMessage reply structure is used to decide whether DivZero (denominator was zero), or OK
	(arithmetic operation was ok) or the expression was not arithmetic in mature respectively.
	*
	*arithmetic arguments are sent and retransmission occurs only 3 times in case there was failure to receive response (answer)
	
*/

/** 
	*RPC client sets up a socket and connect to the server and then sends and receives response from the server
	//**PROBLEM expressions involving the second argument as negative e.g. (2+-3, 34*-4, 344/-6 e.t.c) are
	//considered as subtraction expression {this problem is NOT yet resolved}
*/

#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<winsock2.h>
	
#define DEFUALT_BUFFER_LENG 1024			//the size of the buffer in bytes

#define DEFAULT_PORT 9090					//the port for the connection


#define NEW_LN {printf("\n");}				//new line

//the massage type
typedef struct {
	unsigned int length;						//length of the message
	unsigned char data[DEFUALT_BUFFER_LENG];	//the data itself
	
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
	char msg[DEFUALT_BUFFER_LENG];	/* the string message sent */
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

//new type for the socket address
typedef struct sockaddr_in SocketAddress;


/** The function prototypes */

void initialize_winsock2(void);										//--initialize winsock2
void makeLocalSA(struct sockaddr_in *socket_address);				//--make a socket address using any of the addresses of this computer
void makeDestSA(struct sockaddr_in *socket_address, char *hostname, int port);	//--create a destination socket address
void printSA(struct sockaddr_in socket_address);								//--print the socket_address
void marshal(RPCMessage *rm, Message *message);		//--perform host to network byte ordering on the message to be sent over the network
void unMarshal(RPCMessage *rm, Message *message);	//--perform network to host byte ordering on the message after being received
Status anythingThere(int socket, long micro_sec, long sec);							//--monitor the read socket

Status UDPsend(int socket, RPCMessage *request, SocketAddress destination);		//--sends datagrams using client's socket_address
Status UDPreceive(int socket, RPCMessage *reply, SocketAddress *origin);		//--receives datagrams using 	its socket_addres

Status doArithmetic(RPCMessage *rpc_expression, RPCMessage *rpc_result, Message *arithmeticExpression, SocketAddress destination);//--work with arithmetic kind of inputs
int get_arguments(Message *arithmeticExpression, RPCMessage *rpcMessage);		//--extract the arguments from the string
int get_expression_type(RPCMessage *rpc_msg, char *expression, int *index, char operator, unsigned int procedureId); //--is the expression a +, -, *, or / ?
void printHelp(void);																//--display the help menu
void doStop(RPCMessage *request, RPCMessage *reply, Message *input_msg, SocketAddress destination);	//for stop message
void doPing(RPCMessage *request, RPCMessage *reply, Message *input_msg, SocketAddress destination);	//for ping

int get_substr(char *string, char substring[], int start, int end);					//--extract a given portion of the string
void prnt_msg(char *msg);															//print a message on the screen;
void prnt_err(char *error, int err_code);											//print an error on the screen and error code
void error_msg(char *err_msg);														//print error message on screen

void client_run(int number_of_args, char **args);	 								//--the main client application

/** global variables */
SocketAddress client_socket_addr;							//socket address for the sender/client
int	from_length = sizeof(client_socket_addr);				//from_length = size of the client socket address
int client_socket;											//the client socket
	
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
	else if(argc == 2){	
		srand(time(0));		//use the current time as the seed for the pseudo random generator for the RPCId
		client_run(argc, argv);
	}
	else{
		prnt_msg("Usage: name_of_client destination_machine");
		prnt_msg("run the client by putting the name of the server machine as an argument.");
	}
	
	return 0;
}
/*******************************************************/
/*******************************************************/
//##################################################################################################################################################
//the main client application
void client_run(int number_of_args, char **args){
	
	Message expression;			//the expression entered
	RPCMessage argumentsSent;	//the rpc packet that has the arguments to be evaluated
	RPCMessage resultRecived;	//the result of the evaluation from the server
	
	Status _status;				//the status of the doArithmetic procedure
	
	/* initialize winsock2 */
	initialize_winsock2();
	
	
	/** create a socket for sending and receiving*/
	client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	
	//check whether the socket was created successfully
	if(client_socket == INVALID_SOCKET){
		prnt_err("could not create client socket with error code",WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return;
	}
	prnt_msg("client socket created successfully....");
	
	//prepare socket address in structure for the receiver/server.
	memset((char *)&client_socket_addr, 0, sizeof(client_socket_addr));
	makeDestSA(&client_socket_addr,args[1],DEFAULT_PORT);
	printSA(client_socket_addr);
	
	
	//send request to server and loop back to send more requests
	while(1){
		//first clear the request structure before filling it with a new request message
		memset((char *)&expression, '\0', DEFUALT_BUFFER_LENG);	
		
		prnt_msg("Enter an Expression in the following format: operand1<operator>operand2");//get the input expression
		prnt_msg("Valid operators are (+, -, * and /).");
		prnt_msg(">>> ");
		gets(expression.data);
		
		expression.length = strlen(expression.data);					//get the length of the expression
		
		/*check if help is called */
		if(strcmp(expression.data, "help") == 0){
			printHelp();
		}
		/* check for exit */
		else if(strcmp(expression.data, "exit") == 0){
			closesocket(client_socket);			//close file descriptor
			WSACleanup();						//closeup winsock2
			//client closes
			prnt_msg("THE CLIENT HAS CLOSED DOWN.");
			prnt_msg("goodbye.....");
			exit(EXIT_SUCCESS);
		}
		/* stop */
		else if(strcmp(expression.data, "stop") == 0){
			doStop(&argumentsSent, &resultRecived, &expression, client_socket_addr);
		}
		/* ping */
		else if(strcmp(expression.data, "ping") == 0){
			doPing(&argumentsSent, &resultRecived, &expression, client_socket_addr);
		}
		/* do the arithmetic */
		else{
			/* send the request and block until response is received  */
			 _status = doArithmetic(&argumentsSent, &resultRecived, &expression, client_socket_addr);
			 if(_status == BAD){
				 prnt_msg("send OK, but receive failed");
				 NEW_LN;
			 }
			 else if(_status == OK){
				 prnt_msg("arithmetic operation OK");
				 NEW_LN;
			 }
			 else if (_status == WRONG_LENGTH){
				 error_msg("Wrong arguments");
				 NEW_LN;
			 }
			 else if(_status == DivZero){
				 prnt_msg("The denominator was 0. You get infinity");
				 NEW_LN;
			 }
			 else if(_status = TIMEOUT){
				 prnt_msg("failed to receive answer for the previously submitted arithmetic expression");
				 NEW_LN;
			 }
		}
	}//end while
	
	/*clean up */
	closesocket(client_socket);
	WSACleanup();
}
//##################################################################################################################################################
											/**** SOCKET AND CONNECTION FUNCTIONS */
//##################################################################################################################################################
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
//function to make a socket address using any of the addresses of this computer for a local socket on the DEFAULT_PORT 
void makeLocalSA(struct sockaddr_in *socket_address){
	socket_address->sin_family = AF_INET;
	socket_address->sin_port = htons(DEFAULT_PORT);
	socket_address->sin_addr.s_addr = htonl(INADDR_ANY);
}	
//function to make a socket address for a destination whose machine and port are given as arguments 
void makeDestSA(struct sockaddr_in *socket_address, char *hostname, int port){
	struct hostent *host;
	socket_address->sin_family = AF_INET;	//for IPv4
	host = gethostbyname(hostname);			//get the host's IP
	
	if(host == NULL){	//check to see if the host name is known
		error_msg("Unknown host name");
		exit(EXIT_FAILURE);
	}
	socket_address->sin_addr = *(struct in_addr*) (host->h_addr);
	socket_address->sin_port = htons(port);
	
}
//function to print a socket address 
void printSA(struct sockaddr_in socket_address){
	NEW_LN;
	printf(" > Destination socket_address = %s:%d\n",inet_ntoa(socket_address.sin_addr), ntohs(socket_address.sin_port));
}
//function to monitor the timeout event
Status anythingThere(int socket, long micro_sec, long sec){
	Status select_status;				//status after calling select function
	int select_val;						//value returned by select
	
	/* set up the timeout structure */
	struct timeval timeout;
	timeout.tv_usec = micro_sec;		//set the microseconds
	timeout.tv_sec = sec;				//set the seconds
	
	/* create the set of file descriptors to monitor for the timeout event */
	fd_set file_descriptor_set;			//set of file descriptors
	
	FD_ZERO(&file_descriptor_set);		//first clear the file descriptor set
	FD_SET(socket, &file_descriptor_set);	//add the file descriptor to the set
	//call select
	select_val = select(0, &file_descriptor_set, NULL, NULL, &timeout);
	
	switch(select_val){
		case 0:
			select_status = TIMEOUT;
			break;
		case -1:
			select_status = BAD;
			break;
		case 1:
			select_status = OK;
			break;
		default:
			//do nothing for now
			break;
	}
	return select_status;
}

//function that sends the datagrams using a UDP socket address uses sendto()
Status UDPsend(int socket, RPCMessage *request, SocketAddress destination){
	int send_length;										//size of the data sent to the server as request
	
	/* send the request */
	send_length = sendto(socket, (const void *)request,sizeof(*request), 0, (struct sockaddr *)&destination, from_length);
	if(send_length == SOCKET_ERROR){
		return BAD;
	}
	return OK;
}
//function that receives datagrams using a UDP socket address uses recvfrom()
Status UDPreceive(int socket, RPCMessage *reply, SocketAddress *origin){
	
	int recv_length;					//size of the received datagram
	memset(reply, 0, sizeof(*reply));	//first clear the receive buffer before receiving 
	
	recv_length = recvfrom(socket,(void *)reply, sizeof(*reply), 0, (struct sockaddr *)origin, &from_length);
	if(recv_length == SOCKET_ERROR){
		//receive failed
		return BAD;
	}
	else if(recv_length > DEFUALT_BUFFER_LENG){
		//received data doesn't fit in the buffer
		//return WRONG_LENGTH;
	}
	return OK;
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
//function to perform stop
void doStop(RPCMessage *request, RPCMessage *reply, Message *input_msg, SocketAddress destination){
	int index = 0;
	Status stop_state = TIMEOUT;
	int retransmit_ = 0;
	char stop_msg[4] = "stop";
	while(index <= 4){
		request->msg[index] = stop_msg[index];
		if(index == 4){
			request->msg[index++] = '\0';	//terminate the string
			break;
		}
		index++;
	}
	request->arg1 = 0;
	request->arg2 = 0;
	request->messageType = Request;
	request->RPCId = rand();						//pseudo-randomly generate the RPCId;
	request->procedureId = 0;						//set procedureId to 0 since no arithmetic will be done
	input_msg->length = strlen(input_msg->data);	//set the message size
	/* marshal the message */
	marshal(request, input_msg);
	/* send the message and monitor time out event retransmit twice on failure to get response */
	while((stop_state == TIMEOUT) && retransmit_ <= 2){
		
		UDPsend(client_socket, request, destination);//send message
		/* monitor timeout */
		stop_state = anythingThere(client_socket, 0, 4);	//wait for 4.0 seconds before timeout
		switch(stop_state){
				case TIMEOUT:{
					prnt_msg("timeout event has occurred");
					if(retransmit_ < 2){
						retransmit_++;			//go to the next retransmission
						printf("\n > retransmitting the %d time...",retransmit_);
						NEW_LN;
						stop_state = TIMEOUT;					//remain in the loop so as we can retransmit
					}
					else{
						prnt_msg("all retransmissions have failed to receive");
						error_msg("receiving has completely failed");
						NEW_LN;
						stop_state = BAD;					//exit the loop if all attempts to retransmit fail
					}
				}
					break;
				case BAD:{
					error_msg("select_state() failed");
					stop_state = BAD;				//exit the loop if polling the read file descriptor failed
				}
					break;
				case OK:{
					//do the receive and then umarshal then stop the client if the reply->msg is OK
					if(UDPreceive(client_socket, reply, &destination) == BAD){
						if(WSAGetLastError() == 10054){
							error_msg("It seems the connection was closed down!");
						}
						prnt_err("receive failed with error code",WSAGetLastError());
					}
					//on successful receive
					else{
						unMarshal(request, input_msg);	//unMarshal the datagram to be sent since it was marshaled previously
						unMarshal(reply, input_msg);	//unMarshal the received datagram
						//check if the response was to the request sent
						if(reply->RPCId == request->RPCId){
							//check if the response was OK and close down the client
							if(strcmp(reply->msg, "OK") == 0){
								prnt_msg(reply->msg);
								/* cleanup */
								memset(reply, 0, sizeof(*reply));	//cleanup the structure
								closesocket(client_socket);			//close file descriptor
								WSACleanup();						//closeup winsock2
								/* client closes */
								prnt_msg("THE CLIENT HAS CLOSED DOWN.");
								prnt_msg("goodbye.....");
								exit(EXIT_SUCCESS);
							}
							else{
								//cleanup the structure
								memset(reply, 0, sizeof(*reply));
								return;
							}
						}
						else{
							//this is not my datagram so discard it
							return;
						}
					}//##
					
					stop_state = OK;			//exit the loop if we got a ready file descriptor to be read from
				}
				break;
				default:
					//do nothing for now
					break;
			}//end switch
	}
}
//function to perform ping
void doPing(RPCMessage *request, RPCMessage *reply, Message *input_msg, SocketAddress destination){
	int index = 0;
	Status ping_state = TIMEOUT;
	int retransmit_ = 0;
	char ping_msg[4] = "ping";
	while(index <= 4){
		request->msg[index] = ping_msg[index];
		if(index == 4){
			request->msg[index++] = '\0';	//terminate the string
			break;
		}
		index++;
	}
	request->arg1 = 0;
	request->arg2 = 0;
	request->messageType = Request;
	request->RPCId = rand();						//pseudo-randomly generate the RPCId;
	request->procedureId = 0;						//set procedureId to 0 since no arithmetic will be done
	input_msg->length = strlen(input_msg->data);	//set the message size
	/* marshal the message */
	marshal(request, input_msg);
	/* send the message and monitor time out event retransmit twice on failure to get response */
	while((ping_state == TIMEOUT) && retransmit_ <= 2){
		
		UDPsend(client_socket, request, destination);//send message
		/* monitor timeout */
		ping_state = anythingThere(client_socket, 0, 4);	//wait for 4.0 seconds before timeout
		switch(ping_state){
				case TIMEOUT:{
					prnt_msg("timeout event has occurred");
					if(retransmit_ < 2){
						retransmit_++;			//go to the next retransmission
						printf("\n > retransmitting the %d time...",retransmit_);
						NEW_LN;
						ping_state = TIMEOUT;					//remain in the loop so as we can retransmit
					}
					else{
						prnt_msg("all retransmissions have failed to receive");
						error_msg("ping failed, it seems the server is off");
						error_msg("receiving has completely failed");
						NEW_LN;
						ping_state = BAD;					//exit the loop if all attempts to retransmit fail
					}
				}
					break;
				case BAD:{
					error_msg("select_state() failed");
					ping_state = BAD;				//exit the loop if polling the read file descriptor failed
				}
					break;
				case OK:{
					//do the receive and then umarshal then stop the client if the reply->msg is OK
					if(UDPreceive(client_socket, reply, &destination) == BAD){
						if(WSAGetLastError() == 10054){
							error_msg("It seems the connection was closed down!");
						}
						prnt_err("receive failed with error code",WSAGetLastError());
					}
					//on successful receive
					else{
						unMarshal(request, input_msg);	//unMarshal the datagram to be sent since it was marshaled previously
						unMarshal(reply, input_msg);	//unMarshal the received datagram
						//check if the response was to the request sent
						if(reply->RPCId == request->RPCId){
							//check if the response was OK and continue
							if(strcmp(reply->msg, "OK") == 0){
								prnt_msg(reply->msg);
								/* cleanup */
								memset(reply, 0, sizeof(*reply));	//cleanup the structure
								memset(request, '\0', sizeof(*request));
								prnt_msg("The server says OK.");
								NEW_LN;
								return;
							}
							else{
								//cleanup the structure
								memset(reply, 0, sizeof(*reply));
								return;
							}
						}
						else{
							//this is not my datagram so discard it
							return;
						}
					}//##
					
					ping_state = OK;			//exit the loop if we got a ready file descriptor to be read from
				}
				break;
				default:
					//do nothing for now
					break;
			}//end switch
	}
	
}
//##################################################################################################################################################
										/***** ARITHMETIC FUNCTIONS */
//###################################################################################################################################################
//function to handle the Arithmetic expression
Status doArithmetic(RPCMessage *rpc_expression, RPCMessage *rpc_result, Message *arithmeticExpression, SocketAddress destination){
	
	Status _status;
	Status recv_status;								//the status after calling UDPreceive()
	Status send_status;								//the status after calling UDPsend()
	int index = 0;
	
	Status select_state = TIMEOUT;					//the status of anythingThere()
	unsigned int numb_of_retransmission = 0;		//the number of times we retransmit
	
	/* load the string arithmetic expression in the structure */
	while(index <= arithmeticExpression->length){
		rpc_expression->msg[index] = arithmeticExpression->data[index];
		
		if(index == arithmeticExpression->length){
			rpc_expression->msg[index++] = '\0';	//terminate the string
			break;
		}
		index++;
	}
	
	/* call get arguments to verify the arguments */
	if(get_arguments(arithmeticExpression, rpc_expression) == 1){
		rpc_expression->RPCId = rand();			//pseudo-randomly generate the RPCId;
		rpc_expression->messageType = Request;	//set messageType to request
		
		marshal(rpc_expression, arithmeticExpression);	//marshal
		//###here
		while((select_state == TIMEOUT) && numb_of_retransmission <= 3){
		
			//send the packet having the mathematical arguments
			send_status = UDPsend(client_socket, rpc_expression, destination);
			if(send_status == BAD){
				prnt_err("sending arithmetic expression failed with error code",WSAGetLastError());
				_status = BAD;
			}
			else{
				prnt_msg("arithmetic expression sent.");
				_status = OK;
			}
			
			//monitor timeout event
			select_state = anythingThere(client_socket, 500000, 5);		//retransmission occurs after 5.5 seconds
			switch(select_state){
				case TIMEOUT:{
					prnt_msg("timeout event has occurred");
					if(numb_of_retransmission < 3){
						numb_of_retransmission++;			//go to the next retransmission
						printf("\n > retransmitting the %d time...",numb_of_retransmission);
						NEW_LN;
						select_state = TIMEOUT;					//remain in the loop so as we can retransmit
					}
					else{
						prnt_msg("all retransmissions have failed to receive");
						error_msg("receiving has completely failed");
						select_state = BAD;					//exit the loop if all attempts to retransmit fail
						_status = TIMEOUT;
					}
				}
				break;
				case BAD:{
					error_msg("select_state() failed");
					select_state = BAD;				//exit the loop if polling the read file descriptor failed
				}
				break;
				case OK:{
					//do the receive and then umarshal then display the answer and get outa here
					recv_status = UDPreceive(client_socket, rpc_result, &destination);
					if(recv_status == BAD){
						if(WSAGetLastError() == 10054){
							error_msg("It seems the connection was closed down!");
						}
						prnt_err("receive failed with error code",WSAGetLastError());
						_status = BAD;
					}
					else if(recv_status == WRONG_LENGTH){
						error_msg("message too big, it can fit in the receive buffer");
						_status = WRONG_LENGTH;
					}
					//on successful receive
					else{
						unMarshal(rpc_expression, arithmeticExpression);//unMarshal the datagram to be sent since it was marshaled previously
						unMarshal(rpc_result, arithmeticExpression);	//unMarshal the received datagram
						//check if the response was to the request sent
						if(rpc_result->RPCId == rpc_expression->RPCId){
							//special case of division by zero
							if(rpc_result->arg2 == -1){
								_status = DivZero;
							}
							else if(rpc_result->arg2 == 0){
								printf("\n > <<<< the result is: %d",rpc_result->arg1);
								//cleanup the structure
								memset(rpc_result, 0, sizeof(*rpc_result));
								_status = OK;
							}
							else{
								_status = WRONG_LENGTH;
							}
						}
						else{
							//this is not my datagram so discard it
							_status = OK;
						}
					}//##
					
					select_state = OK;			//exit the loop if we got a ready file descriptor to be read from
				}
				break;
				default:
					//do nothing for now
					break;
			}//end switch
		}//##end while
		
	}
	else{
		prnt_msg("It seems your entry was not a mathematical expression! Type help to see the format.");
		_status = WRONG_LENGTH;
	}
	return _status;
}
//function to extract the arguments and the operator, return 1 on successful extraction or 0 if no arithmetic expression found
int get_arguments(Message *arithmeticExpression, RPCMessage *rpcMessage){
	
	char *expression = arithmeticExpression->data;		//pick the entered expression
	int isOk = 0;
		
	//as long as the string has more characters
	int index = 0;
	while(expression[index] != '\0'){
		
		//for addition
		if(get_expression_type(rpcMessage, expression, &index, '+', 1) == 1){
			isOk = 1;	//expression was found
			break;
		}
		//for subtraction
		if(get_expression_type(rpcMessage, expression, &index, '-', 2) == 1){
			isOk = 1;	//expression was found
			break;
		}
		//for multiplication
		if(get_expression_type(rpcMessage, expression, &index, '*', 3) == 1){
			isOk = 1;	//expression was found
			break;
		}
		//for division
		if(get_expression_type(rpcMessage, expression, &index, '/', 4) == 1){
			isOk = 1;	//expression was found
			break;
		}
		
		
		//the case when the entered string is not am expression
		// in case the string is completed and no arithmetic expression is found
		if(index == strlen(expression)){
			prnt_msg("It seems your entry was not a mathematical expression! Type help to see the format.");
			return isOk;	//expression was not found
		}
		
		index++;
	}
	return isOk;
}
//function to get the type of the expression (is it +, -, * or /)
int get_expression_type(RPCMessage *rpc_msg, char *expression, int *index, char operator, unsigned int procedureId){
	char argument1[DEFUALT_BUFFER_LENG];	//operand1 in string form
	char argument2[DEFUALT_BUFFER_LENG];	//operand2 in string form
	
	if(expression[*index] == operator){
	/**store the first argument only if it is a number */
		if(get_substr(expression, argument1, 0, (*index - 1)) == 0){
			//check if argument1 is a number, convert it to an int otherwise report that one of 
			//the arguments is not a number and ask for another expression
			if(isdigit(*argument1)){
				
				rpc_msg->arg1 = atoi(argument1);				//get the int value of the first argument
				memset(argument1, '\0', DEFUALT_BUFFER_LENG);	//clear up the temporary storage
			}else{
				error_msg(" The first argument was not a number! Type help to see the format.");
				return 0;
			}
			
		}else{
			error_msg("In function get_substr() \'end\' was less than \'start\'");
			return 0;
		}
		/**store the second argument only if it is a number */
		if(get_substr(expression, argument2, (*index + 1), strlen(expression)) == 0){
			//check if argument2 is a number, convert it to an int otherwise report that one of 
			//the arguments is not a number and ask for another expression
			if(isdigit(*argument2)){
				
			rpc_msg->arg2 = atoi(argument2);				//get the int value of the second argument
			memset(argument2, '\0', DEFUALT_BUFFER_LENG);	//clear up the temporary storage
			}else{
				error_msg(" The second argument was not a number! Type help to see the format.");
				return 0;
			}
		}else{
			error_msg("In function get_substr() \'end\' was less than \'start\'");
			return 0;
		}
		
		rpc_msg->procedureId = procedureId;		//set the specific procedureId
		return 1;
	}
	else{
		return 0;	//if the char in the expression is not the desired operator just jump out of here
	}
	
}//###
//function to print the help
void printHelp(void){
	NEW_LN;
	prnt_msg("########################### HELP ###############################");
	prnt_msg(" Format: operand1<operator>operand2 e.g. 2+3");
	prnt_msg(" NO spaces between operator and operands.");
	prnt_msg(" NO negative arithmetic (Negative numbers are not supported).");
	prnt_msg("#################################################################");
	NEW_LN;
}
//####################################################################################################################################################
										/** UTILITY FUNCTIONS */
//####################################################################################################################################################
//get substring. Function gets a substring from the passed string from the start to the end in the string inclusive
//return -1 when end < start or 0 when end >= start or 1 otherwise
int get_substr(char *string, char substring[], int start, int end){
	
	
	int position = start;
	int index = 0;			//the position in the substring
	//ensure end > start
	if(start > end){
		return -1;
	}
	else{
		while(position <= end){
		
		substring[index] = string[position];
		index++;			//go to next position in the substring	
		position++;			//go to next position in the input string
		}
		return 0;
	}
	return 1;
}
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