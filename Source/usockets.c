#pragma region header 

#undef UNICODE

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define SLEEP Sleep
    #define ms 1
    #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSESOCKET(s) closesocket(s)
    #define GETSOCKETERRNO() (WSAGetLastError())
    #pragma comment (lib, "Ws2_32.lib")
#else
    #include <string.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <wchar.h>
    #include <signal.h>
    #define INVALID_SOCKET -1
    #define NO_ERROR 0
    #define SOCKET_ERROR -1
    #define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
    #define SLEEP usleep
    #define ms 1000
    inline void nopp() {}
    #define SOCKET int
    #define ISVALIDSOCKET(s) ((s) >= 0)
    #define CLOSESOCKET(s) close(s)
    #define GETSOCKETERRNO() (errno)
    #define BYTE uint8_t
#endif

#include <stdio.h>
#include <semaphore.h>

#include <poll.h>

//#include <sys/timerfd.h>
#define POLL_SIZE 256

#define BLOCKING_SOCKET 2

#define ST_WAIT 2
#define ST_NEXT 0

#include "WolframLibrary.h"
#include "WolframIOLibraryFunctions.h"
#include "WolframNumericArrayLibrary.h"

#pragma region xinternal 

volatile int emergencyExit = 0;

int* globalPipe;
int globalListener = -1;

sem_t mutex;

typedef struct Server_st {
    SOCKET listenSocket;
    SOCKET *clients;
    int clientsLength;
    int clientsLengthMax;
    int bufferSize;
    int* pipe;
}* Server;

Server serverList[100];
int servers = 0;

typedef struct SocketListenerTaskArgs_st {
    WolframLibraryData libData; 
    Server server;
    int* pipe;
}* SocketListenerTaskArgs; 

long sptr = -1;

typedef struct {
    SOCKET socket;
    BYTE* buf;
    unsigned long length;
} wQuery_t;

typedef struct {
    wQuery_t* wQuery[1024];
    int cursor;
} wStack_t;

typedef struct {
    char type;
    SOCKET socketId;
    unsigned long payload;
} pipePacket_t;

//just a stack


typedef struct {
    SOCKET socketId;
    int state;
    int skip; 
    int trials;
    int *pipe;

    wStack_t* wStack;

    int _flag;
} wSocket_t;

//hash map

#define HASH_FREE -199
#define HASH_NEXT 33
#define HASH_OCCUPIED 71

#define hashmap_size 4096
wSocket_t wSockets[hashmap_size];

unsigned long hash(unsigned long key, unsigned int offset) {
    if (offset < 0 || offset > 32) {
        perror("offset hash table is way too big!");
        //SLEEP(10*ms);
        exit(-1);
    }
    unsigned long knuth = 2654435769;
    unsigned long y = key;
    return ((y * knuth) >> (32 - offset)) % hashmap_size;
}



//Push to the writting stack
void wQueryPush(wQuery_t* q, wStack_t *wStack) {
    //randomly start
    //long init = rand() % wQuery_size;
    //sem_wait(&sem);
    sem_wait(&mutex);
    printf("[wQuery] cursor %d\r\n\r\n", wStack->cursor);
    wStack->cursor++;
    wStack->wQuery[wStack->cursor] = q;

    sem_post(&mutex);
    
}

//Check if something in the writting stack
int wQueryQ(wStack_t *wStack) {
    //sem_wait(&mutex);
    printf("[wQueryQ]\r\n\checking\r\n\r\n");
    printf("[wQuery] cursor %d\r\n\r\n", wStack->cursor);
    if (wStack->cursor >= 0) { 
        printf("[wQueryQ]\r\n\found\r\n\r\n");
        sem_post(&mutex);
        return 0;
    }

    sem_post(&mutex);

    return -1;
}



//Pop something from writting stack
wQuery_t* wQueryPop(wStack_t *wStack) {
    //sem_wait(&mutex);
    wQuery_t* res = 0;
    //randomly start
    printf("[wQueryPop] pop\r\n\r\n");
    printf("[wQuery] cursor %d\r\n\r\n", wStack->cursor);
    
    res = wStack->wQuery[0];
    for (int i=1; i<wStack->cursor; ++i) {
        wStack->wQuery[i-1] = wStack->wQuery[i];
    }

    wStack->cursor--;

    sem_post(&mutex);



    return res;
}

void wQueryUnshift(wQuery_t* q, wStack_t *wStack) {
    //sem_wait(&mutex);

    //randomly start
    printf("[wQuery] cursor %d\r\n\r\n", wStack->cursor);
    printf("[wQueryUnshft] unshift\r\n\r\n");
    

    for (int i=wStack->cursor; i<=1; ++i) {
        wStack->wQuery[i] = wStack->wQuery[i-1];
    }

    wStack->wQuery[0] = q;

    wStack->cursor++;

    sem_post(&mutex);
}

unsigned long HashAllocate(SOCKET socketId, int offset);

void HashCopy(SOCKET socketId, int offsetSrc, int offsetDest) {
    unsigned long hS = hash(socketId, offsetSrc);
    printf("hash >> allocate for a copy\n");
    unsigned long hD = HashAllocate(socketId, offsetDest);

    printf("hash >> copied\n");

    memcpy(&wSockets[hD], &wSockets[hS], sizeof(wSocket_t));
}

//helper functions to check the status of the socket
unsigned long HashAllocate(SOCKET socketId, int offset) {
    printf("hash >> allocate %ld with offset %d\n", socketId, offset);
    unsigned long h = hash(socketId, offset);
    printf("hash >> %ld\n", h);

    if (wSockets[h]._flag == HASH_OCCUPIED) {
        printf("hash >> collizion!\n");

        //copy the original value
        printf("hash >> copy old one %ld\n", wSockets[h].socketId);
        HashCopy(wSockets[h].socketId, offset, offset + 1);

        wSockets[h]._flag = HASH_NEXT;
        return HashAllocate(socketId, offset + 1);
    }

    if (wSockets[h]._flag == HASH_NEXT) {
        printf("hash >> next\n");
        return HashAllocate(socketId, offset + 1);
    }

    printf("hash >> ok!\n");
    wSockets[h]._flag = HASH_OCCUPIED;
    wSockets[h].socketId = socketId;

    return h;
}

