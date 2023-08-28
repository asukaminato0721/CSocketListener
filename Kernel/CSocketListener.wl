(* ::Package:: *)

(* ::Chapter:: *)
(*CSocketListener*)


(* ::Section:: *)
(*Begin package*)


BeginPackage["KirillBelov`CSocketListener`"]; 


(* ::Section:: *)
(*Names*)


CSocketListen::usage = 
"CSocketListen[port|addr, func] creates listener."; 


CSocketListener::usage = 
"CSocketListener[assoc] listener object."; 


CSocket::usage = 
"CSocket[socketId] socket representation."; 

CSocketObject::usage = 
"CSocketObject[socketId] socket representation."; 

(* ::Section:: *)
(*Private context*)


Begin["`Private`"]; 


(* ::Section:: *)
(*Implementation*)


CSocketObject /: BinaryWrite[CSocketObject[socketId_Integer], bytes_ByteArray] := 
If[socketWrite[socketId, bytes, Length[bytes]] === -1, Print["lib writting failed!"]; $Failed, Null]; 


CSocketObject /: WriteString[CSocketObject[socketId_Integer], string_String] := 
If[socketWriteString[socketId, string, StringLength[string]] === -1, Print["lib writting failed!"]; $Failed, Null]; 


CSocketObject /: Close[CSocketObject[socketId_Integer]] := 
closeSocket[socketId]; 


CSocketListen[port_Integer, handler_] := With[{sid = createServer["127.0.0.1", port//ToString]},
Echo["Created server with sid: "<>ToString[sid]];
router[sid] = handler;
CEventLoopRun;
CSocketListener[<|
	"Port" -> port, 
	"Host" -> "127.0.0.1",
	"Handler" -> handler, 
	"Task" -> Null
|>]]; 


CSocketListen[addr_String, handler_] := With[{port = StringSplit[addr,":"]//Last, host = StringSplit[addr,":"]//First},
sid = createServer[host, port];
Echo["Created server with sid: "<>ToString[sid]];
router[sid] = handler;
CEventLoopRun;
CSocketListener[<|
	"Port" -> ToExpression[port], 
	"Host" -> host,
	"Handler" -> handler, 
	"Task" -> Null
|>]]; 

router[task_, event_, {serverId_, clientId_, data_}] := (
	router[serverId][toPacket[task, event, {serverId, clientId, data}]]
)

CEventLoopRun := (Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&]; CEventLoopRun = Null)

CSocketListener /: DeleteObject[CSocketListener[assoc_Association]] := 
stopServer[assoc["Task"][[2]]]; 


(* ::Section:: *)
(*Internal*)


$directory = DirectoryName[$InputFileName, 2]; 


$libFile = FileNameJoin[{
	$directory, 
	"LibraryResources", 
	$SystemID, 
	"socket_listener." <> Internal`DynamicLibraryExtension[]
}]; 


If[!FileExistsQ[$libFile], 
	Get[FileNameJoin[{$directory, "Scripts", "BuildLibrary.wls"}]]
]; 


createServer = LibraryFunctionLoad[$libFile, "create_server", {String, String}, Integer]; 

runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]; 

stopServer::usage = "stopServer[asyncObjId]"; 
stopServer = LibraryFunctionLoad[$libFile, "stop_server", {Integer}, Integer]; 


socketWrite = LibraryFunctionLoad[$libFile, "socket_write", {Integer, "ByteArray", Integer}, Integer]; 


socketWriteString = LibraryFunctionLoad[$libFile, "socket_write_string", {Integer, String, Integer}, Integer]; 


closeSocket = LibraryFunctionLoad[$libFile, "close_socket", {Integer}, Integer]; 


toPacket[task_, event_, {serverId_, clientId_, data_}] := 
<|
	"Socket" -> CSocket[serverId], 
	"SourceSocket" -> CSocket[clientId], 
	"DataByteArray" -> ByteArray[data]
|>; 


(* ::Section:: *)
(*End private context*)


End[]; 


(* ::Section:: *)
(*End package*)


EndPackage[]; 