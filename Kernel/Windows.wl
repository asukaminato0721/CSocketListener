BeginPackage["KirillBelov`CSockets`Interface`Windows`"]

createAsynchronousTask;

socketOpen;
socketClose;
socketListenerTaskRemove;
socketConnect;
socketBinaryWrite;
socketWriteString;
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

Echo["CSockets >> Windows >> " <> $SystemID];
Echo["CSockets >> Windows >> Loading library... LLink "<>ToString[$linkVersion] ];

If[$linkVersion === "JustTryToFind",
  $linkVersion = 7;
        If[!FileExistsQ[$libFile], 
            Echo["CSockets >> Windows >> Not found! LLink "<>ToString[$linkVersion] ];
            $buildLibrary;

            If[FailureQ[
	            socketOpen = LibraryFunctionLoad[$libFile, "socketOpen", {String, String}, Integer]; 
	        ],
                Echo["CSockets >> Windows >> Loading process failed. LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
        ,
            If[FailureQ[
	            socketOpen = LibraryFunctionLoad[$libFile, "socketOpen", {String, String}, Integer]; 
	        ],
                Echo["CSockets >> Windows >> It did not work! LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
        ]
,
        If[!FileExistsQ[$libFile], 
            Echo["CSockets >> Windows >> Not found! LLink "<>ToString[$linkVersion] ];
            $buildLibrary;

            If[FailureQ[
	            socketOpen = LibraryFunctionLoad[$libFile, "socketOpen", {String, String}, Integer]; 
	        ],
                Echo["CSockets >> Windows >> Loading process failed. LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
        ,

            Echo["CSockets >> Windows >> It did not work! LLink "<>ToString[$linkVersion] ];
            $linkVersion = 6;
            Echo["CSockets >> Windows >> Trying LLink "<>ToString[$linkVersion] ]; 

            If[FailureQ[
	            socketOpen = LibraryFunctionLoad[$libFile, "socketOpen", {String, String}, Integer]; 
	        ],
                Echo["CSockets >> Windows >> It did not work! LLink "<>ToString[$linkVersion] ];
                Exit[-1];
            ];
        ]
];

Echo["CSockets >> Windows >> Succesfully loaded! LLink "<>ToString[$linkVersion] ];


(* ::Section:: *)
(*Implementation*)

createAsynchronousTask[socketId_, handler_, OptionsPattern[] ] := With[{},
    Internal`CreateAsynchronousTask[socketListen, {socketId, OptionValue["BufferSize"]}, handler]
];

Options[createAsynchronousTask] = {"BufferSize"->2^11}


socketClose = LibraryFunctionLoad[$libFile, "socketClose", {Integer}, Integer]; 
socketListen = LibraryFunctionLoad[$libFile, "socketListen", {Integer, Integer}, Integer]; 
socketListenerTaskRemove = LibraryFunctionLoad[$libFile, "socketListenerTaskRemove", {Integer}, Integer]; 
socketConnect = LibraryFunctionLoad[$libFile, "socketConnect", {String, String}, Integer]; 
socketBinaryWrite = LibraryFunctionLoad[$libFile, "socketBinaryWrite", {Integer, "ByteArray", Integer, Integer}, Integer]; 
socketWriteString = LibraryFunctionLoad[$libFile, "socketWriteString", {Integer, String, Integer, Integer}, Integer]; 
socketReadyQ = LibraryFunctionLoad[$libFile, "socketReadyQ", {Integer}, True | False]; 
socketReadMessage = LibraryFunctionLoad[$libFile, "socketReadMessage", {Integer, Integer}, "ByteArray"]; 
socketPort = LibraryFunctionLoad[$libFile, "socketPort", {Integer}, Integer]; 


End[]
EndPackage[]