//
// Created by 98max on 12/9/2022.
//

#include <net/tcp.h>

using namespace maxOS;
using namespace maxOS::net;
using namespace maxOS::common;
using namespace maxOS::memory;

///__Handler__///

TransmissionControlProtocolHandler::TransmissionControlProtocolHandler()
{
}

TransmissionControlProtocolHandler::~TransmissionControlProtocolHandler()
{
}

bool TransmissionControlProtocolHandler::HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket, uint8_t* data, uint16_t size)
{
    return true;
}

///__Socket__///

TransmissionControlProtocolSocket::TransmissionControlProtocolSocket(TransmissionControlProtocolProvider* backend)
{
    //Set the default values
    this -> backend = backend;
    handler = 0;

    //Closed as default
    state = CLOSED;
}

TransmissionControlProtocolSocket::~TransmissionControlProtocolSocket()
{
}
/**
 * @brief Handle the TCP message (socket end)
 * @param data The datah
 * @param size The size of the data
 * @return True if the connection is to be terminated after hadnling or false if not
 */
bool TransmissionControlProtocolSocket::HandleTransmissionControlProtocolMessage(uint8_t* data, uint16_t size)
{
    if(handler != 0)
        return handler -> HandleTransmissionControlProtocolMessage(this, data, size);
    return false;
}

/**
 * @breif Send data over the socket
 * @param data The data to send
 * @param size The size of the data
 */
void TransmissionControlProtocolSocket::Send(uint8_t* data, uint16_t size)
{
    //Wait for the socket to be connected
    while(state != ESTABLISHED);

    //Pass the data to the backend
    backend -> Send(this, data, size, PSH|ACK);
}

/**
 * @brief Disconnect the socket
 */
void TransmissionControlProtocolSocket::Disconnect()
{
    backend -> Disconnect(this);
}

///__Provider__///

TransmissionControlProtocolProvider::TransmissionControlProtocolProvider(InternetProtocolProvider* backend)
        : InternetProtocolHandler(backend, 0x06)
{
    //Set the default values
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;
    numSockets = 0;

    //Set the free port to 1024, this is the first port that is not reserved
    freePort = 1024;
}

TransmissionControlProtocolProvider::~TransmissionControlProtocolProvider()
{
}

//Shorthand for BE
uint32_t bigEndian32(uint32_t x)
{
    return ((x & 0xFF000000) >> 24)
           | ((x & 0x00FF0000) >> 8)
           | ((x & 0x0000FF00) << 8)
           | ((x & 0x000000FF) << 24);
}

/**
 * @brief Handle the TCP message (provider end)
 * @param srcIP_BE The source IP address
 * @param dstIP_BE The destination IP address
 * @param internetprotocolPayload The payload
 * @param size The size of the payload
 * @return True if data is to be sent back or false if not
 */
