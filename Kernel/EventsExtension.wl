BeginPackage["KirillBelov`CSockets`EventsExtension`", {"KirillBelov`CSockets`","JerryI`Misc`Events`"}]; 

CSocketsClosingHandler = (EventFire["csocket-"<>ToString[# // First], "Closed", True])&

CSocketObject /: EventFire[CSocketObject[uid_], opts__] := EventFire["csocket-"<>ToString[uid], opts ]
CSocketObject /: EventRemove[CSocketObject[uid_] ] := EventRemove["csocket-"<>ToString[uid] ]
CSocketObject /: EventClone[CSocketObject[uid_] ]  := EventClone["csocket-"<>ToString[uid] ]
CSocketObject /: EventHandler[CSocketObject[uid_], opts__ ]  := EventHandler["csocket-"<>ToString[uid], opts ]

EndPackage[]