#undef UNICODE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <stdbool.h>

#include "WolframLibrary.h"
#include "WolframIOLibraryFunctions.h"
#include "WolframNumericArrayLibrary.h"

uv_loop_t *loop;

int uv_loop_running = -1;

struct srv
{
    uv_stream_t* stream;
    int id;
    struct sockaddr_in addr;
    mint asyncObjID;
    int state;
};

typedef struct srv server;
server* servers;
int nservers = 0;

#define MAXCLIENTS 2048

struct cli
{
    uv_stream_t* stream;
    uv_stream_t* parent;
    int id;
    int state;
};

typedef struct cli client;
client* clients;
int nclients = 0;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    //Here it fucks up
    free(wr->buf.base);
    free(wr);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

#define HASH_FREE -199
#define HASH_NEXT 33
#define HASH_OCCUPIED 71

typedef struct {
    unsigned long stream;
    long id;

    int _flag;
} uState_t;

#define hashmap_size 2048
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

unsigned long HashAllocate(unsigned long socketId, int offset);

void HashCopy(unsigned long socketId, int offsetSrc, int offsetDest) {
    unsigned long hS = hash((unsigned long)socketId, offsetSrc);
    printf("hash >> allocate for a copy\n");
    unsigned long hD = HashAllocate((unsigned long)socketId, offsetDest);

    printf("hash >> copied\n");

    memcpy(&uState[hD], &uState[hS], sizeof(uState_t));
}

//helper functions to check the status of the socket
unsigned long HashAllocate(unsigned long socketId, int offset) {
    printf("hash >> allocate %ld with offset %ld\n", (unsigned long)socketId, offset);
    unsigned long h = hash((unsigned long)socketId, offset);
    printf("hash >> %ld\n", h);

    if (uState[h]._flag == HASH_OCCUPIED) {
        printf("hash >> collizion!\n");

        //copy the original value
        printf("hash >> copy old one %ld\n", (unsigned long)uState[h].stream);
        HashCopy(uState[h].stream, offset, offset + 1);

        uState[h]._flag = HASH_NEXT;
        return HashAllocate(socketId, offset + 1);
    }

    if (uState[h]._flag == HASH_NEXT) {
        printf("hash >> next\n");
        return HashAllocate(socketId, offset + 1);
    }

    printf("hash >> ok!\n");
    uState[h]._flag = HASH_OCCUPIED;
    uState[h].stream = (unsigned long)socketId;

    return h;
}

void HashInit() {
    for (int i=0; i<hashmap_size; ++i) {
        uState[i]._flag = HASH_FREE;
        uState[i].id = -1;
    }
}

void HashFree(unsigned long socketId, int offset) {
    //printf("hash >> freeing %ld\n", (unsigned long)socketId);
    unsigned long h = hash((unsigned long)socketId, offset);
    if (uState[h]._flag == HASH_NEXT) {
        return HashFree(socketId, offset + 1);
    }

    if (uState[h]._flag == HASH_OCCUPIED) {
        uState[h]._flag = HASH_FREE;
        return;
    }

    //printf("hash >> already freed!\n");
}

unsigned long HashGet(unsigned long socketId, int offset) {
    //printf("[HashGet] get\r\n\r\n");
    unsigned long h = hash((unsigned long)socketId, offset);
    if (uState[h]._flag == HASH_NEXT) {
        //printf("[HashGet] next\r\n\r\n");
        return HashGet(socketId, offset + 1);
    }
    //printf("[HashGet] done\r\n\r\n");

    return h;
}


void uStateSet(unsigned long socketId, int state) {
    unsigned long h = HashGet(socketId, 0);
    if ((unsigned long)(uState[h].stream) != (unsigned long)socketId || uState[h]._flag == HASH_FREE) {
        printf("[uGetState] probably it is gone already\r\n\r\n");
        return;
    }
    uState[h].id = state;
}

int fetchClientId(uv_stream_t *client) {
    unsigned long h = HashGet((unsigned long)client, 0);
    if ((unsigned long)(uState[h].stream) != (unsigned long)client) {
        return -1;
    }
    return uState[h].id;
}

