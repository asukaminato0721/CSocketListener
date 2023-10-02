PacletInstall["JerryI/LPM"]
<< JerryI`LPM`

PacletRepositories[{
  Github -> "https://github.com/KirillBelovTest/Objects",
  Github -> "https://github.com/KirillBelovTest/Internal",
  Github -> "https://github.com/KirillBelovTest/TCPServer",
  Github -> "https://github.com/KirillBelovTest/HTTPHandler",
  Github -> "https://github.com/KirillBelovTest/WebSocketHandler",
  Github -> "https://github.com/JerryI/wl-wsp",
  Github -> "https://github.com/JerryI/wl-misc"
}]


SetDirectory[$InputFileName // DirectoryName]
Get[FileNameJoin[{ParentDirectory[Directory[]], "Kernel", "CSockets.wl"}]]

<<KirillBelov`Objects`
<<KirillBelov`Internal`
<<KirillBelov`TCPServer`

<<KirillBelov`HTTPHandler`
<<KirillBelov`HTTPHandler`Extensions`
<<KirillBelov`WebSocketHandler`

<<JerryI`WSP`
<<JerryI`WSP`WLJS`
<<JerryI`Misc`Events`
<<JerryI`Misc`WLJS`Transport`

$PublicDirectory = FileNameJoin[{Directory[], "Metaballs"}]


Print["Staring HTTP server..."];

tcp = TCPServer[];
tcp["CompleteHandler", "HTTP"] = HTTPPacketQ -> HTTPPacketLength;
tcp["MessageHandler", "HTTP"] = HTTPPacketQ -> http;

http = HTTPHandler[];

http["MessageHandler", "Index"] = AssocMatchQ[<|"Path" -> "/"|>] -> (HypertextProcess[#, "index.wsp", "Base" -> $PublicDirectory] &)
http["MessageHandler", "WSP"] = GetFileRequestQ[{"wsp"}] -> (HypertextProcess[#, "Base" -> $PublicDirectory] &)

(* ::End::*)
SocketListen[CSocketOpen[8010], tcp@# &]

Print["Staring WS/HTTP server..."];

wcp = TCPServer[]
wcp["CompleteHandler", "WebSocket"] = WebSocketPacketQ -> WebSocketPacketLength
wcp["MessageHandler", "WebSocket"]  = WebSocketPacketQ -> ws

ws = WebSocketHandler[]

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

SocketListen[CSocketOpen[8020], wcp@#&]

SetOptions[WSPEngine, "Cache" -> False];



bodies  = NBodySimulation[
  "InverseSquare", {
  <|"Mass" -> 1, "Position" -> {0, 2}, "Velocity" -> {0, .5}|>,
  <|"Mass" -> 1, "Position" -> {3, 0.4}, "Velocity" -> {+0.01, -.5}|>,
  <|"Mass" -> 1, "Position" -> {-1.1, 0.4}, "Velocity" -> {-2.11, -.5}|>}, 1];

width = 500;
height = 500;
t = 0;

getScaled[t_] := Module[{max,min, pos = bodies[All, "Position", t]},
  max = 1.5 Max[pos//Flatten] {1,1};
  min = 1.5 Min[pos//Flatten] {1,1};

  ( {width, height}  (# - min) / (max - min))& /@ pos
];

update = EventObject[<|"id"->"reCompute"|>];


$DefaultSerializer = ExportByteArray[#, "ExpressionJSON"]&;

EventHandler[update, Function[data,

  d = getScaled[t];

  
  t = t + 0.01;
  If[t > 1.0, t = 0;
  (* regenerate it! *)
  bodies  = NBodySimulation[
    "InverseSquare", {
    <|"Mass" -> 1, "Position" -> RandomReal[{-3,3},2], "Velocity" -> RandomReal[{-3,3},2]|>,
    <|"Mass" -> 1, "Position" -> RandomReal[{-3,3},2], "Velocity" -> RandomReal[{-3,3},2]|>,
    <|"Mass" -> 1, "Position" -> RandomReal[{-3,3},2], "Velocity" -> RandomReal[{-3,3},2]|>}, 1]  
  ];


  WebSocketSend[Global`client, Global`UpdateCanvas[d] // $DefaultSerializer];
]];

(* kickstarter rontSubmit[UpdateCanvas[getScaled[t]]]*)


StringTemplate["open http://``:``/"]["127.0.0.1", "8010"] // Print;
While[True, Pause[1]];

