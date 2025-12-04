
	switch (arg[0]) {
	case "expose":
	case "R":
		return 0;
	break;
	case "AA":
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
			hasZ = 1;
		break;
		}
		/* Send whenever we have at least X/Y; Z is optional */
		if (hasX == 1 && hasY == 1) {
			send(parent(), "setAxis", axisX, axisY, axisZ);
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
		hasZ = 0;
		return;
	break;
	}
	usual();