int fetchStreamId(uv_stream_t *s) {
    unsigned long h = HashGet((unsigned long)s, 0);
    if ((unsigned long)(uState[h].stream) != (unsigned long)s) {
        return -1;
    }
    return uState[h].id;
}

WolframIOLibrary_Functions ioLibrary;
WolframNumericArrayLibrary_Functions numericLibrary;
mint asyncObjID;


uv_mutex_t mutex;

typedef struct SocketTaskArgs_st {
    WolframNumericArrayLibrary_Functions numericLibrary;
    WolframIOLibrary_Functions ioLibrary;
    mint garbage; 
}* SocketTaskArgs; 

DLLEXPORT mint WolframLibrary_getVersion( ) {
    return WolframLibraryVersion;
}

DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
    servers = (server*)malloc(sizeof(server)*10);
    for (int i=0; i<10; ++i) servers[i].state = -1; //all closed

    nservers = 0;

    uv_mutex_init(&mutex);

    clients = (client*)malloc(sizeof(client)*MAXCLIENTS);
    for (int i=0; i<MAXCLIENTS; ++i) clients[i].state = -1; //all closed

    nclients = 0;

    loop = uv_default_loop();
    

    ioLibrary = libData->ioLibraryFunctions;
    numericLibrary = libData->numericarrayLibraryFunctions;

    HashInit();

    return 0;
}

DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libData) {
    uv_stop(loop);

    return;
}

void pipeBufData (uv_buf_t buf, uv_stream_t *client) {
    int clientId = fetchClientId(client);
    if (clientId < 0) {
        printf("socket is broken!\r\n");
        return;
    }
    int streamId = fetchStreamId(clients[clientId].parent);

    mint dims[1]; 
    MNumericArray data;

	DataStore ds;

    //printf("CURRENT ID OF CLIENT: %d\n", clientId);

    //printf("RECEIVED %d BYTES\n", buf.len);
    
    dims[0] = buf.len; 
    numericLibrary->MNumericArray_new(MNumericArray_Type_UBit8, 1, dims, &data); 
    memcpy(numericLibrary->MNumericArray_getData(data), buf.base, buf.len);
                
    ds = ioLibrary->createDataStore();
    ioLibrary->DataStore_addInteger(ds, streamId);
    ioLibrary->DataStore_addInteger(ds, clientId);
    ioLibrary->DataStore_addMNumericArray(ds, data);

    //printf("raise async event %d for server %d and client %d\n", asyncObjID, streamId, clientId);
    ioLibrary->raiseAsyncEvent(asyncObjID, "RECEIVED_BYTES", ds);
}



void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    //printf("echo read\n");
    if (nread > 0) {
        uv_buf_t b = uv_buf_init(buf->base, nread);
        pipeBufData(b, client);
        free(b.base);   
        return;
    }

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));

        //uv_close((uv_handle_t*) client, NULL);
        int uid = fetchClientId(client);
        if (uid < 0) {
            printf("socket is broken!\r\n");
            free(buf->base);
            return;
        }
        printf("writeerror !\n");
        printf("making %d closed by the reading thread!\n", uid);
        if (uv_is_closing((uv_handle_t*) clients[uid].stream) == 0)
            uv_close((uv_handle_t*) clients[uid].stream, NULL);
        clients[uid].state = -1;   
        
        uStateSet((unsigned long)clients[uid].stream, -1);
        HashFree((unsigned long)clients[uid].stream, 0);

        //printf("we closed socket: %d ;)))\n", fetchClientId(client));
        //clients[fetchClientId(client)].state = 2;
        //mb one can notify mathematica about it
    }

    free(buf->base);
}

void findEmptyClientsSlot() {
    if (clients[nclients].state == -1) return;
    nclients++;
    if (nclients == MAXCLIENTS) nclients = 0;

    while(true) {
        if (clients[nclients].state == -1) return;

        nclients++;
        if (nclients == MAXCLIENTS) nclients = 0;
    }
    
}

