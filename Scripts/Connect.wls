#!/usr/bin/env wolframscript
(* ::Package:: *)

PacletDirectoryLoad[Directory[]]


Get["KirillBelov`CSockets`"]; 

client = CSocketConnect["ya.ru", 80];
Pause[3];
BinaryWrite[client, ByteArray[{1,2,3}] ];
While[True,
Pause[1]
];


