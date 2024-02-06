#!/usr/bin/env wolframscript
(* ::Package:: *)

PacletDirectoryLoad[Directory[]]


Get["KirillBelov`CSockets`"]; 

handler = Function[assoc, 
    Echo["Recevied some data..."];
    Echo[assoc];
    Echo["Reply"];
    Pause[2];
    BinaryWrite[assoc["SourceSocket"], ByteArray[{7}] ];
    Echo["Done"];
];

listener = SocketListen[SocketOpen[8000], handler[#]&]
client = CSocketConnect[8000]


Pause[1]
Pause[1]
Pause[1]

BinaryWrite[client, ByteArray[{7}] ];
While[True, Pause[0.1]; ];



