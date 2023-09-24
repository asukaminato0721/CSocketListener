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

$PublicDirectory = {FileNameJoin[{Directory[], "Benchmark"}]}

<<KirillBelov`Objects`
<<KirillBelov`Internal`
<<KirillBelov`TCPServer`

<<KirillBelov`HTTPHandler`
<<KirillBelov`HTTPHandler`Extensions`

Print["Staring HTTP server..."];

http1 = HTTPHandler[];

tcp1 = TCPServer[];
tcp1["CompleteHandler", "HTTP"] = HTTPPacketQ -> HTTPPacketLength;
tcp1["MessageHandler", "HTTP"] = HTTPPacketQ -> http1;

payload = ExportByteArray[ExampleData[{"TestImage", "House"}], "JPEG"];

http1["MessageHandler", "Files"] = GetFileRequestQ[{"png", "html", "js", "wln"}] -> (ImportFile[#, "Base" -> $PublicDirectory] &)

SocketListen[CSocketOpen[8010], tcp1@# &];


http2 = HTTPHandler[];

tcp2 = TCPServer[];
tcp2["CompleteHandler", "HTTP"] = HTTPPacketQ -> HTTPPacketLength;
tcp2["MessageHandler", "HTTP"] = HTTPPacketQ -> http2;

payload = ExportByteArray[ExampleData[{"TestImage", "House"}], "JPEG"];

http2["MessageHandler", "Files"] = GetFileRequestQ[{"png", "html", "js", "skp", "wln"}] -> (ImportFile[#, "Base" -> $PublicDirectory] &)

SocketListen[8011, tcp2@# &];


While[True, Pause[1]];

