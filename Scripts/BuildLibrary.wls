#!/usr/bin/env wolframscript
(* ::Package:: *)

Get["CCompilerDriver`"]; 


Block[{$directory, $libSrc, $libDir, $linkerOptions, $liblink = 7}, 
	$directory = DirectoryName[If[$InputFileName == "", 
		NotebookFileName[], 
		$InputFileName
	], 2]; 

	$libSrc = File[FileNameJoin[{
		$directory, 
		"Source", 
		If[$OperatingSystem === "Windows", "wsockets.c", "usockets.c"]
	}] ]; 


	$libDir = FileNameJoin[{
		$directory, 
		"LibraryResources", 
		ToString[$liblink],
		$SystemID
	}]; 


	If[!FileExistsQ[$libDir], CreateDirectory[]];

	CreateLibrary[$libSrc, "csockets", 
		"TargetDirectory" -> $libDir, 
		"Debug" -> True, 
		"Libraries" -> If[$OperatingSystem === "Windows", {"ws2_32"}, {"/opt/homebrew/lib/libuv.a"}],
		"IncludeDirectories" ->  If[$OperatingSystem === "Windows", {}, {"/opt/homebrew/include"}]
	]
]; 
