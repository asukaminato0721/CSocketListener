#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <stdbool.h>

#include "WolframLibrary.h"
#include "WolframIOLibraryFunctions.h"
#include "WolframNumericArrayLibrary.h"

uv_loop_t *uvloop;
uv_async_t poke;

mint asyncObjGlobal;

static uv_mutex_t mutex;

//global objects to be accessed from everywhere
WolframIOLibrary_Functions ioLibrary;
WolframNumericArrayLibrary_Functions numericLibrary;

#define SOCKET mint

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;



typedef struct {
    SOCKET socket;

    void (*ref)(void*);

    char* host;
    char* port;

    mint asyncObjID;
    long length;

    char* buf;
} packet_t;

packet_t* que[128];
int w_cursor = 0;
int r_cursor = 0;

#define U_INVALID -1
#define U_VALID 1

#define HASH_FREE -199
#define HASH_NEXT 33
#define HASH_OCCUPIED 71

typedef struct {
    SOCKET socketId;
    SOCKET serverId;
    
    int state;

    int _flag;
} uState_t;


#define hashmap_size 4096
uState_t uState[hashmap_size];

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

unsigned long HashAllocate(SOCKET socketId, int offset);

void HashCopy(SOCKET socketId, int offsetSrc, int offsetDest) {
    unsigned long hS = hash((unsigned long)socketId, offsetSrc);
    printf("hash >> allocate for a copy\n");
    unsigned long hD = HashAllocate((unsigned long)socketId, offsetDest);

    printf("hash >> copied\n");

    memcpy(&uState[hD], &uState[hS], sizeof(uState_t));
}

//helper functions to check the status of the socket
unsigned long HashAllocate(SOCKET socketId, int offset) {
    printf("hash >> allocate %ld with offset %ld\n", (unsigned long)socketId, offset);
    unsigned long h = hash((unsigned long)socketId, offset);
    printf("hash >> %ld\n", h);

    if (uState[h]._flag == HASH_OCCUPIED) {
        printf("hash >> collizion!\n");

        //copy the original value
        printf("hash >> copy old one %ld\n", (unsigned long)uState[h].socketId);
        HashCopy(uState[h].socketId, offset, offset + 1);

        uState[h]._flag = HASH_NEXT;
        return HashAllocate(socketId, offset + 1);
    }

    if (uState[h]._flag == HASH_NEXT) {
        printf("hash >> next\n");
        return HashAllocate(socketId, offset + 1);
    }

    printf("hash >> ok!\n");
    uState[h]._flag = HASH_OCCUPIED;
    uState[h].socketId = socketId;

    return h;
}

void HashInit() {
    for (int i=0; i<hashmap_size; ++i) {
        uState[i]._flag = HASH_FREE;
        uState[i].state = U_INVALID;
    }
}

void HashFree(SOCKET socketId, int offset) {
    printf("hash >> freeing %ld\n", (unsigned long)socketId);
    unsigned long h = hash((unsigned long)socketId, offset);
    if (uState[h]._flag == HASH_NEXT) {
        return HashFree(socketId, offset + 1);
    }

    if (uState[h]._flag == HASH_OCCUPIED) {
        uState[h]._flag = HASH_FREE;
        return;
    }

    printf("hash >> already freed!\n");
}

unsigned long HashGet(SOCKET socketId, int offset) {
    printf("[HashGet] get\r\n\r\n");
    unsigned long h = hash((unsigned long)socketId, offset);
    if (uState[h]._flag == HASH_NEXT) {
        printf("[HashGet] next\r\n\r\n");
        return HashGet(socketId, offset + 1);
    }
    printf("[HashGet] done\r\n\r\n");

    return h;
}

uState_t uinvalid = {
    .state = U_INVALID,
    .socketId = -1,
    .serverId = -1,
    ._flag = HASH_OCCUPIED
};

uState_t* uGetState(SOCKET socketId) {
    printf("[uGetState] get state\r\n");
    unsigned long h = HashGet(socketId, 0);
    if (uState[h].socketId != socketId || uState[h]._flag == HASH_FREE) {
        printf("[uGetState] probably it is gone already\r\n\r\n");
        return 0;
    }
    return &uState[h];
}

