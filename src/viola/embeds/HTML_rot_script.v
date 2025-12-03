
	switch (arg[0]) {
	case "expose":
	case "R":
		return 0;
	break;
	case "AA":
		print("[ROT] AA: ", arg[1], "=", arg[2], "\n");
		switch (arg[1]) {
		case "X":
			rotX = float(arg[2]);
		break;
		case "Y":
			rotY = float(arg[2]);
		break;
		case "Z":
			rotZ = float(arg[2]);
			/* Z rotation is the 2D rotation - send to parent */
			print("[ROT] sending setRot to parent: ", rotZ, " degrees\n");
			send(parent(), "setRot", rotZ);
		break;
		}
		return;
	break;
	case "AI":
		return;
	break;
	case "config":
		return;
	break;
	case "gotoAnchor":
		return "";
	break;
	case "clone":
		return clone(cloneID());
	break;
	case "init":
		usual();
		rotX = 0;
		rotY = 0;
		rotZ = 0;
		print("[ROT] init: done\n");
		return;
	break;
	}
	usual();


