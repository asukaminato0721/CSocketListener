#!/usr/bin/env wolframscript
(* ::Package:: *)

LINUXLIVUVPATH = "/home/kirill/Downloads/libuv-1.46.0/build/libuv.so"
LINUXLIBUVINCLUDE = "/home/kirill/Downloads/libuv-1.46.0/includes"

MACOSXLIBUVPATH = "/opt/homebrew/lib/libuv.a"

(*MACOSXLIBUVPATH = "/usr/local/lib/libuv.a"*)

MACOSXLIBUVINCLUDE = "/opt/homebrew/include"

(*MACOSXLIBUVINCLUDE = "/usr/local/include"*)

Get["CCompilerDriver`"]; 

Echo["CScockets >> version WL >> "<>$Version];
(*
$CCompiler={"Compiler"->GenericCCompiler,
 "CompilerInstallation"->"C:\\mingw64\\bin",
 "CompilerName"->"gcc.exe",
 "ShellOutputFunction"->Print}

 *)

OSIncludes = If[$OperatingSystem === "Windows", {}, If[$OperatingSystem === "MacOSX", {MACOSXLIBUVINCLUDE}, {LINUXLIBUVINCLUDE}]];

OSLibs = If[$OperatingSystem === "Windows", {"ws2_32"}, If[$OperatingSystem === "MacOSX", {MACOSXLIBUVPATH}, {LINUXLIVUVPATH}]];
OSPath = If[$OperatingSystem === "Windows", "Windows", "Unix"];

OSSrc = If[$OperatingSystem === "Windows", "wsockets.c", "csockets.c"];

Block[{$directory, $libSrc, $libDir}, 
	$directory = DirectoryName[If[$InputFileName == "", 
		NotebookFileName[], 
		$InputFileName
	], 2]; 


	$libSrc = File[FileNameJoin[{
		$directory, 
		"Source", 
		OSSrc
	}]]; 


	$libDir = FileNameJoin[{
		$directory, 
		"LibraryResources", 
		"7",
		$SystemID
	}]; 


	If[!FileExistsQ[$libDir], CreateDirectory[]];


	Get["CCompilerDriver`"]; 

	CreateLibrary[$libSrc, "csockets" , "TargetDirectory" -> $libDir, "Debug"->True, "Libraries"->OSLibs, "IncludeDirectories"->OSIncludes]
]; 