void findEmptyServersSlot() {
    if (servers[nservers].state == -1) return;
    nservers++;
    if (nservers == 10) nservers = 0;

    while(true) {
        if (servers[nservers].state == -1) return;

        nservers++;
        if (nservers == 10) nservers = 0;
    }
    
}

void on_new_connection(uv_stream_t *server, int status) {
    
    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        // error!
        return;
    }

    findEmptyClientsSlot();

    printf("New connection for %d\n", nclients);

    uv_tcp_t *c = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));

    
    //hash_table_occupy((uv_stream_t*)c, nclients);
    HashAllocate((uv_stream_t*)c, 0);
    uStateSet((uv_stream_t*)c, nclients);

    clients[nclients].stream = (uv_stream_t*)c;
    clients[nclients].parent = (uv_stream_t*)server;
    clients[nclients].id = nclients;
    clients[nclients].state = 0;

    uv_tcp_init(loop, c);

    if (uv_accept(server, (uv_stream_t*) c) == 0) {
        //printf("uv start reading");
        uv_read_start((uv_stream_t*) c, alloc_buffer, echo_read);
    } else {
        printf("not accepted for %d", nclients);
        clients[nclients].state = -1;
        if (uv_is_closing((uv_handle_t*) c) == 0)
            uv_close((uv_handle_t*) c, NULL);
        //hash_table_deoccupy((unsigned long)c);  
        uStateSet((uv_stream_t*)c, -1);
        HashFree((uv_stream_t*)c, 0);
    }
}

uv_async_t cbwrite;
uv_async_t cbclose;


void async_cb_write(uv_async_t* async, int status);
void async_cb_close(uv_async_t* async, int status);

static void uvTask(mint asyncObjID, void* vtarg)
{
    fprintf(stderr, "\nHee uvTask: %d\n", asyncObjID);
    printf("Event-Loop started! \n");
    uv_async_init(loop, &cbwrite, async_cb_write);
    uv_async_init(loop, &cbclose, async_cb_close);
    uv_run(loop, UV_RUN_DEFAULT);
}


DLLEXPORT int run_uvloop(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) {
    printf("creating async task...\n");
    SocketTaskArgs threadArg = (SocketTaskArgs)malloc(sizeof(struct SocketTaskArgs_st));
    threadArg->ioLibrary = libData->ioLibraryFunctions; 
    threadArg->numericLibrary = libData->numericarrayLibraryFunctions;
    ioLibrary = libData->ioLibraryFunctions;
    numericLibrary = libData->numericarrayLibraryFunctions;
    
        
    asyncObjID = ioLibrary->createAsynchronousTaskWithThread(uvTask, threadArg);

    MArgument_setInteger(Res, asyncObjID); 
    return LIBRARY_NO_ERROR;     
}

DLLEXPORT int create_server(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res) 
{
    char* listenAddrName = MArgument_getUTF8String(Args[0]); 
    char* listenPortName = MArgument_getUTF8String(Args[1]); 
  
    //loop = uv_default_loop();

    uv_tcp_t* s = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    findEmptyServersSlot();

    //hash_table_occupy((uv_stream_t*)s, nservers);
    HashAllocate((uv_stream_t*)s, 0);
    uStateSet((uv_stream_t*)s, nservers);

    servers[nservers].stream = (uv_stream_t*)s;
    servers[nservers].id = nservers;
    servers[nservers].state = 0;


    uv_tcp_init(loop, s);

    uv_ip4_addr(listenAddrName, atoi(listenPortName), &(servers[nservers].addr));
    uv_tcp_bind(s, (const struct sockaddr*)&(servers[nservers].addr), 0);
    int r = uv_listen((uv_stream_t*) s, 128, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }

    printf("LISTEN unsigned long at %s:%d\n", listenAddrName, atoi(listenPortName)); 

    //MArgument_setInteger(Res, nservers); 

    servers[nservers].asyncObjID = nservers;

    printf("server: %d\n", nservers); 

    MArgument_setInteger(Res, nservers); 

    return LIBRARY_NO_ERROR; 
}


