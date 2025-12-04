
	switch (arg[0]) {
	case "expose":
	case "R":
		return 0;
	break;
	case "AA":
		switch (arg[1]) {
		case "X":
			myRotX = float(arg[2]);
			hasX = 1;
			/* Send X rotation immediately - only set X, leave others as 0 */
			send(parent(), "setRotX", myRotX);
		break;
		case "Y":
			myRotY = float(arg[2]);
			hasY = 1;
			send(parent(), "setRotY", myRotY);
		break;
		case "Z":
			myRotZ = float(arg[2]);
			hasZ = 1;
			send(parent(), "setRotZ", myRotZ);
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
		hasX = 0;
		hasY = 0;
		hasZ = 0;
		return;
	break;
	}
	usual();


