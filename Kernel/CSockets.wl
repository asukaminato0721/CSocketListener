(* ::Package:: *)

(* ::Chapter:: *)
(*CSocketListener*)


(* ::Section:: *)
(*Begin package*)


BeginPackage["KirillBelov`CSockets`"]; 


(* ::Section:: *)
(*Names*)


CSocketObject::usage = 
"CSocketObject[socketId] socket representation."; 


CSocketOpen::usage = 
"CSocketOpen[port] returns new server socket."; 


CSocketConnect::usage = 
"CSocketConnect[host, port] connect to socket."; 


CSocketListener::usage = 
"CSocketListener[assoc] listener object."; 


(* ::Section:: *)
(*Private context*)


Begin["`Private`"]; 


(* ::Section:: *)
(*Implementation*)




CSocketOpen[host_String: "127.0.0.1", port_Integer] := (task;
CSocketObject[host, port // ToString]); 

CSocketOpen[hostport_String: "127.0.0.1"] := (task;
With[{port = StringSplit[addr,":"]//Last, host = StringSplit[addr,":"]//First},
	CSocketObject[host, port]
]
); 


CSocketConnect[host_String: "127.0.0.1", port_Integer] := 
CSocketObject[socketConnect[host, ToString[port]]]; 


CSocketConnect[address_String] /; 
StringMatchQ[address, __ ~~ ":" ~~ NumberString] := 
CSocketObject[Apply[socketConnect, StringSplit[address, ":"]]]; 


CSocketObject /: BinaryWrite[CSocketObject[socketId_Integer], data_ByteArray] := 
If[socketBinaryWrite[socketId, data, Length[data], $bufferSize] === -1, $Failed, 0]; 


CSocketObject /: BinaryWrite[CSocketObject[socketId_Integer], data_List] := 
If[socketBinaryWrite[socketId, ByteArray[data], Length[data], $bufferSize] === -1, $Failed, 0]; 


CSocketObject /: WriteString[CSocketObject[socketId_Integer], data_String] := 
If[socketWriteString[socketId, data, StringLength[data], $bufferSize] === -1, $Failed, 0]; 


CSocketObject /: SocketReadMessage[CSocketObject[socketId_Integer], bufferSize_Integer: $bufferSize] := 
socketReadMessage[socketId, bufferSize]; 


CSocketObject /: SocketReadyQ[CSocketObject[socketId_Integer]] := 
socketReadyQ[socketId]; 


CSocketObject /: Close[CSocketObject[socketId_Integer]] := 
socketClose[socketId]; 

router[task_, event_, {serverId_, clientId_, data_}] := (
	Print["recv"];
	router[serverId][toPacket[task, event, {serverId, clientId, data}]]
)

task := (task = True; Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&];); 

CSocketObject /: SocketListen[CSocketObject[host_String, port_String], handler_] := 
Module[{sid}, 
	sid = createServer[host, port];
	Echo["Created server with sid: "<>ToString[sid]];
	router[sid] = handler;

	CSocketListener[<|
		"Socket" -> socket, 
		"Handler" -> handler, 
		"TaskId" -> id, 
		"Task" -> task
	|>]
]; 


(* ::Section:: *)
(*Internal*)


$directory = DirectoryName[$InputFileName, 2]; 


$libFile = FileNameJoin[{
	$directory, 
	"LibraryResources", 
	$SystemID, 
	"csockets." <> Internal`DynamicLibraryExtension[]
}]; 


$bufferSize = 8192; 


If[!FileExistsQ[$libFile], 
	Get[FileNameJoin[{$directory, "Scripts", "BuildLibrary.wls"}]]
]; 


toPacket[task_, event_, {serverId_, clientId_, data_}] := 
<|
	"Socket" -> CSocketObject[serverId], 
	"SourceSocket" -> CSocketObject[clientId], 
	"DataByteArray" -> ByteArray[data]
|>; 

(*
socketOpen = LibraryFunctionLoad[$libFile, "socket_open", {String, String}, Integer]; 
*)

socketClose = LibraryFunctionLoad[$libFile, "close_socket", {Integer}, Integer]; 

runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]; 


createServer = LibraryFunctionLoad[$libFile, "create_server", {String, String}, Integer]; 

(*
socketListenerTaskRemove = LibraryFunctionLoad[$libFile, "socketListenerTaskRemove", {Integer}, Integer]; 


socketConnect = LibraryFunctionLoad[$libFile, "socketConnect", {String, String}, Integer]; *)


socketBinaryWrite = LibraryFunctionLoad[$libFile, "socket_write", {Integer, "ByteArray", Integer, Integer}, Integer]; 


socketWriteString = LibraryFunctionLoad[$libFile, "socket_write_string", {Integer, String, Integer, Integer}, Integer]; 


(*socketReadyQ = LibraryFunctionLoad[$libFile, "socketReadyQ", {Integer}, True | False]; 


socketReadMessage = LibraryFunctionLoad[$libFile, "socketReadMessage", {Integer, Integer}, "ByteArray"]; 


socketPort = LibraryFunctionLoad[$libFile, "socketPort", {Integer}, Integer]; 
*)

(* ::Section:: *)
(*End private context*)


End[]; 


(* ::Section:: *)
(*End package*)


EndPackage[]; 