typedef struct uv_write_q_st {
    uv_write_t* req; 
    uv_stream_t* stream; 
    uv_buf_t* buf;
} uv_write_q; 

volatile uv_write_q uv_write_que[128];
volatile int uv_write_que_ptr = -1;

void echo_write(uv_write_t *req, int status) {
    //printf("echo write\n");
    if (status) {
        int uid = fetchClientId(req->handle);
        if (uid < 0) {
            printf("client hash is broken\r\n");
            free_write_req(req);
            return;
        }
        printf("writeerror !\n");
        printf("making %d closed manually!\n", uid);
        if (uv_is_closing((uv_handle_t*) clients[uid].stream) == 0)
            uv_close((uv_handle_t*) clients[uid].stream, NULL);
        clients[uid].state = -1;
        uStateSet((unsigned long)clients[uid].stream, -1);
        HashFree((unsigned long)clients[uid].stream, 0);     
    }

    //printf("free write req !\n");
    free_write_req(req);

    /*uv_write_que_ptr--;
    printf("counter set %d\n", uv_write_que_ptr);

    if (uv_write_que_ptr > -1) {
        printf("checking next... in the que at %d\n", uv_write_que_ptr);
        uv_write_q *p = &uv_write_que[uv_write_que_ptr];

        uv_write(p->req, p->stream, p->buf, 1, echo_write);
        printf("written from the que!\n");
    } else {
        printf("no pending write queries, i.e. %d. Done!\n", uv_write_que_ptr);
    }*/
}

int write_fifo_ptr = -1;
int close_fifo_ptr = -1;

typedef struct {
    uv_write_t* req;
    uv_stream_t* stream;
    uv_buf_t* buf;
    uv_handle_t* handle;
} write_fifo_t;

write_fifo_t write_fifo[1024];
write_fifo_t close_fifo[64];



void async_cb_write(uv_async_t* async, int status) {
  //printf("async_cb\n");
  uv_mutex_lock(&mutex);
  while (write_fifo_ptr >= 0) {
    uv_write(write_fifo[write_fifo_ptr].req, write_fifo[write_fifo_ptr].stream, write_fifo[write_fifo_ptr].buf, 1, echo_write);
    write_fifo_ptr--;
  }
  uv_mutex_unlock(&mutex);
  //uv_close((uv_handle_t*) async, NULL);
}

void async_cb_close(uv_async_t* async, int status) {
  //printf("async_cb_close\n");
    uv_mutex_lock(&mutex);
  while (close_fifo_ptr >= 0) {
    uv_close((uv_handle_t*)close_fifo[close_fifo_ptr].handle, NULL);
    close_fifo_ptr--;
  }
uv_mutex_unlock(&mutex);
  //uv_close((uv_handle_t*) async, NULL);
}



int uv_write_push(uv_write_t* req, uv_stream_t* stream, uv_buf_t* buf) {
    uv_async_t *message = (uv_async_t*)malloc(sizeof(uv_async_t));
    uv_mutex_lock(&mutex);
    ++write_fifo_ptr;
    write_fifo[write_fifo_ptr].req = req;
    write_fifo[write_fifo_ptr].stream = stream;
    write_fifo[write_fifo_ptr].buf = buf;
    uv_mutex_unlock(&mutex);

    uv_async_send(&cbwrite);
}

void uv_close_push(uv_handle_t* handle, void* m) {
    uv_async_t *message = (uv_async_t*)malloc(sizeof(uv_async_t));
    uv_mutex_lock(&mutex);
    ++close_fifo_ptr;
    close_fifo[close_fifo_ptr].handle = handle;
    uv_mutex_unlock(&mutex);
    uv_async_send(&cbclose);    
}



