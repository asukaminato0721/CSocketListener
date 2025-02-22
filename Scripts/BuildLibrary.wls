#!/usr/bin/env wolframscript
(* ::Package:: *)

Get["CCompilerDriver`"]; 


getLibraryLinkVersion[] := 
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

build[lib_String, opts: OptionsPattern[CreateLibrary]] := 
Block[{$directory, $libSrc, $libDir, $linkerOptions}, 
    $directory = DirectoryName[If[$InputFileName == "", 
        NotebookFileName[], 
        $InputFileName
    ], 2]; 

    $libSrc = File[FileNameJoin[{
        $directory, 
        "Source", 
        lib <> ".c"
    }]]; 

    $libDir = FileNameJoin[{
        $directory, 
        "LibraryResources", 
        $SystemID <> "-v" <> ToString[getLibraryLinkVersion[] ]
    }]; 


    If[!FileExistsQ[$libDir], CreateDirectory[]];

    CreateLibrary[$libSrc, lib, 
        "TargetDirectory" -> $libDir, 
        "Debug" -> False, 
        opts
    ]
]; 

If[$OperatingSystem === "Windows", 
	build["wsockets", "Libraries" -> {"ws2_32"}]
, 

	$includes = $ScriptCommandLine[[-2]];
	$libs = $ScriptCommandLine[[-1]];

	build["usockets", "Libraries" -> {$libs}, "Includes"->{$includes}] 
];


