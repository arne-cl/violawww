
	switch (arg[0]) {
	case "expose":
	case "R":
		return 0;
	break;
	case "AA":
		print("[SCALE] AA: ", arg[1], "=", arg[2], "\n");
		switch (arg[1]) {
		case "X":
			scaleX = float(arg[2]);
			hasX = 1;
		break;
		case "Y":
			scaleY = float(arg[2]);
			hasY = 1;
		break;
		case "Z":
			scaleZ = float(arg[2]);
		break;
		}
		/* Send when we have both X and Y */
		if (hasX == 1 && hasY == 1) {
			print("[SCALE] sending setScale to parent: ", scaleX, ",", scaleY, "\n");
			send(parent(), "setScale", scaleX, scaleY);
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
		scaleX = 1;
		scaleY = 1;
		scaleZ = 1;
		hasX = 0;
		hasY = 0;
		print("[SCALE] init: done\n");
		return;
	break;
	}
	usual();


