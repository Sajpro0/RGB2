As i am unable to format a Markdown file as of now, this is the current readme

This is a hopefully growing project for controlling homemade rgb systems
From the firmware on the microcontrollers to the software on the pc
Maybe even a web gui if someone is capable to do it

As for me, at the time of writing this, i am planning to do both the firmware and software
(in my case, fw for the ESP-01 or ESP-12E in C/Wiring (or maybe the ESP-IDE or how is it called if i can do it)
and a control sw in C# with a buncha features)

Features (FW):
	Minimal:
		Way to signal capabilities
		Process RGB inputs coming from network X wrapper (the network sw part being implementable) using a simple protocol
	Usually necessary:
		Ability to connect to the network (in case of wifi)
	Everything else:
		Data storage for all sorts of stuff to enable changing the network credentials etc
		Volume processing (react to volume instead of/along with rgb)
		Filters on the device (like fade out etc, mainly for volume so less packets are used)

Features (SW):
	Minimal:
		Static program that turns computer sound to rgb signals for the device or enables manual control of the lights
	Final wanted result:
		A server program, interconnecting rgb devices and control panels
		Control panel made in FNA featuring everything that can be done with the rgb protocol in a user friendly way
			(manual rgb set, reconfiguration, building rgb algorithms from code blocks, dynamically loading .NET dlls in the server for the audio an rgb processor)



refer to docs/progressive for active documentation and logging of everything