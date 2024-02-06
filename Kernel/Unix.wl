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

$linkVersion = 7;
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

(* Trials and errors with different LLink versions and compilation processes *)
(* Fuck youself WRI! *)

If[!FileExistsQ[$libFile], 
    Echo["CSockets >> Unix >> Not found! LLink "<>ToString[$linkVersion] ];
    $linkVersion--;

    If[!FileExistsQ[$libFile], 
        Echo["CSockets >> Unix >> Not found! LLink "<>ToString[$linkVersion] ];
        $linkVersion++;
        $buildLibrary;

        If[!FileExistsQ[$libFile],
            Echo["CSockets >> Unix >> File does not exists. Sorry, we can't run this thing. LLink "<>ToString[$linkVersion] ];
            Exit[-1];
        ];
    ,
    
        If[FailureQ[
		    runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	    ],
            Echo["CSockets >> Unix >> Loading process failed. Sorry, we can't do much about it :("];
            Exit[-1];
        ];
    
    ];

,
    If[FailureQ[
	    runLoop = LibraryFunctionLoad[$libFile, "run_uvloop", {Integer}, Integer]
	],
        Echo["CSockets >> Unix >> Wrong LLink version! Trying another... LLink "<>ToString[$linkVersion]<>" Trying another..."];
        
        $linkVersion--;
        If[!FileExistsQ[$libFile], 
            Echo["CSockets >> Unix >> Not found! LLink "<>ToString[$linkVersion] ];
            $linkVersion++;
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
        ]
    ];
    
];

Echo["CSockets >> Unix >> Succesfully loaded! LLink "<>ToString[$linkVersion] ];


(* A hack, since UV Library does not allow to have multiple event loops *)
router;
task;

createAsynchronousTask[socketId_, handler_, OptionsPattern[] ] := With[{},
 
    With[{sid = createServer @@ sockets[socketId]},
        router[_, event_, {sid, payload__}] := (handler[sid, event, {sid, payload}]);
    ];   

    (* multiple async tasks are not supported! just return server's id *)
    If[!TrueQ[task], internalTask = Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&]; task = True];
   

    internalTask
]

Options[createAsynchronousTask] = {"BufferSize"->2^11}

sockets = <||>;
socketOpen[host_String, port_String] := With[{uid = CreateUUID[] // Hash},
    sockets[uid] = {host, port};
    uid
]


createServer = LibraryFunctionLoad[$libFile, "create_server", {String, String}, Integer]; 
socketClose = LibraryFunctionLoad[$libFile, "close_socket", {Integer}, Integer]; 
socketBinaryWrite = LibraryFunctionLoad[$libFile, "socket_write", {Integer, "ByteArray", Integer, Integer}, Integer]; 
socketWriteString = LibraryFunctionLoad[$libFile, "socket_write_string", {Integer, String, Integer, Integer}, Integer]; 

socketConnectInternal = LibraryFunctionLoad[$libFile, "socket_connect", {String, String}, Integer];

buffers;

socketConnect[host_String, port_String] := With[{sid = socketConnectInternal[host, port]},
    If[!TrueQ[task], internalTask = Internal`CreateAsynchronousTask[runLoop, {0}, router[##]&]; task = True];
    
    buffers[sid] = {};

    router[_, event_, {sid, _, payload_}] := With[{data = ByteArray[payload]},
        If[Length[buffers[sid] ] == 0,
            buffers[sid] = data;
        ,
            buffers[sid] = Join[buffers[sid], data];
        ];
    ];

    TimeConstrained[
        While[getState[sid] =!= 1,
            Pause[0.3];
        ];

        sid

    , 10, $Failed]
]

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

getPort = LibraryFunctionLoad[$libFile, "get_socket_port", {Integer}, Integer]; 
getState = LibraryFunctionLoad[$libFile, "get_socket_state", {Integer}, Integer]; 

socketPort[uid_] := Null

End[]
EndPackage[]