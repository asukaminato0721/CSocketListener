#!/usr/bin/env wolframscript
(* ::Package:: *)

Get["CCompilerDriver`"]; 

$libs = If[$OperatingSystem === "Windows", {"ws2_32"}, {}];
$src  = If[$OperatingSystem === "Windows", "csockets.c", "usockets.c"];

Block[{$directory, $libSrc, $libDir, $linkerOptions}, 
	$directory = DirectoryName[If[$InputFileName == "", 
		NotebookFileName[], 
		$InputFileName
	], 2]; 

	$libSrc = File[FileNameJoin[{
		$directory, 
		"Source", 
		$src
	}]]; 


	$libDir = FileNameJoin[{
		$directory, 
		"LibraryResources", 
		$SystemID
	}]; 


	If[!FileExistsQ[$libDir], CreateDirectory[]];

	CreateLibrary[$libSrc, "csockets", 
		"TargetDirectory" -> $libDir, 
		"Debug" -> True,
		"Libraries" -> $libs,
		"IncludeDirectories" -> {FileNameJoin[{$directory, "Source", "lib"}]}
		(*,"CompileOptions"->{"-Og"}*)
	]
]; 