/*void uSetState(SOCKET socketId, uState_t st) {
    printf("[uSetState] set state\r\n");
    unsigned long h = HashGet(socketId, 0);
    if (uState[h].socketId != socketId || uState[h]._flag == HASH_FREE) {
        printf("[uSetState] probably it is gone already\r\n\r\n");
        return;
    }
    
    uState[h].socketId = st.socketId;
    uState[h].serverId = st.serverId;
    uState[h].state = st.state;
}*/


void async_handleQue(uv_async_t* async, int status);

void pipeInit(packet_t* p) {
    printf("[pipeInit] \r\n");
    for (int i=0; i<128; ++i) que[i] = 0;
}

int pipePush(packet_t* p) {
    uv_sleep(1000);
    printf("[pipePush] cursor: %d\r\n", w_cursor);
    uv_mutex_lock(&mutex);
    //printf("[pipePush] after mutex \r\n");
    que[w_cursor] = p;

    w_cursor++;
    if (w_cursor > 127) w_cursor = 0;

    printf("[pipePush] uv_async_send\r\n");
    
    //
  
    //printf("[pipePush] before unlock mutex\r\n");
    uv_mutex_unlock(&mutex);

    //uv_async_send(&poke);
    uv_async_t *message = (uv_async_t*)malloc(sizeof(uv_async_t));
    uv_async_init(uvloop, message, async_handleQue);
    uv_async_send(message);     
    //printf("[pipePush] after unlock mutex\r\n");
    return 0;
}

packet_t* pipePop() {
    uv_sleep(1000);
    printf("[pipePop] cursor: %d\r\n", r_cursor);
    //printf("[pipePop] before mutex\r\n");
    uv_mutex_lock(&mutex);
    //printf("[pipePop] after mutex\r\n");
    if (r_cursor == w_cursor) {
        //printf("[pipePop] before mutex unlock\r\n");
        if (que[r_cursor] != 0) {
            printf("fatal error!\r\n");
            exit(-1);
        }

        uv_mutex_unlock(&mutex);
        //printf("[pipePop] after mutex unlock\r\n");        
        return 0;
    }

    if (r_cursor > w_cursor) {
        printf("fatal error racing!\r\n");
        exit(-1);
    }
    

    printf("[pipePop] something is there \r\n");
    packet_t* r = que[r_cursor];
    que[r_cursor] = 0;

    r_cursor++;
    if (r_cursor > 127) r_cursor = 0;

    //printf("[pipePop] before mutex unlock\r\n");
    uv_mutex_unlock(&mutex);
    //printf("[pipePop] after mutex unlock\r\n");
    return r;
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    printf("[alloc_buffer]\r\n");
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

void pipe_buf (uv_stream_t *client, const uv_buf_t *buf) {
    printf("[pipe_buf]\r\n");
    SOCKET clientId = (SOCKET)client;
    
    uState_t *cli = uGetState(client);
    uState_t *srv = uGetState(cli->serverId);

    SOCKET serverId = cli->serverId;

    mint dims[1]; 
    MNumericArray data;

	DataStore ds;
    
    dims[0] = buf->len; 
    numericLibrary->MNumericArray_new(MNumericArray_Type_UBit8, 1, dims, &data); 
    memcpy(numericLibrary->MNumericArray_getData(data), buf->base, buf->len);
                
    ds = ioLibrary->createDataStore();
    ioLibrary->DataStore_addInteger(ds, serverId);
    ioLibrary->DataStore_addInteger(ds, clientId);
    ioLibrary->DataStore_addMNumericArray(ds, data);

    printf("[pipe_buf] raise async event %ld\r\n", asyncObjGlobal);
    //printf("raise async event %d for server %d and client %d\n", asyncObjID, streamId, clientId);
    ioLibrary->raiseAsyncEvent(asyncObjGlobal, "Received", ds);

    //free(buf->base);
}

void async_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    printf("[async_read]\r\n");
    uv_sleep(1000);

    if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "[async_read] Read error %s\n", uv_err_name(nread));
            uv_close((uv_handle_t*) client, NULL);
        }
    } else if (nread > 0) {
        pipe_buf(client, buf);
    }

    if (buf->base) {
        printf("[pipe_buf] free buffer\r\n");
        //free(buf->base);
    }
}

