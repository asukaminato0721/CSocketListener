#!/usr/bin/env wolframscript
(* ::Package:: *)
Get["https://raw.githubusercontent.com/JerryI/wl-localpackages/main/Kernel/LPM.wl"];

PacletRepositories[{
  Github -> "https://github.com/KirillBelovTest/Objects",
  Github -> "https://github.com/KirillBelovTest/Internal",
  Github -> "https://github.com/KirillBelovTest/TCPServer",
  Github -> "https://github.com/KirillBelovTest/HTTPHandler",
  Github -> "https://github.com/KirillBelovTest/WebSocketHandler",
  Github -> "https://github.com/JerryI/wl-wlx" -> "dev",
  Github -> "https://github.com/JerryI/wl-misc" -> "dev"
}]

PacletDirectoryLoad[DirectoryName[$InputFileName] // ParentDirectory]

RandomWord[];

(* Here you can configure *)
ENV = <|
    "WSPort"->8011,
    "HTTPPort"->8010,
    "Host"->"127.0.0.1"
|>

ENV["HAddr"] := StringTemplate["``:``"][ENV["Host"], ENV["HTTPPort"]]
ENV["WAddr"] := StringTemplate["``:``"][ENV["Host"], ENV["WSPort"]]

(* TCP Server *)
<<KirillBelov`Objects`
<<KirillBelov`Internal`
<<KirillBelov`CSockets`
<<KirillBelov`TCPServer`

(* HTTP services *)
<<KirillBelov`HTTPHandler`
<<KirillBelov`HTTPHandler`Extensions`

(* WS services *)
<<KirillBelov`WebSocketHandler`

<<JerryI`WLX`
<<JerryI`WLX`Importer`
<<JerryI`WLX`WLJS`
<<JerryI`WLX`WebUI`

(* Event handling and WLJS communication *)
<<JerryI`Misc`Events`
<<JerryI`Misc`WLJS`Transport`

<<KirillBelov`CSockets`EventsExtension`;

(* WLX scripts *)



SetDirectory[FileNameJoin[{If[StringQ[NotebookDirectory[]], NotebookDirectory[], DirectoryName[$InputFileName]], "WLJSBasicDyn"}]]


Print["Staring HTTP server..."];

tcp = TCPServer[];
tcp["CompleteHandler", "HTTP"] = HTTPPacketQ -> HTTPPacketLength;
tcp["MessageHandler", "HTTP"] = HTTPPacketQ -> http;

(* our main file for all requests *)
index := ImportComponent["index.wlx"];

http = HTTPHandler[];

http["MessageHandler", "Index"] = AssocMatchQ[<|"Method" -> "GET"|>] -> Function[x, index[x]]

SocketListen[CSocketOpen[ENV["HAddr"]], tcp@#&]

Print["Staring WS/HTTP server..."];

wcp = TCPServer[]
wcp["CompleteHandler", "WebSocket"] = WebSocketPacketQ -> WebSocketPacketLength
wcp["MessageHandler", "WebSocket"]  = WebSocketPacketQ -> ws

ws = WebSocketHandler[]

(* configure the handler for WLJS communications *)
ws["MessageHandler", "Evaluate"]  = Function[True] -> WLJSTransportHandler

(* symbols tracking *)
WLJSTransportHandler["AddTracking"] = Function[{symbol, name, cli, callback},
    Print["Add tracking... for "<>name];
    Experimental`ValueFunction[Unevaluated[symbol]] = Function[{y,x}, callback[cli, x]];
, HoldFirst]

WLJSTransportHandler["GetSymbol"] = Function[{expr, client, callback},
    Print["evaluating the desired symbol on the Kernel"];
    callback[expr // ReleaseHold];
]

SocketListen[CSocketOpen[ENV["WAddr"]], wcp@#&, "SocketEventsHandler"->CSocketsClosingHandler]
(* reseved keyword for WLJS interpreter *)
SetAttributes[Offload, HoldFirst];

StringTemplate["open http://``"][ENV["HAddr"]] // Print;
While[True, Pause[1]];

