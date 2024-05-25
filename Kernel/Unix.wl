BeginPackage["KirillBelov`CSockets`Interface`Unix`"]

createAsynchronousTask;
socketOpen;
socketClose;
socketBinaryWrite;
socketWriteString;

socketConnect;

socketReadyQ;
socketReadMessage;
socketPort;


Begin["`Private`"]

(* ::Section:: *)
(*Internal*)


$linkVersion  := $linkVersion = If[(StringCases[ToString[#], "LibraryVersionInformation"] // Flatten // Length) > 0, "JustTryToFind", #] &@
LibraryVersionInformation[FindLibrary["demo"]]["WolframLibraryVersion"]; 
$directory = DirectoryName[$InputFileName, 2];

$libFile := FileNameJoin[{
	$directory, 
	"LibraryResources", 
    $linkVersion // ToString,
	$SystemID, 
	"csockets." <> Internal`DynamicLibraryExtension[]
}]; 

$buildLibrary := Get[FileNameJoin[{$directory, "Scripts", "BuildLibrary.wls"}] ]; 

Echo["CSockets >> Unix >> " <> $SystemID];
Echo["CSockets >> Unix >> Loading library... LLink "<>ToString[$linkVersion] ];

If[$linkVersion === "JustTryToFind",
  $linkVersion = 7;
  If[!FileExistsQ[$libFile], 
    Echo["CSockets >> Unix >> Not found! LLink "<>ToString[$linkVersion] ];
    $buildLibrary;

            If[FailureQ[
	            runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	        ],
                Echo["CSockets >> Unix >> Loading process failed. LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
        ,
            If[FailureQ[
	            runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	        ],
                Echo["CSockets >> Unix >> It did not work! LLink "<>ToString[$linkVersion] ];
                $linkVersion = 6;
                Echo["CSockets >> Unix >> Trying LLink "<>ToString[$linkVersion] ];
                
                If[FailureQ[
	                runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	            ],
                    Echo["CSockets >> Unix >> Loading process failed. LLink "<>ToString[$linkVersion] ];
                    Exit[-1];
                ];                
            ];
  ];
,
  If[!FileExistsQ[$libFile], 
    Echo["CSockets >> Unix >> Not found! LLink "<>ToString[$linkVersion] ];
    $buildLibrary;

            If[FailureQ[
	            runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	        ],
                Echo["CSockets >> Unix >> Loading process failed. LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
        ,
            If[FailureQ[
	            runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	        ],
                Echo["CSockets >> Unix >> It did not work! LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
  ];
];


Echo["CSockets >> Unix >> Succesfully loaded! LLink "<>ToString[$linkVersion] ];


socketsInfo = <||>;

(* A hack, since UV Library does not allow to have multiple event loops *)
router;
task;

createAsynchronousTask[socketId_, handler_, OptionsPattern[] ] := With[{},
 
    With[{sid = createServer @ socketId},
        router[_,  event_String, {sid, cid_, payload_}] := (handler[sid, event, {sid, cid, payload}]);
        router[_, "NewClient", {sid, cid_, port_}] := ( socketsInfo[cid] = port);
    ];   

    (* multiple async tasks are not supported! just return server's id *)
    If[!TrueQ[task], internalTask = Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&]; task = True];
   

    internalTask
]

Options[createAsynchronousTask] = {"BufferSize"->2^11}

socketOpen[host_String, port_String] := With[{uid = openSocket[host, port]},
    socketsInfo[uid] = ToExpression[port];
    uid
]

openSocket = LibraryFunctionLoad[$libFile, "socket_open", {String, String}, Integer]; 
createServer = LibraryFunctionLoad[$libFile, "create_server", {Integer}, Integer]; 
socketClose = LibraryFunctionLoad[$libFile, "close_socket", {Integer}, Integer]; 
socketBinaryWrite = LibraryFunctionLoad[$libFile, "socket_write", {Integer, "ByteArray", Integer, Integer}, Integer]; 
socketWriteString = LibraryFunctionLoad[$libFile, "socket_write_string", {Integer, String, Integer, Integer}, Integer]; 

socketConnectInternal = LibraryFunctionLoad[$libFile, "socket_connect", {Integer}, Integer];

buffers;

socketConnect[host_String, port_String] := Module[{state = False}, With[{sid = socketConnectInternal[socketOpen[host, port] ]},
    Echo["sid >> "<>ToString[sid] ];
    
    buffers[sid] = {};

    router[_, "Received", {sid, _, payload_}] := With[{data = ByteArray[payload]},
        If[Length[buffers[sid] ] == 0,
            buffers[sid] = data;
        ,
            buffers[sid] = Join[buffers[sid], data];
        ];
    ];

    router[_, "Connected", {sid, __}] := state = True;


    If[!TrueQ[task], internalTask = Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&]; task = True];

    TimeConstrained[
        While[!state,
            Pause[0.3];
        ];

        ClearAll[state];

        

        sid

    , 10, $Failed]
] ]

socketReadyQ[uid_] := Length[buffers[uid] ] > 0

socketReadMessage[uid_, size_] := With[{},
    While[Length[buffers[uid] ] < size,
        Pause[0.1];
    ];

    With[{d = Take[buffers[uid], size]},
        If[Length[buffers[uid] ] == size, buffer[uid] = {}, buffers[uid] = Drop[buffer[uid], size] ];
        d
    ]
]

socketPort[id_] := If[KeyExistsQ[socketsInfo, id], 
    socketsInfo[id], 
    -1
]

End[]
EndPackage[]
