Graphics3D 800,600,0,2
SetBuffer BackBuffer()
t = 0
While Not KeyHit(1)
	c = (MilliSecs() / 1000) Mod 6
	Select c
		Case 0
			ClsColor 255,0,0
		Case 1
			ClsColor 0,255,0
		Case 2
			ClsColor 0,0,255
		Case 3
			ClsColor 255,255,0
		Case 4
			ClsColor 0,255,255
		Case 5
			ClsColor 255,0,255
	End Select
	Cls
	Flip
Wend
End