void on_new_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        printf("[on_new_connection] New connection error %s\r\n", uv_strerror(status));
        return;
    }

    printf("[on_new_connection] New connection\r\n");

    uv_tcp_t *c = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uvloop, c);

    if (uv_accept(server, (uv_stream_t*) c) == 0) {
        unsigned long h = HashAllocate(c, 0);
        uState[h].serverId = (SOCKET)server;
        uState[h].socketId = (SOCKET)c;
        uState[h].state = U_VALID;

        uv_read_start((uv_stream_t*) c, alloc_buffer, async_read);
    } else {
        printf("[on_new_connection] not accepted\r\n");
        if (uv_is_closing((uv_handle_t*) c) == 0)
            uv_close((uv_handle_t*) c, NULL);
    }
}

void socket_open(void* p) {
    printf("[socket_open] enter\r\n");
    uv_tcp_t* s = (uv_tcp_t*)((packet_t*)p)->socket;
    printf("[socket_open] assigned\r\n");
    uv_tcp_init(uvloop, s);
    printf("[socket_open] initiated\r\n");
    struct sockaddr_in* addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
    printf("[socket_open] ip\r\n");
    uv_ip4_addr(((packet_t*)p)->host, atoi(((packet_t*)p)->port), addr);
    printf("[socket_open] bind\r\n");
    int result = uv_tcp_bind(s, (const struct sockaddr*)addr, 0);
    printf("[socket_open] ok\r\n");
    if (result) {
        printf("[socket_open] bind error %s @ %s:%s\r\n", uv_strerror(result), ((packet_t*)p)->host, ((packet_t*)p)->port);
    }

    printf("[socket_open] has alloc\r\n");
    HashAllocate(((packet_t*)p)->socket, 0);

    uState_t *st = uGetState(((packet_t*)p)->socket);
    st->socketId = ((packet_t*)p)->socket;

    st->state = U_VALID;
    


    //free(((packet_t*)p)->host);
    //free(((packet_t*)p)->port);
}

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    //Here it fucks up
    free(wr->buf.base);
    free(wr);
}

void async_write(uv_write_t* wreq, int status) {
  printf("[async_write] \r\n");
  if (status) {
    fprintf(stderr, "uv_write error: %s\n", uv_err_name(status));
    
    free_write_req(wreq);
    return;
  }

  printf("[async_write] ok!\r\n");

  free_write_req(wreq);
}

void socket_write(void* p) {
    printf("[socket_write] enter\r\n");


    write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    req->buf = uv_buf_init(((packet_t*)p)->buf, ((packet_t*)p)->length);

    uv_write((uv_write_t*) req, (uv_stream_t*)((packet_t*)p)->socket, &req->buf, 1, async_write);
}

void socket_listen(void* p) {
    printf("[socket_listen] enter\r\n");
    uState_t *st = uGetState(((packet_t*)p)->socket);
    st->serverId = ((packet_t*)p)->asyncObjID;

    
    uv_tcp_t* s = (uv_tcp_t*)((packet_t*)p)->socket;
    int result = uv_listen((uv_stream_t*) s, 128, on_new_connection);
    if (result) {
        printf("[socket_listen] Listen error %s\n", uv_strerror(result));
    } else {
        printf("[socket_listen] OK\n");
    }
}

void async_handleQue(uv_async_t* async, int status) {
    printf("[async_handleQue]\r\n");
    packet_t* p;

    while((p = pipePop()) != 0) {
        printf("[async_handleQue] processing... \r\n");
        (*p->ref)(p);
        //free(p);
    }

    printf("[async_handleQue] close\r\n");
    uv_close((uv_handle_t*) async, NULL);
    printf("[async_handleQue] done\r\n");
}

DLLEXPORT mint WolframLibrary_getVersion() {
    printf("[WolframLibrary_getVersion]\r\nlibrary version: %d\r\n\r\n", WolframLibraryVersion);
    return WolframLibraryVersion;
}

void Segfault_Handler(int signo)
{
    fprintf(stderr,"\n[!] Oops! Segmentation fault...\n");
    exit(-1);
}

static void uvLoop(mint asyncObjID, void* vtarg)
{
    printf("[uvLoop] Event-Loop started! \n");
    
    uv_run(uvloop, UV_RUN_DEFAULT);

    printf("[uvLoop] you should not be here!!!! \n");
}

DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
    printf("[WolframLibrary_initialize]\r\ninitialized\r\n\r\n"); 
    signal(SIGSEGV,Segfault_Handler);

    printf("[WolframLibrary_initialize] creating uv task...\n");

    

    uv_mutex_init(&mutex);

    HashInit();

    uvloop = uv_default_loop();
    uv_async_init(uvloop, &poke, async_handleQue); 
    
    //ioLibrary->createAsynchronousTaskWithThread(uvLoop, NULL);   
   // uv_thread_t threads[1];
    //uv_thread_create(&threads[0], uvLoop, NULL);    

    return LIBRARY_NO_ERROR; 
}

DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libData) { 
    printf("[WolframLibrary_uninitialize]\r\nuninitialized\r\n\r\n"); 

    uv_stop(uvloop);
    //uv_loop_close(uvloop);

    return; 
}
DLLEXPORT int socketOpen(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    char* host = MArgument_getUTF8String(Args[0]);
    char* port = MArgument_getUTF8String(Args[1]);

    printf("[socketOpen]\r\n");

    uv_tcp_t* s = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    packet_t* packet = (packet_t*)malloc(sizeof(packet));
    packet->ref = &socket_open;
    packet->socket = (SOCKET)s;
    packet->host = "127.0.0.1";
    //memcpy(packet->host,  host, sizeof(char)*(strlen(host)+1));
    packet->port = "8010";
    //memcpy(packet->port,  port, sizeof(char)*(strlen(port)+1));    

    pipePush(packet);

    /**/
    
    MArgument_setInteger(Res, (SOCKET)s);

    return LIBRARY_NO_ERROR; 
}

DLLEXPORT int socketClose(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    SOCKET socketId = MArgument_getInteger(Args[0]);
    
    return LIBRARY_NO_ERROR; 
}

typedef struct SocketTaskArgs_st {
    WolframNumericArrayLibrary_Functions numericLibrary;
    WolframIOLibrary_Functions ioLibrary;
    mint garbage; 
}* SocketTaskArgs; 

DLLEXPORT int startLoop(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){

    printf("[socketListen]\r\n");
    SocketTaskArgs threadArg = (SocketTaskArgs)malloc(sizeof(struct SocketTaskArgs_st));
    mint asyncObjID = libData->ioLibraryFunctions->createAsynchronousTaskWithThread(uvLoop, threadArg);
    ioLibrary = libData->ioLibraryFunctions; 
    numericLibrary = libData->numericarrayLibraryFunctions;

    asyncObjGlobal = asyncObjID;

    MArgument_setInteger(Res, asyncObjID); 
    return LIBRARY_NO_ERROR; 
}

DLLEXPORT int socketListen(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    SOCKET listenSocket = MArgument_getInteger(Args[0]);

    printf("[socketListen]\r\n");

    //mint asyncObjID = ioLibrary->createAsynchronousTaskWithThread(dummy, NULL);

    packet_t* packet = (packet_t*)malloc(sizeof(packet));
    packet->ref = &socket_listen;
    packet->socket = listenSocket;  
    packet->asyncObjID = 0;

    pipePush(packet);

    MArgument_setInteger(Res, listenSocket); 
    return LIBRARY_NO_ERROR; 
}

DLLEXPORT int socketListenerTaskRemove(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    return LIBRARY_NO_ERROR;
}

DLLEXPORT int socketConnect(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){ 
    return LIBRARY_NO_ERROR;
}

DLLEXPORT int socketBinaryWrite(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    printf("[socketBinaryWrite]\r\n");

    SOCKET clientId = MArgument_getInteger(Args[0]); 
    MNumericArray mArr = MArgument_getMNumericArray(Args[1]); 

    int iResult;
    char *data = (char *)libData->numericarrayLibraryFunctions->MNumericArray_getData(mArr); 
    long dataLength = MArgument_getInteger(Args[2]); 

    uState_t* st = uGetState(clientId);

    if (st == 0) {
        printf("[socketBinaryWrite] writting to a closed or nonexisting socket!\r\n");
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

    //make a copy
    char* copy = (char*)malloc(dataLength);
    memcpy(copy, data, dataLength);

    packet_t* packet = (packet_t*)malloc(sizeof(packet));
    packet->ref = &socket_write;
    packet->socket = clientId;
    packet->buf = copy;

    //pipePush(packet);
    
    MArgument_setInteger(Res, dataLength);
    return LIBRARY_NO_ERROR;
}
DLLEXPORT int socketWriteString(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    return LIBRARY_NO_ERROR;
}

DLLEXPORT int socketReadyQ(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    return LIBRARY_NO_ERROR;
}

DLLEXPORT int socketPort(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    return LIBRARY_NO_ERROR; 
}

DLLEXPORT int socketReadMessage(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    return LIBRARY_NO_ERROR; 
}