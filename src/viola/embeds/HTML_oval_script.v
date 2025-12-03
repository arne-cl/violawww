
	switch (arg[0]) {
	case "getShapeType":
		return "oval";
	break;
	case "getX":
		return posX;
	break;
	case "getY":
		return posY;
	break;
	case "getW":
		return sizeX;
	break;
	case "getH":
		return sizeY;
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
		print("[OVAL] D: done, self=", self(), " parent=", p, "\n");
		/* Only register if parent is a GRAPHICS container */
		if (findPattern(p, "HTML_graphics") >= 0) {
			print("[OVAL] D: registering with GRAPHICS parent\n");
			send(p, "addChild", self());
		}
		return 1; /* Keep object alive */
	break;
	case "R":
		return 0;
	break;
	case "AA":
		print("[OVAL] AA: ", arg[1], "=", arg[2], "\n");
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
		print("[OVAL] setPos: ", arg[1], ",", arg[2], "\n");
		posX = int(arg[1]);
		posY = int(arg[2]);
		return;
	break;
	case "setSize":
		print("[OVAL] setSize: ", arg[1], ",", arg[2], "\n");
		sizeX = int(arg[1]);
		sizeY = int(arg[2]);
		return;
	break;
	case "setFGColor":
		print("[OVAL] setFGColor: ", arg[1], "\n");
		fgColor = arg[1];
		return;
	break;
	case "setRot":
		print("[OVAL] setRot: ", arg[1], " degrees\n");
		set("gfxRotZ", arg[1]);
		return;
	break;
	case "setScale":
		print("[OVAL] setScale: ", arg[1], ",", arg[2], "\n");
		set("gfxScaleX", arg[1]);
		set("gfxScaleY", arg[2]);
		return;
	break;
	case "setAxis":
		print("[OVAL] setAxis: ", arg[1], ",", arg[2], "\n");
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
		posX = 0;
		posY = 0;
		sizeX = 0;
		sizeY = 0;
		fgColor = "black";
		print("[OVAL] init: done\n");
		return;
	break;
	}
	usual();