void HashInit() {
    for (int i=0; i<hashmap_size; ++i) {
        wSockets[i].skip = 0;
        wSockets[i].trials = 0;
        wSockets[i]._flag = HASH_FREE;
    }
}

void HashFree(SOCKET socketId, int offset) {
    printf("hash >> freeing %ld\n", socketId);
    unsigned long h = hash(socketId, offset);
    if (wSockets[h]._flag == HASH_NEXT) {
        return HashFree(socketId, offset + 1);
    }

    if (wSockets[h]._flag == HASH_OCCUPIED) {
        wSockets[h]._flag = HASH_FREE;
        return;
    }

    printf("hash >> already freed!\n");
}

unsigned long HashGet(SOCKET socketId, int offset) {
    printf("[HashGet] get\r\n\r\n");
    unsigned long h = hash(socketId, offset);
    if (wSockets[h]._flag == HASH_NEXT) {
        printf("[HashGet] next\r\n\r\n");
        return HashGet(socketId, offset + 1);
    }
    printf("[HashGet] done\r\n\r\n");

    return h;
}

void wSocketsSet(SOCKET socketId, int state) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    if (wSockets[h].socketId != socketId) {
        printf("Cannot set the state of a wrong socket %ld!\r\n", socketId);
        exit(-1);
        return;
    }
    wSockets[h].state = state;
    wSockets[h].skip = 0;
    sem_post(&mutex);
}



void wSocketsSubtractSkipping(SOCKET socketId) {
    //sem_wait(&mutex);
    printf("[wSocketsSubtractSkipping] hash get\r\n\r\n");
    unsigned long h = HashGet(socketId, 0);
    wSockets[h].skip -= 1;
    sem_post(&mutex);
}

void wSocketsAddSkipping(SOCKET socketId) {
    //sem_wait(&mutex);
    printf("[wSocketsAddSkipping] hash get\r\n\r\n");
    unsigned long h = HashGet(socketId, 0);
    printf("[wSocketsAddSkipping] adding skip\r\n\r\n");
    wSockets[h].skip += (70 +  wSockets[h].trials * 70);
    wSockets[h].trials += 1;
    sem_post(&mutex);
}

void wSocketsResetSkipping(SOCKET socketId) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    wSockets[h].trials = 0;
    sem_post(&mutex);
}

int wSocketsCheckSkipping(SOCKET socketId) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    int skip = wSockets[h].skip;
    sem_post(&mutex);
    return skip;
}

int wSocketsGetState(SOCKET socketId) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    if (wSockets[h].socketId != socketId) {
        sem_post(&mutex);
        return INVALID_SOCKET;
    }
    int state = wSockets[h].state;
    sem_post(&mutex);

    return state;
}

void wSocketsSetPipe(SOCKET socketId, int* pipe) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    wSockets[h].pipe = pipe;
    sem_post(&mutex);
}

int* wSocketsGetPipe(SOCKET socketId) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    int* p = wSockets[h].pipe;
    sem_post(&mutex);
    return p;
}

void wSocketsSetwStack(SOCKET socketId, wStack_t *wStack) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    wSockets[h].wStack = wStack;
    sem_post(&mutex);
}

wStack_t * wSocketsGetwStack(SOCKET socketId) {
    //sem_wait(&mutex);
    unsigned long h = HashGet(socketId, 0);
    wStack_t * w = wSockets[h].wStack;
    sem_post(&mutex);
    return w;
}

int socketWrite(SOCKET socketId, BYTE *buf, unsigned long dataLength, int bufferSize);


//push to the stack a task to write data to the socket
void addToWriteQuery(SOCKET socketId, BYTE *buf, unsigned long dataLength, wStack_t *stack) {
    wQuery_t* query;

    printf("[wquery]\r\n\tadded to the query\r\n\r\n");
    query = (wQuery_t*)malloc(sizeof(wQuery_t));
    query->socket = socketId;
    query->length = dataLength;
    query->buf = (BYTE*)malloc(sizeof(BYTE)*dataLength);
    //make a copy, since WL frees all memory
    memcpy((void*) query->buf, (void*)buf, sizeof(BYTE)*dataLength);
    wQueryPush(query, stack);
}

