#!/usr/bin/env wolframscript
(* ::Package:: *)

LINUXLIVUVPATH = "/home/kirill/Downloads/libuv-1.46.0/build/libuv.so"
MACOSXLIBUVPATH = "/usr/local/lib/libuv.a"

MACOSXLIBUVINCLUDE = "/usr/local/lib/libuv.a"
OSLinker = If[$OperatingSystem === "Windows", {}, If[$OperatingSystem === "MacOSX", {MACOSXLIBUVPATH<>" -pthread"}, {LINUXLIVUVPATH<>" -pthread"}]];

Get["CCompilerDriver`"]; 

$libs = If[$OperatingSystem === "Windows", {"ws2_32"}, {}];
$src  = If[$OperatingSystem === "Windows", "csockets.c", "csockets.c"];

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
		(*,"CompileOptions"->{"-Og"}*),
		"LinkerOptions"->OSLinker
	]
]; 