bool TransmissionControlProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE, uint8_t* internetprotocolPayload, uint32_t size)
{

    //Check if the size is at least 20 bytes, this is the size of the header
    if(size < 20)
        return false;

    //Get the header
    TransmissionControlProtocolHeader* msg = (TransmissionControlProtocolHeader*)internetprotocolPayload;

    //Get the connection values
    uint16_t localPort = msg -> dstPort;
    uint16_t remotePort = msg -> srcPort;

    //Create the socket
    TransmissionControlProtocolSocket* socket = 0;

    for(uint16_t i = 0; i < numSockets && socket == 0; i++)                         //Loop until the socket is found / reaced max
    {
        if( sockets[i] -> localPort == msg -> dstPort                               //Check if the local port is the same as the destination port
        &&  sockets[i] -> localIP == dstIP_BE                                       //Check if the local IP is the same as the destination IP
        &&  sockets[i] -> state == LISTEN                                           //Check if the socket is in the LISTEN state
        && (((msg -> flags) & (SYN | ACK)) == SYN))                                 //Check if the SYN flag is set (allow for acknoweldgement)
        {
            socket = sockets[i];
        }
        else if( sockets[i] -> localPort == msg -> dstPort                          //Check if the local port is the same as the destination port
             &&  sockets[i] -> localIP == dstIP_BE                                  //Check if the local IP is the same as the destination IP
             &&  sockets[i] -> remotePort == msg -> srcPort                         //Check if the remote port is the same as the source port
             &&  sockets[i] -> remoteIP == srcIP_BE)                                //Check if the remote IP is the same as the source IP
        {
            socket = sockets[i];
        }
    }


    bool reset = false;

    //Check if the socket is found and if the socket wants to reset
    if(socket != 0 && msg -> flags & RST)
    {
        socket -> state = CLOSED;
    }

    //Check if the socket is found and if the socket is not closed
    if(socket != 0 && socket -> state != CLOSED)
    {
        switch((msg -> flags) & (SYN | ACK | FIN))
        {
            /*
             * Example for explanation:
             * socket -> state = SYN_RECEIVED;                                                  //The state of the socket, e.g. recieved, or established. This is used to know how to handle the socket
             * socket -> remotePort = msg -> srcPort;                                           //The remote port, e.g. the port of the server
             * socket -> remoteIP = srcIP_BE;                                                   //The remote IP, e.g. the IP of the server
             * socket -> acknowledgementNumber = bigEndian32( msg -> sequenceNumber ) + 1;      //The acknowledgement number, the number used to keep track of what has been received, this is just incremented by 1 each time
             * socket -> sequenceNumber = 0xbeefcafe;                                           //The sequence number, the number of the next set that is to be sent but in this case sequence isnt enabled so just set it to anything
             * Send(socket, 0,0, SYN|ACK);                                                      //The response command, genneraly has to have the acknoledgement flag set
             * socket -> sequenceNumber++;                                                      //Increment the sequence number
             *
             */

            case SYN:
                if(socket -> state == LISTEN)
                {
                    socket -> state = SYN_RECEIVED;
                    socket -> remotePort = msg -> srcPort;
                    socket -> remoteIP = srcIP_BE;
                    socket -> acknowledgementNumber = bigEndian32( msg -> sequenceNumber ) + 1;
                    socket -> sequenceNumber = 0xbeefcafe;
                    Send(socket, 0,0, SYN|ACK);
                    socket -> sequenceNumber++;
                }
                else
                    reset = true;
                break;


            case SYN | ACK:
                if(socket -> state == SYN_SENT)
                {
                    socket -> state = ESTABLISHED;
                    socket -> acknowledgementNumber = bigEndian32( msg -> sequenceNumber ) + 1;
                    socket -> sequenceNumber++;
                    Send(socket, 0,0, ACK);
                }
                else
                    reset = true;
                break;


            case SYN | FIN:
            case SYN | FIN | ACK:
                reset = true;
                break;


            case FIN:
            case FIN|ACK:
                if(socket -> state == ESTABLISHED)
                {
                    socket -> state = CLOSE_WAIT;
                    socket -> acknowledgementNumber++;
                    Send(socket, 0,0, ACK);
                    Send(socket, 0,0, FIN|ACK);
                }
                else if(socket -> state == CLOSE_WAIT)
                {
                    socket -> state = CLOSED;
                }
                else if(socket -> state == FIN_WAIT1
                        || socket -> state == FIN_WAIT2)
                {
                    socket -> state = CLOSED;
                    socket -> acknowledgementNumber++;
                    Send(socket, 0,0, ACK);
                }
                else
                    reset = true;
                break;


            case ACK:
                if(socket -> state == SYN_RECEIVED)
                {
                    socket -> state = ESTABLISHED;
                    return false;
                }
                else if(socket -> state == FIN_WAIT1)
                {
                    socket -> state = FIN_WAIT2;
                    return false;
                }
                else if(socket -> state == CLOSE_WAIT)
                {
                    socket -> state = CLOSED;
                    break;
                }

                if(msg -> flags == ACK)
                    break;

                // no break, because of piggybacking

            default:

                //By default handle the data

                if(bigEndian32(msg -> sequenceNumber) == socket -> acknowledgementNumber)
                {

                    reset = !(socket -> HandleTransmissionControlProtocolMessage(internetprotocolPayload + msg -> headerSize32*4,size - msg -> headerSize32*4));
                    if(!reset)
                    {
                        int x = 0;                                                                      //The number of bytes to send back
                        for(int i = msg -> headerSize32*4; i < size; i++)                               //Loop through the data
                            if(internetprotocolPayload[i] != 0)                                         //Check if the data is not 0
                                x = i;                                                                  //Set the number of bytes to send back to the current index
                        socket -> acknowledgementNumber += x - msg -> headerSize32*4 + 1;               //Increment the acknowledgement number by the number of bytes to send back
                        Send(socket, 0,0, ACK);                                          //Send the acknowledgement
                    }
                }
                else
                {
                    // data in wrong order
                    reset = true;
                }

        }
    }



    if(reset)                                                                       //If the socket is to be reset
    {
        if(socket != 0)                                                             //If the socket exists then send a reset flag
        {
            Send(socket, 0,0, RST);
        }
        else                                                                        //If it doesnt exist then create a new socket and send a reset flag
        {
            TransmissionControlProtocolSocket socket(this);                     //Create a new socket
            socket.remotePort = msg -> srcPort;                                         //Set the remote port
            socket.remoteIP = srcIP_BE;                                                 //Set the remote IP
            socket.localPort = msg -> dstPort;                                          //Set the local port
            socket.localIP = dstIP_BE;                                                  //Set the local IP
            socket.sequenceNumber = bigEndian32(msg -> acknowledgementNumber);       //Set the sequence number
            socket.acknowledgementNumber = bigEndian32(msg -> sequenceNumber) + 1;   //Set the acknowledgement number
            Send(&socket, 0,0, RST);                               //Send the reset flag
        }
    }


    if(socket != 0 && socket -> state == CLOSED)                                        //If the socket is closed then remove it from the list
        for(uint16_t i = 0; i < numSockets && socket == 0; i++)                         //Loop through the sockets waiting for the socket to be found
            if(sockets[i] == socket)                                                    //If the socket is found
            {
                sockets[i] = sockets[--numSockets];                                     //Remove the socket from the list
                MemoryManager::activeMemoryManager -> free(socket);              //Free the memory used by the socket
                break;                                                                  //Break out of the loop
            }



    return false;
}

