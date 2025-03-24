//%attributes = {}
ARRAY TEXT:C222($voices; 0)
VOICES LIST($voices)

If (Is macOS:C1572)
	If (Macintosh option down:C545)
		SAY("when the solution matters...4D"; {rate: 180; volume: 1; voice: "com.apple.eloquence.en-GB.Grandma"}; \
			Folder:C1567(fk desktop folder:K87:19).file("test.aiff").platformPath)
	Else 
		SAY("when the solution matters...4D"; {rate: 180; volume: 1; voice: "com.apple.eloquence.en-GB.Grandma"})
	End if 
Else 
	//class not registed on non-US system?
	//SAY("when the solution matters...4D"; {rate: 4; volume: 100; voice: "Microsoft Zira Desktop - English (United States)"})
	If (Windows Alt down:C563)
		SAY("when the solution matters...4D"; {rate: 4; volume: 100; voice: "Microsoft Sayaka - Japanese (Japan)"}; \
			Folder:C1567(fk desktop folder:K87:19).file("test.wav").platformPath)
	Else 
		SAY("when the solution matters...4D"; {rate: 4; volume: 100; voice: "Microsoft Sayaka - Japanese (Japan)"})
	End if 
End if 