DLLEXPORT int socket_write(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){

    
    int iResult; 
    WolframNumericArrayLibrary_Functions numericLibrary = libData->numericarrayLibraryFunctions; 
    int clientId = MArgument_getInteger(Args[0]); 

    if (clients[clientId].state == -1) {
        printf("Client %d is closed already!\n", clientId);
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }    

    if (uv_is_writable(clients[clientId].stream) == 0) {
        printf("Client %d is not writtable anymore!\n", clientId);
        if (uv_is_closing((uv_handle_t*) clients[clientId].stream) == 0)
            uv_close_push((uv_handle_t*) clients[clientId].stream, NULL);

        uStateSet((unsigned long)clients[clientId].stream, -1);
        HashFree((unsigned long)clients[clientId].stream, 0);
 
        clients[clientId].state = -1;
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

          
    mint bytesLen = MArgument_getInteger(Args[2]); 
    char *bytes = (char*) malloc(sizeof(char)*bytesLen);
    //otherwise Mathematica will free the buffer before it will be sent
    memcpy(bytes, numericLibrary->MNumericArray_getData(MArgument_getMNumericArray(Args[1])), sizeof(char)*bytesLen);

    //printf("*** sending stuff.... to socket %d\n", clientId);
    write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    req->buf = uv_buf_init(bytes, bytesLen);

    int st = uv_write_push((uv_write_t*) req, clients[clientId].stream, &req->buf);
    //int st = uv_write((uv_write_t*) req, clients[clientId].stream, &req->buf, 1, echo_write);
    //ON ERROR send expection to mathematica
    //printf("*** done with %d ***\n", st);

    MArgument_setInteger(Res, st); 
    return LIBRARY_NO_ERROR; 
}



DLLEXPORT int socket_write_string(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    int iResult; 
    WolframNumericArrayLibrary_Functions numericLibrary = libData->numericarrayLibraryFunctions; 
    int clientId = MArgument_getInteger(Args[0]); 

    if (clients[clientId].state == -1) {
        printf("Client %d is closed already!\n", clientId);
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }    

    if (uv_is_writable(clients[clientId].stream) == 0) {
        printf("Client %d is not writtable anymore!\n", clientId);
        if (uv_is_closing((uv_handle_t*) clients[clientId].stream) == 0)
            uv_close_push((uv_handle_t*) clients[clientId].stream, NULL);
        
        uStateSet((unsigned long)clients[clientId].stream, -1);
        HashFree((unsigned long)clients[clientId].stream, 0);

        clients[clientId].state = -1;
        MArgument_setInteger(Res, -1);
        return LIBRARY_NO_ERROR;
    }

    mint bytesLen = MArgument_getInteger(Args[2]); 
    char *bytes = (char*) malloc(sizeof(char)*bytesLen);
    //otherwise Mathematica will free the buffer before it will be sent
    memcpy(bytes, MArgument_getUTF8String(Args[1]), sizeof(char)*bytesLen);

    //printf("*** sending stuff.... to socket %d\n", clientId);
    write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    req->buf = uv_buf_init(bytes, bytesLen);

    //int st = uv_write((uv_write_t*) req, clients[clientId].stream, &req->buf, 1, echo_write);
    int st = uv_write_push((uv_write_t*) req, clients[clientId].stream, &req->buf);
    //ON ERROR send expection to mathematica
    //printf("*** done with %d ***\n", st);

    MArgument_setInteger(Res, st); 
    return LIBRARY_NO_ERROR; 
}

DLLEXPORT int close_socket(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    int clientId = MArgument_getInteger(Args[0]); 

    printf("Client %d was closed by Wolfram!\n", clientId);
    if (uv_is_closing((uv_handle_t*) clients[clientId].stream) == 0)
        uv_close_push((uv_handle_t*) clients[clientId].stream, NULL);
    clients[clientId].state = -1;  

    uStateSet((unsigned long)clients[clientId].stream, -1);
    HashFree((unsigned long)clients[clientId].stream, 0);   
    
    MArgument_setInteger(Res, 0);
    return LIBRARY_NO_ERROR; 
}

DLLEXPORT int stop_server(WolframLibraryData libData, mint Argc, MArgument *Args, MArgument Res){
    exit(-1);
    //MArgument_setInteger(Res, libData->ioLibraryFunctions->removeAsynchronousTask(taskId)); 
    return LIBRARY_NO_ERROR; 
}