//check the stack
int pokeWriteQuery(wStack_t *wStack) {
    //a fence to block the operations with the stack
    ////sem_wait(&mutex);

    //if there is something
    if (wQueryQ(wStack) < 0) return -1;
    printf("[wquery]\r\n\tchecking...\r\n\r\n");

    //pop
    wQuery_t* ptr = wQueryPop(wStack);
    printf("[wquery]\r\n\tpopped...\r\n\r\n");
    //just in case

    //skip if it is a delayed message
    int skipping = wSocketsCheckSkipping(ptr->socket);
    if (skipping > 0 && skipping < 3000) {
        printf("[wquery]\r\n\tskipping left %d\r\n\r\n", skipping);
        //decreate the counter
        printf("[wquery]\r\n\t subtracting\r\n\r\n");
        wSocketsSubtractSkipping(ptr->socket);
        //put it back
        printf("[wquery]\r\n\t push it back\r\n\r\n");
        wQueryUnshift(ptr, wStack);
        printf("[wquery]\r\n\t return\r\n\r\n");    

        return ST_WAIT;
    }

    //if toomany skippings
    if (skipping >= 3000) {
        printf("[wquery]\r\n\tsend failed too many reties\r\n\r\n");

        //send command to close
        pipePacket_t packet;
        packet.type = 'C';
        packet.socketId = ptr->socket;
        write(wSocketsGetPipe(ptr->socket)[1], &packet, sizeof(pipePacket_t));


    } else {

        //now we can finally try to write something
        long result = socketWrite(ptr->socket, ptr->buf, ptr->length, 0);
        if (result == SOCKET_ERROR) {
            printf("[wquery]\r\n\tsend failed with error: %d\r\n\r\n", (int)GETSOCKETERRNO());
            
            //send command to close
            pipePacket_t packet;
            packet.type = 'C';
            packet.socketId = ptr->socket;
            write(wSocketsGetPipe(ptr->socket)[1], &packet, sizeof(pipePacket_t));

        } else {
            //printf("[wquery]\r\n\tq finished\r\n\r\n");
            if (result < ptr->length) {
                wSocketsAddSkipping(ptr->socket);
                addToWriteQuery(ptr->socket, ptr->buf + result, ptr->length - result, wStack);
            } else {
                wSocketsResetSkipping(ptr->socket);
                printf("[wquery]\r\n\t bytes written %ld!\r\n\r\n", result);
            }
        }
    }

    //free buffers
    //printf("[wquery]\r\n\tq free buffer\r\n\r\n");

    //remove fence
    //sem_post(&mutex);

    return ST_NEXT;

}

#pragma endregion

#pragma region initialization 

DLLEXPORT mint WolframLibrary_getVersion() {
    printf("[WolframLibrary_getVersion]\r\nlibrary version: %d\r\n\r\n", WolframLibraryVersion);
    return WolframLibraryVersion;
}

void Segfault_Handler(int signo)
{
    fprintf(stderr,"\n[!] Oops! Segmentation fault...\n");
}

DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
    #ifdef _WIN32
        int iResult; 
        WSADATA wsaData; 

        iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0) {
            return LIBRARY_FUNCTION_ERROR;
        }
    #endif

    signal(SIGPIPE, SIG_IGN);//ignore signals

    HashInit();

    printf("[WolframLibrary_initialize]\r\ninitialized\r\n\r\n"); 
    sem_init(&mutex, 0, 1);
    signal(SIGSEGV,Segfault_Handler);
    
    //sem_init(&mutex, 0, 1);

    return LIBRARY_NO_ERROR; 
}

DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libData) { 
    #ifdef _WIN32
        WSACleanup(); 
    #endif 
    emergencyExit = 1;
    
    for (int i=0; i<servers; ++i) {
        pipePacket_t cmd;
        cmd.type = 'E';

        write(serverList[i]->pipe[1], &cmd, sizeof(pipePacket_t));
    }
    
    
    SLEEP(1000 * ms);



    printf("[WolframLibrary_uninitialize]\r\nuninitialized\r\n\r\n"); 

    return; 
}

#pragma endregion

#pragma region internal 

static SOCKET currentSoketId = INVALID_SOCKET;

static void socketListenerTask(mint taskId, void* vtarg); 

int currentTime() {
    #ifdef _WIN32
        SYSTEMTIME st, lt;
    
        GetSystemTime(&st);
        GetLocalTime(&lt);
    
        printf("%d.%d\n", st.wSecond, st.wMilliseconds);
    #endif

    return 0;
}

int socketWrite(SOCKET socketId, BYTE *buf, unsigned long dataLength, int bufferSize) { 
    /*int iResult; 
    int writeLength; 
    char *buffer; 
    int errno;   
    SOCKET currentSoketIdBackup; */


    unsigned long total = 0;        // how many bytes we've sent
    unsigned long bytesleft = dataLength; // how many we have left to send
    long n;

    //int trials = 0;

    //try until get an error of an overflow
    while(total < dataLength) {
        n = send(socketId, buf+total, bytesleft, 0);
        if (n == SOCKET_ERROR) { break; }
        total += n;
        bytesleft -= n;

        //trials++;
        printf("[send] wroom-wroom for %ld\r\n", socketId);

        /*if (trials > 100) {
            printf("[socketWrite]\r\nfuck it!\r\n\r\n"); 
            n = SOCKET_ERROR;
            break;
        }*/
    }

    if (n == SOCKET_ERROR) {
        int err = GETSOCKETERRNO();
        printf("[socketWrite]\r\nerror %d for fd %ld\r\n\r\n", err, socketId); 
        if (err == 35 || err == 10035) {
            //overflow of a buffer. Put the rest to the que
            printf("[socketWrite]\r\n Next time!\r\n\r\n");
            printf("[socketWrite]\r\n leftover bytes %ld\r\n\r\n", bytesleft);
            
            return total;
        }
        return SOCKET_ERROR; 
    }

    return total;
}

MNumericArray createByteArray(WolframLibraryData libData, BYTE *data, const mint dataLength){
    MNumericArray nArray;
    libData->numericarrayLibraryFunctions->MNumericArray_new(MNumericArray_Type_UBit8, 1, &dataLength, &nArray);
    memcpy((uint8_t*) libData->numericarrayLibraryFunctions->MNumericArray_getData(nArray), data, dataLength);
    return nArray;
}



#pragma endregion

#pragma region socketOpen[host_String, port_String]: socketId_Integer 

