
	switch (arg[0]) {
	case "getShapeType":
		return "line";
	break;
	case "getX":
		return x1;
	break;
	case "getY":
		return y1;
	break;
	case "getW":
		return x2 - x1;
	break;
	case "getH":
		return y2 - y1;
	break;
	case "getFG":
		return fgColor;
	break;
	case "getRot":
		return get("gfxRotZ");
	break;
	case "getScaleX":
		return get("gfxScaleX");
	break;
	case "getScaleY":
		return get("gfxScaleY");
	break;
	case "getAxisX":
		return get("gfxAxisX");
	break;
	case "getAxisY":
		return get("gfxAxisY");
	break;
	case "expose":
		return;
	break;
	case "D":
		/* Register with parent container */
		p = parent();
		print("[LINE] D: done, self=", self(), " parent=", p, "\n");
		/* Only register if parent is a GRAPHICS container */
		if (findPattern(p, "HTML_graphics") >= 0) {
			print("[LINE] D: registering with GRAPHICS parent\n");
			send(p, "addChild", self());
		}
		return 1; /* Keep object alive */
	break;
	case "R":
		return 0;
	break;
	case "AA":
		print("[LINE] AA: ", arg[1], "=", arg[2], "\n");
		switch (arg[1]) {
		case "ID":
		case "NAME":
			tagID = arg[2];
		break;
		}
		return;
	break;
	case "AI":
		return;
	break;
	case "setPos":
		/* For LINE, POS sets the start point */
		print("[LINE] setPos (start): ", arg[1], ",", arg[2], "\n");
		x1 = int(arg[1]);
		y1 = int(arg[2]);
		return;
	break;
	case "setSize":
		/* For LINE, SIZE sets the end point relative to start */
		print("[LINE] setSize (end offset): ", arg[1], ",", arg[2], "\n");
		x2 = x1 + int(arg[1]);
		y2 = y1 + int(arg[2]);
		return;
	break;
	case "setFGColor":
		print("[LINE] setFGColor: ", arg[1], "\n");
		fgColor = arg[1];
		return;
	break;
	case "setRot":
		print("[LINE] setRot: ", arg[1], " degrees\n");
		set("gfxRotZ", arg[1]);
		return;
	break;
	case "setScale":
		print("[LINE] setScale: ", arg[1], ",", arg[2], "\n");
		set("gfxScaleX", arg[1]);
		set("gfxScaleY", arg[2]);
		return;
	break;
	case "setAxis":
		print("[LINE] setAxis: ", arg[1], ",", arg[2], "\n");
		set("gfxAxisX", arg[1]);
		set("gfxAxisY", arg[2]);
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
		x1 = 0;
		y1 = 0;
		x2 = 0;
		y2 = 0;
		fgColor = "black";
		print("[LINE] init: done\n");
		return;
	break;
	}
	usual();

