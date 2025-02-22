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

getLibraryLinkVersion[] := getLibraryLinkVersion[] =
Which[
    $VersionNumber >= 14.1, 
        With[{n = LibraryVersionInformation[FindLibrary["demo"] ]["WolframLibraryVersion"]},
            If[!NumberQ[n], 7, n]
        ], 
    $VersionNumber >= 13.1, 
        7, 
    $VersionNumber >= 12.1, 
        6, 
    $VersionNumber >= 12.0, 
        5, 
    $VersionNumber >= 11.2, 
        4, 
    $VersionNumber >= 10.0, 
        3, 
    $VersionNumber >= 9.0, 
        2, 
    True, 
        1
]; 

$directory = DirectoryName[$InputFileName, 2];

$libFile := FileNameJoin[{
	$directory, 
	"LibraryResources", 
    $SystemID <> "-v" <> ToString[getLibraryLinkVersion[] ],
	"wsockets." <> Internal`DynamicLibraryExtension[]
}]; 

Echo["CSockets >> Windows >> Loading library... LLink v"<>ToString[getLibraryLinkVersion[] ] ];

While[FailureQ[
    socketOpen = LibraryFunctionLoad[$libFile, "socketOpen", {String, String}, Integer] 
] && getLibraryLinkVersion[] > 5,
    getLibraryLinkVersion[] = getLibraryLinkVersion[] - 1;
    Echo["CSockets >> Windows >> Another attempt to load v"<>ToString[getLibraryLinkVersion[] ] ];
];

If[getLibraryLinkVersion[] <= 5, 
    Echo["CSockets >> Windows >> Loading process failed. "];
    Exit[-1];
]

Echo["CSockets >> Windows >> Succesfully loaded!"];


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