DLLEXPORT int socketOpen(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    char* host = MArgument_getUTF8String(Args[0]);
    char* port = MArgument_getUTF8String(Args[1]);
    
    int iResult; 
    SOCKET listenSocket = INVALID_SOCKET; 
    struct addrinfo hints; 
    struct addrinfo *address = NULL; 
    int iMode = 1;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(host, port, &hints, &address);
    if (iResult != 0) {
        printf("[socketOpen]\r\ngetaddrinfo error: %d\r\n\r\n", iResult);
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

    listenSocket = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (!ISVALIDSOCKET(listenSocket)) {
        printf("[socketOpen]\r\nsocket error: %d\r\n\r\n", (int)GETSOCKETERRNO());
        freeaddrinfo(address);
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

    iResult = bind(listenSocket, address->ai_addr, (int)address->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("[socketOpen]\r\nbind error: %d\r\n\r\n", (int)GETSOCKETERRNO());
        CLOSESOCKET(listenSocket);
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

    iResult = listen(listenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("[socketOpen]\r\nerror during call listen(%d)\r\n\r\n", (int)listenSocket);
        CLOSESOCKET(listenSocket);
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

    #ifdef _WIN32 
    iResult = ioctlsocket(listenSocket, FIONBIO, &iMode); 
    #else
    iResult = fcntl(listenSocket, O_NONBLOCK | SO_REUSEADDR, &iMode); 

    //to prevent OS holding address after exit
    int reuse = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    #ifdef SO_REUSEPORT
        if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
            perror("setsockopt(SO_REUSEPORT) failed");
    #endif    
    #endif

    if (iResult != NO_ERROR) {
        printf("[socketOpen]\r\nioctlsocket failed with error: %d\r\n\r\n", iResult);
    } else {
        HashAllocate(listenSocket, 0);
        wSocketsSet(listenSocket, 1);
    }

    freeaddrinfo(address);

    printf("[socketOpen]\r\nopened socket id: %d\r\n\r\n", (int)listenSocket);
    MArgument_setInteger(Res, listenSocket);
    return LIBRARY_NO_ERROR; 
}

#pragma endregion

#pragma region socketClose[socketId_Integer]: socketId_Integer 

DLLEXPORT int socketClose(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    SOCKET socketId = MArgument_getInteger(Args[0]);
    printf("[socketClose]\r\nsocket id: %d\r\n\r\n", (int)socketId);
    int res = 0;
    //better to add to the query for PIPE!!!
    if (wSocketsGetState(socketId) != INVALID_SOCKET) {
        pipePacket_t packet;
        packet.type = 'C';
        packet.socketId = socketId;
        write(wSocketsGetPipe(socketId)[1], &packet, sizeof(pipePacket_t));
    } else {
        printf("[socketClose]\r\ns already closed! id: %d\r\n\r\n", (int)socketId);
    }
    //do we actually need to close it?

    MArgument_setInteger(Res, res);
    return LIBRARY_NO_ERROR; 
}

#pragma endregion

#pragma region socketListen[socketid_Integer, bufferSize_Integer]: taskId_Integer 

DLLEXPORT int socketListen(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    SOCKET listenSocket = MArgument_getInteger(Args[0]);
    int bufferSize = MArgument_getInteger(Args[1]);
    
    mint taskId;
    SOCKET *clients = (SOCKET*)malloc(4 * sizeof(SOCKET));
    Server server = (Server)malloc(sizeof(struct Server_st));

    server->listenSocket=listenSocket;
    server->clients=clients;
    server->clientsLength=0;
    server->clientsLengthMax=4;
    server->bufferSize=bufferSize;


    //create a pipe
    int *fd = (int*)malloc(sizeof(int)*2); 
    pipe(fd);

    server->pipe = fd;

    SocketListenerTaskArgs threadArg = (SocketListenerTaskArgs)malloc(sizeof(struct SocketListenerTaskArgs_st));
    threadArg->libData=libData; 
    threadArg->pipe = fd;
    threadArg->server=server; 
    taskId = libData->ioLibraryFunctions->createAsynchronousTaskWithThread(socketListenerTask, threadArg);

    
    serverList[servers] = server;
    servers++;

    printf("[socketListen]\r\nlistening task id: %d\r\n\r\n", (int)taskId);
    MArgument_setInteger(Res, taskId); 
    return LIBRARY_NO_ERROR; 
}

/*static void socketListenerConnectionTask(mint taskId, void* vtarg) {
    SocketListenerTaskArgs targ = (SocketListenerTaskArgs)vtarg;
	WolframLibraryData libData = targ->libData;

    int* pipe = targ->pipe;

    int iResult;
    char *buffer = (char*)malloc(2*4096 * sizeof(char));
    mint dims[1];
    MNumericArray data;
	DataStore ds;

    signal(SIGPIPE, SIG_IGN);//ignore signals

    //allocating que
    wQuery_t** wQuery = (wQuery_t**)malloc(sizeof(wQuery_t*)*wQuery_size);
    wQueryInit(wQuery);
    //add server's socket
    //wSocketsSetwQuery(server->listenSocket, wQuery);


    //allocating POLL
    struct pollfd poll_set[POLL_SIZE];
    int numfds = 0;

    //adding server
    memset(poll_set, '\0', sizeof(poll_set));

    //adding a pipe
    poll_set[numfds].fd = pipe[0];
    poll_set[numfds].events = POLLIN;

    pipePacket_t* cmd = (pipePacket_t*)malloc(sizeof(pipePacket_t));

    BYTE* wbuffer;

    numfds++;  

    int offset = numfds;  
    
    while(emergencyExit == 0) {
	//while(libData->ioLibraryFunctions->asynchronousTaskAliveQ(taskId) && emergencyExit == 0)
        printf("[socketListenerConnectionTask]\r\n waiting... \r\n\r\n");
        poll(poll_set, numfds, -1);
        printf("[socketListenerConnectionTask]\r\n new event! \r\n\r\n");

        for(int fd_index = 0; fd_index < numfds; fd_index++) {
            if( poll_set[fd_index].revents ) {
                if (poll_set[fd_index].fd == pipe[0]) {
                    
                    int result = read(pipe[0], cmd, sizeof(pipePacket_t));
                    if (result != sizeof(pipePacket_t)) {
                        perror("read");
                        exit(3);
                    }

                    printf("Pipe cmd: %c\n", cmd->type);

                    switch(cmd->type) {
                        case 'P':
                            printf("[poll] poke!\r\n");
  
                            printf("[poll] locked state\r\n");
                            int st = pokeWriteQuery(wQuery);
                            printf("[poll] unlock\r\n");
   

                            if (st == ST_NEXT) {
                                pipePacket_t packet;
                                packet.type = 'P';
                                //poke itself
                                write(pipe[1], &packet, sizeof(pipePacket_t));
                            }

                            if (st == ST_WAIT) {
                                pipePacket_t packet;
                                packet.type = 'P';
                                //poke itself
                                write(pipe[1], &packet, sizeof(pipePacket_t));
                                //tried to use timers, but works only on Linux ;()
                                SLEEP(ms);
                            }

                        break;

                        case 'C':

                            wSocketsSet(cmd->socketId, INVALID_SOCKET);
                            HashFree(cmd->socketId, 0);
                            CLOSESOCKET(cmd->socketId);

                            //looking for it in the pool...
                            for (int j=0; j<POLL_SIZE; ++j) {
                                if (poll_set[j].fd == cmd->socketId) {
                                    printf("removing it from the poll pool...\r\n");
                                    poll_set[j].events = 0;
                                    if (numfds > 1) {
                                        poll_set[j] = poll_set[numfds - 1];
                                    }
                                    numfds--;  
                                    break;
                                }
                            }

                            printf("done!\r\n");
                        break;

                        case 'E':
                            emergencyExit = -1;
                            fd_index = numfds;
                        break;
                    }

                } else {
                    //ioctl(poll_set[fd_index].fd, FIONREAD, &nread);

                    //read(poll_set[fd_index].fd, &ch, 1);
                    printf("Serving client on fd %d\n", poll_set[fd_index].fd);

                    if (wSocketsGetState(poll_set[fd_index].fd) == INVALID_SOCKET) {
                        printf("oupps... already closed %d\n", poll_set[fd_index].fd);
                        printf("Removing client on fd %d\n", poll_set[fd_index].fd);
                        int i;
                        if (numfds > 1) {
                            poll_set[fd_index] = poll_set[numfds - 1];
                        } else {
                            poll_set[fd_index].events = 0;
                        }
                        numfds--;  
                        continue;                        
                    }
                    //ch++;
                    //write(poll_set[fd_index].fd, &ch, 1);

                    iResult = recv(poll_set[fd_index].fd, buffer, 4096*2, 0); 
                    if (iResult > 0){
                        printf("[socketListenerConnectionTask]\r\nrecv %d bytes from %d\r\n\r\n", iResult, (int)poll_set[fd_index].fd);
                        dims[0] = iResult;
                        libData->numericarrayLibraryFunctions->MNumericArray_new(MNumericArray_Type_UBit8, 1, dims, &data); 
                        memcpy(libData->numericarrayLibraryFunctions->MNumericArray_getData(data), buffer, iResult);

                    } else if (iResult == 0) {
                        printf("[socketListenerConnectionTask]\r\nclient %d closed\r\n\r\n", (int)poll_set[fd_index].fd);
                        //poolingPoolDelete(poll_set[fd_index].fd);
                        //server->clients[i] = INVALID_SOCKET;
                        wSocketsSet(poll_set[fd_index].fd, INVALID_SOCKET);
                        HashFree(poll_set[fd_index].fd, 0);
                        close(poll_set[fd_index].fd);
                        //HashFree(poll_set[fd_index].fd);

                        //poll_set[fd_index].events = 0;
                        printf("Removing client on fd %d\n", poll_set[fd_index].fd);
                        int i;
                        if (numfds > 1) {
                            poll_set[fd_index] = poll_set[numfds - 1];
                        } else {
                            poll_set[fd_index].events = 0;
                        }
                        numfds--;                        
                    } else  {
                        printf("[socketListenerConnectionTask]\r\nclient %d might be broken; error: %d\r\n\r\n", (int)poll_set[fd_index].fd, (int)GETSOCKETERRNO());


                    }

                }
            }
        
        }
    }   
}*/

static void socketListenerTask(mint taskId, void* vtarg)
{
    SocketListenerTaskArgs targ = (SocketListenerTaskArgs)vtarg;
    Server server = targ->server;
	WolframLibraryData libData = targ->libData;

    int* pipe = targ->pipe;

    int iResult;
    SOCKET clientSocket = INVALID_SOCKET;
    char *buffer = (char*)malloc(server->bufferSize * sizeof(char));
    mint dims[1];
    MNumericArray data;
	DataStore ds;

    signal(SIGPIPE, SIG_IGN);//ignore signals

    //allocating que
    wStack_t* stack = (wStack_t*)malloc(sizeof(wStack_t));
    stack->cursor = -1;
    //add server's socket
    //wSocketsSetwQuery(server->listenSocket, wQuery);


    //allocating POLL
    struct pollfd poll_set[POLL_SIZE];
    int numfds = 0;

    //adding server
    memset(poll_set, '\0', sizeof(poll_set));
    poll_set[numfds].fd = server->listenSocket;
    poll_set[numfds].events = POLLIN;

    numfds++;

    //adding a pipe
    poll_set[numfds].fd = pipe[0];
    poll_set[numfds].events = POLLIN;

    pipePacket_t* cmd = (pipePacket_t*)malloc(sizeof(pipePacket_t));

    BYTE* wbuffer;

    numfds++;  

    int offset = numfds;  
    
    while(emergencyExit == 0 && libData->ioLibraryFunctions->asynchronousTaskAliveQ(taskId)) {
	//while(libData->ioLibraryFunctions->asynchronousTaskAliveQ(taskId) && emergencyExit == 0)
        printf("[socketListenerTask]\r\n waiting... \r\n\r\n");
        poll(poll_set, numfds, -1);
        printf("[socketListenerTask]\r\n new event! \r\n\r\n");

        for(int fd_index = 0; fd_index < numfds; fd_index++) {
            if( poll_set[fd_index].revents ) {
                if (poll_set[fd_index].fd == server->listenSocket) {

                    clientSocket = accept(server->listenSocket, NULL, NULL); 
                    if (ISVALIDSOCKET(clientSocket)) {
                        printf("[socketListenerTask]\r\nnew client: %d\r\n\r\n", (int)clientSocket);
                        printf("[socketListenerTask]\r\npoll size: %d\r\n\r\n", (int)numfds);

                        HashAllocate(clientSocket, 0);
                        wSocketsSet(clientSocket, 1);
                        wSocketsSetPipe(clientSocket, pipe);
                        wSocketsSetwStack(clientSocket, stack);
                        
                        /*server->clients[server->clientsLength] = clientSocket;
                        //poolingPoolPush(clientSocket);
                        server->clientsLength++;
                        printf("[socketListenerTask]\r\nclients length: %d\r\n\r\n", (int)server->clientsLength);

                        if (server->clientsLength == server->clientsLengthMax) {
                            server->clientsLengthMax *= 2; 
                            server->clients = realloc(server->clients, server->clientsLengthMax * sizeof(SOCKET)); 
                        }*/

                        poll_set[numfds].fd = clientSocket;
                        poll_set[numfds].events = POLLIN;
                        numfds++;

                        printf("[poll] Adding client on fd %d\n", clientSocket);
                    } else {
                        printf("[poll] Cannot accept socket.. Problem with it at %ld\n", clientSocket);
                    }

                } else if (poll_set[fd_index].fd == pipe[0]) {
                    
                    
                    int result = read(pipe[0], cmd, sizeof(pipePacket_t));
                    if (result != sizeof(pipePacket_t)) {
                        perror("read");
                        exit(3);
                    }

                    printf("Pipe cmd: %c\n", cmd->type);

                    switch(cmd->type) {
                        case 'P':
                            printf("[poll] poke!\r\n");
             
                            printf("[poll] locked state\r\n");
                            int st = pokeWriteQuery(stack);
                            printf("[poll] unlocked\r\n");
                   

                            if (st == ST_NEXT) {
                                pipePacket_t packet;
                                packet.type = 'P';
                                //poke itself
                                write(pipe[1], &packet, sizeof(pipePacket_t));
                            }

                            if (st == ST_WAIT) {
                                pipePacket_t packet;
                                packet.type = 'P';
                                //poke itself
                                write(pipe[1], &packet, sizeof(pipePacket_t));
                                //tried to use timers, but works only on Linux ;()
                                SLEEP(1 * ms);
                            }

                        break;

                        case 'C':
                            if (wSocketsGetState(cmd->socketId) == INVALID_SOCKET) {
                                printf("[c] socket %ld was already closed!\r\n", cmd->socketId);
                                //break the loop
                                fd_index = numfds;
                                break;
                            }

                            printf("[c] closing socket %ld...\r\n", cmd->socketId);
                            wSocketsSet(cmd->socketId, INVALID_SOCKET);
                            HashFree(cmd->socketId, 0);
                            CLOSESOCKET(cmd->socketId);

                            //looking for it in the pool...
                            for (int j=0; j<POLL_SIZE; ++j) {
                                if (poll_set[j].fd == cmd->socketId) {
                                    printf("removing it from the poll pool...\r\n");
                                    poll_set[j].events = 0;
                                    poll_set[j].fd = -1;
                                    poll_set[j] = poll_set[numfds - 1];
                                    numfds--;  
                                    break;
                                }
                            }

                            printf("done!\r\n");
                            fd_index = numfds;
                            
                        break;

                        case 'E':
                            emergencyExit = -1;
                            fd_index = numfds;
                        break;
                    }

                } else {
                    //ioctl(poll_set[fd_index].fd, FIONREAD, &nread);

                    //read(poll_set[fd_index].fd, &ch, 1);
                    printf("Serving client on fd %d\n", poll_set[fd_index].fd);

                    if (wSocketsGetState(poll_set[fd_index].fd) == INVALID_SOCKET) {
                        printf("oupps... already closed %d\n", poll_set[fd_index].fd);
                        continue;                        
                    }
                    //ch++;
                    //write(poll_set[fd_index].fd, &ch, 1);

                    iResult = recv(poll_set[fd_index].fd, buffer, server->bufferSize, 0); 
                    if (iResult > 0){
                        printf("[socketListenerTask]\r\nrecv %d bytes from %d\r\n\r\n", iResult, (int)poll_set[fd_index].fd);
                        dims[0] = iResult;
                        libData->numericarrayLibraryFunctions->MNumericArray_new(MNumericArray_Type_UBit8, 1, dims, &data); 
                        memcpy(libData->numericarrayLibraryFunctions->MNumericArray_getData(data), buffer, iResult);
                        ds = libData->ioLibraryFunctions->createDataStore();
                        libData->ioLibraryFunctions->DataStore_addInteger(ds, server->listenSocket);
                        libData->ioLibraryFunctions->DataStore_addInteger(ds, poll_set[fd_index].fd);
                        libData->ioLibraryFunctions->DataStore_addMNumericArray(ds, data);
                        libData->ioLibraryFunctions->raiseAsyncEvent(taskId, "Received", ds);
                    } else if (iResult == 0) {
                        printf("[socketListenerTask recv]\r\nclient %d will be closed\r\n\r\n", (int)poll_set[fd_index].fd);
                        //poolingPoolDelete(poll_set[fd_index].fd);
                        //server->clients[i] = INVALID_SOCKET;
                        //wSocketsSet(poll_set[fd_index].fd, INVALID_SOCKET);
                        pipePacket_t packet;
                        packet.type = 'C';
                        packet.socketId = poll_set[fd_index].fd;
                        write(pipe[1], &packet, sizeof(pipePacket_t));

                    } else  {
                        printf("[socketListenerTask]\r\nclient %d might be broken; error: %d\r\n\r\n", (int)poll_set[fd_index].fd, (int)GETSOCKETERRNO());
                        /*                 
                        //wSocketsSet(poll_set[fd_index].fd, INVALID_SOCKET);
                        close(poll_set[fd_index].fd);
                        wSocketsSet(poll_set[fd_index].fd, INVALID_SOCKET);
                        HashFree(poll_set[fd_index].fd, 0);
                        //poll_set[fd_index].events = 0;
                        printf("Removing client on fd %d\n", poll_set[fd_index].fd);
                        int i;
                        if (numfds > 1) {
                            poll_set[fd_index] = poll_set[numfds - 1];
                        } else {
                            poll_set[fd_index].events = 0;
                        }
                        numfds--;  */

                    }

                }
            }
        
        }
    }

    printf("[socketListenerTask]\r\nremoveAsynchronousTask: %d\r\n\r\n", (int)taskId);
    for (int i = offset; i < numfds; i++)
    {
        printf("[socketListenerTask]\r\nclose client: %d\r\n\r\n", (int) poll_set[i].fd);
        CLOSESOCKET(poll_set[i].fd);
        wSocketsSet(poll_set[i].fd, INVALID_SOCKET);
        HashFree(poll_set[i].fd, 0);
    }

    //close server
    CLOSESOCKET(poll_set[0].fd);
    wSocketsSet(poll_set[0].fd, INVALID_SOCKET);
    HashFree(poll_set[0].fd, 0);

    //free(targ); 
    //free(server->clients);
    //free(buffer);

    printf("[socketListenerTask]\r\ndone!\r\n\r\n");
}


#pragma endregion

#pragma region socketListenerTaskRemove[taskId_Integer]: taskId_Integer 

DLLEXPORT int socketListenerTaskRemove(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    mint taskId = MArgument_getInteger(Args[0]);
    printf("[socketListenerTaskRemove]\r\nremoved task id: %d\r\n\r\n", (int)taskId);
    MArgument_setInteger(Res, libData->ioLibraryFunctions->removeAsynchronousTask(taskId));
    return LIBRARY_NO_ERROR;
}

#pragma endregion

#pragma region socketConnect[host_String, port_String]: socketId_Integer 

DLLEXPORT int socketConnect(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    char *host = MArgument_getUTF8String(Args[0]);
    char *port = MArgument_getUTF8String(Args[1]);

    int iResult; 
    int iMode = 1; 
    SOCKET connectSocket = INVALID_SOCKET; 
    struct addrinfo *address = NULL; 
    struct addrinfo hints; 

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(host, port, &hints, &address);
    if (iResult != 0){
        printf("[socketConnect]\r\ngetaddrinfo error: %d\r\n\r\n", iResult);
        return LIBRARY_FUNCTION_ERROR; 
    }

    connectSocket = socket(address->ai_family, address->ai_socktype, address->ai_protocol); 
    if (connectSocket == INVALID_SOCKET){
        printf("[socketConnect]\r\nsocket error: %d\r\n\r\n", GETSOCKETERRNO());
        freeaddrinfo(address); 
        return LIBRARY_FUNCTION_ERROR; 
    }

    iResult = connect(connectSocket, address->ai_addr, (int)address->ai_addrlen);
    freeaddrinfo(address);
    if (iResult == SOCKET_ERROR) {
        printf("[socketConnect]\r\nconnect error: %d\r\n\r\n", GETSOCKETERRNO());
        CLOSESOCKET(connectSocket); 
        connectSocket = INVALID_SOCKET;
        return LIBRARY_FUNCTION_ERROR;
    }

    #ifdef _WIN32 
    iResult = ioctlsocket(connectSocket, FIONBIO, &iMode); 
    #else
    iResult = fcntl(connectSocket, O_NONBLOCK, &iMode); 
    #endif

    if (iResult != NO_ERROR) {
        printf("[socketOpen]\r\nioctlsocket failed with error: %d\r\n\r\n", iResult);
    }



    if (globalListener < 0) {
        //create a pipe
        int *fd = (int*)malloc(sizeof(int)*2); 
        pipe(fd);

        SocketListenerTaskArgs threadArg = (SocketListenerTaskArgs)malloc(sizeof(struct SocketListenerTaskArgs_st));
        threadArg->libData=libData; 
        threadArg->pipe = fd;
        //libData->ioLibraryFunctions->createAsynchronousTaskWithThread(socketListenerConnectionTask, threadArg);
        globalListener = 0;

        globalPipe = fd;
    }

    //send command to a globalPipe;
    //globalPipe


    MArgument_setInteger(Res, connectSocket); 
    return LIBRARY_NO_ERROR;
}

#pragma endregion

#pragma region socketBinaryWrite[socketId_Integer, data: ByteArray[<>], dataLength_Integer, bufferLength_Integer]: socketId_Integer 

DLLEXPORT int socketBinaryWrite(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    ////sem_wait(&mutex);

    SOCKET clientId = MArgument_getInteger(Args[0]); 
    MNumericArray mArr = MArgument_getMNumericArray(Args[1]); 
    int iResult;
    BYTE *data = (BYTE *)libData->numericarrayLibraryFunctions->MNumericArray_getData(mArr); 
    int dataLength = MArgument_getInteger(Args[2]); 
    int bufferSize = MArgument_getInteger(Args[3]); 
    
    

    /*iResult = socketWrite(clientId, data, dataLength, bufferSize); 
    if (iResult == SOCKET_ERROR) {
        printf("[socketWrite]\r\n\tsend failed with error: %d\r\n\r\n", (int)GETSOCKETERRNO());
        CLOSESOCKET(clientId);
        MArgument_setInteger(Res, GETSOCKETERRNO()); 
        return LIBRARY_FUNCTION_ERROR; 
    }*/
    pipePacket_t packet;
    int state = wSocketsGetState(clientId);
    wStack_t* w = wSocketsGetwStack(clientId);

    switch (state) {
        case INVALID_SOCKET:
            printf("[socketBinaryWrite]\r\n\tsend failed with error: %d\r\n\r\n", (int)SOCKET_ERROR);
            MArgument_setInteger(Res, -1); 
            //sem_post(&mutex);
            return LIBRARY_NO_ERROR;  

        case BLOCKING_SOCKET:
            printf("[socketBinaryWrite]\r\n\tnot supported! %d\r\n\r\n", (int)SOCKET_ERROR);
            return LIBRARY_NO_ERROR;  
        break;


        default:
            //addToWriteQuery(clientId, data, dataLength);

            //trigger the second thread safely
    
            addToWriteQuery(clientId, data, dataLength, w);
      
            packet.type = 'P';
            int fd = wSocketsGetPipe(clientId)[1];
            printf("[socketBinaryWrite]\r\n\tsent a trigger via pipe %d\r\n\r\n", 0);
            write(fd, &packet, sizeof(pipePacket_t));

            /*
            packet.type = 'W';
            packet.socketId = clientId;
            packet.payload = dataLength;
            int fd = wSocketsGetPipe(clientId)[1];
            printf("[socketBinaryWrite]\r\n\tsent via pipe %d\r\n\r\n", 0);
            write(fd, &packet, sizeof(pipePacket_t));
            write(fd, data, dataLength);*/

            MArgument_setInteger(Res, clientId);
            return LIBRARY_NO_ERROR;
    }


}

#pragma endregion

#pragma region socketWriteString[socketId_Integer, data_String, dataLength_Integer, bufferSize_Integer]: socketId_Integer 

DLLEXPORT int socketWriteString(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    ////sem_wait(&mutex);

    int iResult; 
    SOCKET socketId = MArgument_getInteger(Args[0]); 
    char* data = MArgument_getUTF8String(Args[1]); 
    int dataLength = MArgument_getInteger(Args[2]); 
    int bufferSize = MArgument_getInteger(Args[3]); 
    
    /*iResult = socketWrite(socketId, data, dataLength, bufferSize); 
    if (iResult == SOCKET_ERROR) {
        printf("[socketWriteString]\r\nsend failed with error: %d\r\n\r\n", (int)GETSOCKETERRNO());
        CLOSESOCKET(socketId);
        MArgument_setInteger(Res, GETSOCKETERRNO()); 
        return LIBRARY_FUNCTION_ERROR; 
    }*/
    int state = wSocketsGetState(socketId);
    wStack_t* w = wSocketsGetwStack(socketId);
    pipePacket_t packet;

    switch (state) {
        case INVALID_SOCKET:
            printf("[socketBinaryWrite]\r\n\tsend failed with error: %d\r\n\r\n", (int)SOCKET_ERROR);
            MArgument_setInteger(Res, -1); 
            //sem_post(&mutex);
            return LIBRARY_NO_ERROR;  

        case BLOCKING_SOCKET:
            printf("[socketBinaryWrite]\r\n\tnot supported! %d\r\n\r\n", (int)SOCKET_ERROR);
        break;


        default:
            //addToWriteQuery(socketId, data, dataLength);

            //trigger the second thread safely
      
            addToWriteQuery(socketId, data, dataLength, w);
       
            packet.type = 'P';
            int fd = wSocketsGetPipe(socketId)[1];
            printf("[socketBinaryWrite]\r\n\tsent a trigger via pipe %d\r\n\r\n", 0);
            write(fd, &packet, sizeof(pipePacket_t));

            MArgument_setInteger(Res, socketId);
            return LIBRARY_NO_ERROR;
    }
}

#pragma endregion

#pragma region socketReadyQ[socketId_Integer]: readyQ: True | False 

DLLEXPORT int socketReadyQ(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    SOCKET socketId = MArgument_getInteger(Args[0]); 
    
    int iResult; 
    BYTE *buffer = (BYTE *)malloc(sizeof(BYTE)); 
    
    iResult = recv(socketId, buffer, 1, MSG_PEEK);
    if (iResult == SOCKET_ERROR){
        MArgument_setBoolean(Res, False); 
    } else {
        MArgument_setBoolean(Res, True); 
    }

    free(buffer);
    return LIBRARY_NO_ERROR;
}

#pragma endregion

#pragma region socketReadMessage[socketId_Integer, bufferSize_Integer]: ByteArray[<>] 

DLLEXPORT int socketReadMessage(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    SOCKET socketId = MArgument_getInteger(Args[0]);
    int bufferSize = MArgument_getInteger(Args[1]);
    
    BYTE *buffer = (BYTE*)malloc(bufferSize * sizeof(BYTE));
    int iResult;
    int length = 0;

    iResult = recv(socketId, buffer, bufferSize, 0);
    if (iResult > 0) {
        printf("[socketReadMessage]\r\nreceived %d bytes\r\n\r\n", iResult);
        MArgument_setMNumericArray(Res, createByteArray(libData, buffer, iResult));
    } else {
        return LIBRARY_FUNCTION_ERROR;
    }

    return LIBRARY_NO_ERROR; 
}

#pragma endregion

#pragma region socketPort[socketId_Integer]: port_Integer

DLLEXPORT int socketPort(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    SOCKET socketId = MArgument_getInteger(Args[0]); 
    struct  sockaddr_in sin;
    int port;
    int addrlen = sizeof(sin);

    getsockname(socketId, (struct sockaddr *)&sin, &addrlen);
    port = ntohs(sin.sin_port); 

    printf("[sockePort]\r\nsocketId: %d and port: %d\r\n\r\n", (int)socketId, port);
    MArgument_setInteger(Res, port);
    return LIBRARY_NO_ERROR; 
}

#pragma endregion
