
	switch (arg[0]) {
	case "expose":
	case "R":
		return 0;
	break;
	case "AA":
		print("[AXIS] AA: ", arg[1], "=", arg[2], "\n");
		switch (arg[1]) {
		case "X":
			axisX = int(arg[2]);
			hasX = 1;
		break;
		case "Y":
			axisY = int(arg[2]);
			hasY = 1;
		break;
		case "Z":
			axisZ = int(arg[2]);
		break;
		}
		/* Send when we have both X and Y */
		if (hasX == 1 && hasY == 1) {
			print("[AXIS] sending setAxis to parent: ", axisX, ",", axisY, "\n");
			send(parent(), "setAxis", axisX, axisY);
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
		axisX = 0;
		axisY = 0;
		axisZ = 0;
		hasX = 0;
		hasY = 0;
		print("[AXIS] init: done\n");
		return;
	break;
	}
	usual();