/**
 * @brief Send a packet (Throught the provider)
 * @param socket    The socket to send the packet from
 * @param data    The data to send
 * @param size   The size of the data
 * @param flags  The flags to send
 */
void TransmissionControlProtocolProvider::Send(TransmissionControlProtocolSocket* socket, uint8_t* data, uint16_t size, uint16_t flags)
{
    //Get the total size of the packet and the packet with the pseudo header
    uint16_t totalLength = size + sizeof(TransmissionControlProtocolHeader);
    uint16_t lengthInclPHdr = totalLength + sizeof(TransmissionControlProtocolPseudoHeader);

    //Create a buffer for the packet
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager -> malloc(lengthInclPHdr);
    uint8_t* buffer2 = buffer + sizeof(TransmissionControlProtocolHeader) + sizeof(TransmissionControlProtocolPseudoHeader);

    //Create the headers
    TransmissionControlProtocolPseudoHeader* phdr = (TransmissionControlProtocolPseudoHeader*)buffer;
    TransmissionControlProtocolHeader* msg = (TransmissionControlProtocolHeader*)(buffer + sizeof(TransmissionControlProtocolPseudoHeader));

    //Size is translated into 32bit
    msg -> headerSize32 = sizeof(TransmissionControlProtocolHeader)/4;

    //Set the ports
    msg -> srcPort = socket -> localPort;
    msg -> dstPort = socket -> remotePort;

    //Set TCP related data
    msg -> acknowledgementNumber = bigEndian32( socket -> acknowledgementNumber );
    msg -> sequenceNumber = bigEndian32( socket -> sequenceNumber );
    msg -> reserved = 0;
    msg -> flags = flags;
    msg -> windowSize = 0xFFFF;
    msg -> urgentPtr = 0;

    //Through the options allow for the MSS to be set
    msg -> options = ((flags & SYN) != 0) ? 0xB4050402 : 0;

    //Increase the sequence number
    socket -> sequenceNumber += size;

    //Copy the data into the buffer
    for(int i = 0; i < size; i++)
        buffer2[i] = data[i];

    //Set the pseudo header
    phdr -> srcIP = socket -> localIP;
    phdr -> dstIP = socket -> remoteIP;
    phdr -> protocol = 0x0600;
    phdr -> totalLength = ((totalLength & 0x00FF) << 8) | ((totalLength & 0xFF00) >> 8);

    //Calculate the checksum
    msg -> checksum = 0;
    msg -> checksum = InternetProtocolProvider::Checksum((uint16_t*)buffer, lengthInclPHdr);


    //Send and then free the data
    InternetProtocolHandler::Send(socket -> remoteIP, (uint8_t*)msg, totalLength);
    MemoryManager::activeMemoryManager -> free(buffer);
}

