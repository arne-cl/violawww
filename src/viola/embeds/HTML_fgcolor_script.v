
	switch (arg[0]) {
	case "expose":
	case "R":
		return 0;
	break;
	case "AA":
		switch (arg[1]) {
		case "NAME":
			colorName = arg[2];
			/* Send immediately */
			send(parent(), "setFGColor", colorName);
		break;
		case "RGB":
			colorRGB = arg[2];
			/* Send immediately */
			send(parent(), "setFGColor", colorRGB);
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
		colorName = "";
		colorRGB = "";
		return;
	break;
	}
	usual();

