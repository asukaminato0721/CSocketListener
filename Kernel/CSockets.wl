(* ::Package:: *)

(* ::Chapter:: *)
(*CSocketListener*)

(* bug-fix OpenMP on Linux machines *)
If[$OperatingSystem == "Linux", 
	Echo["CSockets >> OpenMP patch will be applied!"];
	Run["export KMP_INIT_AT_FORK=FALSE"];
];

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




CSocketOpen[host_String: "127.0.0.1", port_Integer] := (
CSocketObject[host, port // ToString]); 

CSocketOpen[hostport_String: "127.0.0.1"] := (
With[{port = StringSplit[hostport,":"]//Last, host = StringSplit[hostport,":"]//First},
	CSocketObject[host, port]
]
); 


CSocketConnect[host_String: "127.0.0.1", port_Integer] := 
CSocketObject[socketConnect[host, ToString[port]]]; 


CSocketConnect[address_String] /; 
StringMatchQ[address, __ ~~ ":" ~~ NumberString] := 
CSocketObject[Apply[socketConnect, StringSplit[address, ":"]]]; 


CSocketObject /: BinaryWrite[CSocketObject[socketId_Integer], bytes_ByteArray] := 
If[socketWrite[socketId, bytes, Length[bytes]] === -1, Print["lib writting failed!"]; $Failed, Null]; 


CSocketObject /: WriteString[CSocketObject[socketId_Integer], string_String] := 
If[socketWriteString[socketId, string, StringLength[string]] === -1, Print["lib writting failed!"]; $Failed, Null]; 


CSocketObject /: Close[CSocketObject[socketId_Integer]] := 
closeSocket[socketId]; 


router[task_, event_, {serverId_, clientId_, data_}] := (
	router[serverId][toPacket[task, event, {serverId, clientId, data}]]
)

task := (task = True; Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&];); 

CSocketObject /: SocketListen[CSocketObject[host_String, port_String], handler_] := 
With[{sid = createServer[host, port]}, 
	
	
	task;
	Echo["Created server with sid: "<>ToString[sid]];
	router[sid] = handler;

	CSocketListener[<|
		"Port" -> ToExpression[port], 
		"Host" -> host,
		"Handler" -> handler, 
		"Task" -> Null
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


runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]; 


createServer = LibraryFunctionLoad[$libFile, "create_server", {String, String}, Integer]; 

(*
socketListenerTaskRemove = LibraryFunctionLoad[$libFile, "socketListenerTaskRemove", {Integer}, Integer]; 


socketConnect = LibraryFunctionLoad[$libFile, "socketConnect", {String, String}, Integer]; *)

socketWrite = LibraryFunctionLoad[$libFile, "socket_write", {Integer, "ByteArray", Integer}, Integer]; 


socketWriteString = LibraryFunctionLoad[$libFile, "socket_write_string", {Integer, String, Integer}, Integer]; 


closeSocket = LibraryFunctionLoad[$libFile, "close_socket", {Integer}, Integer]; 


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