/**
 * @breif Connect to a remote host through the TCP protocol
 * @param ip The IP of the remote host
 * @param port The port of the remote host
 * @return The socket that is connected to the remote host, 0 if it failed
 */
TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Connect(uint32_t ip, uint16_t port)
{
    //Create a new socket
    TransmissionControlProtocolSocket* socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager -> malloc(sizeof(TransmissionControlProtocolSocket));

    //If there is space for the socket
    if(socket != 0)
    {
        //Set the socket
        new (socket) TransmissionControlProtocolSocket(this);

        //Set local and remote addresses
        socket -> remotePort = port;
        socket -> remoteIP = ip;
        socket -> localPort = freePort++;
        socket -> localIP = backend -> GetInternetProtocolAddress();

        //Convert into big endian
        socket -> remotePort = ((socket -> remotePort & 0xFF00)>>8) | ((socket -> remotePort & 0x00FF) << 8);
        socket -> localPort = ((socket -> localPort & 0xFF00)>>8) | ((socket -> localPort & 0x00FF) << 8);

        //Set the socket into the socket array and then set its state
        sockets[numSockets++] = socket;
        socket -> state = SYN_SENT;

        //Dummy sequence number
        socket -> sequenceNumber = 0xbeefcafe;

        //Send a sync packet
        Send(socket, 0,0, SYN);
    }

    return socket;
}

/**
 * @breif Begin the disconnect process
 * @param socket The socket to disconnect
 */
void TransmissionControlProtocolProvider::Disconnect(TransmissionControlProtocolSocket* socket)
{

    socket -> state = FIN_WAIT1;                            //Begin fin wait sequence
    Send(socket, 0,0, FIN + ACK);            //Send FIN|ACK packet
    socket -> sequenceNumber++;                             //Increase the sequence number
}

/**
 * Begin listening on a port
 * @param port The port to listen on
 * @return The socket that will handle the connection
 */
TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Listen(uint16_t port)
{
    //Create a new socket
    TransmissionControlProtocolSocket* socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager -> malloc(sizeof(TransmissionControlProtocolSocket));

    //If there is space for the socket
    if(socket != 0)
    {
        //Set the socket
        new (socket) TransmissionControlProtocolSocket(this);

        //Configure the socket
        socket -> state = LISTEN;
        socket -> localIP = backend -> GetInternetProtocolAddress();
        socket -> localPort = ((port & 0xFF00)>>8) | ((port & 0x00FF) << 8);

        //Add the socket to the socket array
        sockets[numSockets++] = socket;
    }

    //Return the socket
    return socket;
}

/**
 * @breif Bind a data handler to this socket
 * @param socket The socket to bind the handler to
 * @param handler The handler to bind
 */
void TransmissionControlProtocolProvider::Bind(TransmissionControlProtocolSocket* socket, TransmissionControlProtocolHandler* handler)
{
    socket -> handler = handler;
}
