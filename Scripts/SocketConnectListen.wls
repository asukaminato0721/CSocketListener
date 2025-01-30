#!/usr/bin/env wolframscript
(* ::Package:: *)

PacletDirectoryLoad[Directory[]]


Get["KirillBelov`CSockets`"]; 

handler = Function[assoc, 
    Echo["Recevied some data..."];
    Echo[">> port "];
    Echo[assoc["SourceSocket"]["DestinationPort"] ];
    Echo["Reply"];
    Pause[2];
    BinaryWrite[assoc["SourceSocket"], "Pong" // StringToByteArray ];
    Echo["Done"];
];

listener = SocketListen[SocketOpen[8000], handler[#]&]
client = CSocketConnect[8000]

Pause[1]

BinaryWrite[client, "Ping" // StringToByteArray ];

SocketReadMessage[client, 4] // ByteArrayToString // Echo;

Echo[client["DestinationPort"] ]




