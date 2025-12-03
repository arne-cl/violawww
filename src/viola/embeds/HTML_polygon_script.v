
	switch (arg[0]) {
	case "getShapeType":
		return "polygon";
	break;
	case "getPoints":
		/* Return number of points */
		return pointCount;
	break;
	case "getPointX":
		return pointsX[arg[1]];
	break;
	case "getPointY":
		return pointsY[arg[1]];
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
		print("[POLYGON] D: done with ", pointCount, " points, parent=", p, "\n");
		if (findPattern(p, "HTML_graphics") >= 0) {
			print("[POLYGON] D: registering with GRAPHICS parent\n");
			send(p, "addChild", self());
		}
		return 1; /* Keep object alive */
	break;
	case "R":
		return 0;
	break;
	case "AA":
		print("[POLYGON] AA: ", arg[1], "=", arg[2], "\n");
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
	case "addPoint":
		print("[POLYGON] addPoint[", pointCount, "]: ", arg[1], ",", arg[2], "\n");
		pointsX[pointCount] = int(arg[1]);
		pointsY[pointCount] = int(arg[2]);
		pointCount = pointCount + 1;
		return;
	break;
	case "setFGColor":
		print("[POLYGON] setFGColor: ", arg[1], "\n");
		fgColor = arg[1];
		return;
	break;
	case "setRot":
		print("[POLYGON] setRot: ", arg[1], " degrees\n");
		set("gfxRotZ", arg[1]);
		return;
	break;
	case "setScale":
		print("[POLYGON] setScale: ", arg[1], ",", arg[2], "\n");
		set("gfxScaleX", arg[1]);
		set("gfxScaleY", arg[2]);
		return;
	break;
	case "setAxis":
		print("[POLYGON] setAxis: ", arg[1], ",", arg[2], "\n");
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
		pointCount = 0;
		fgColor = "black";
		print("[POLYGON] init: done\n");
		return;
	break;
	}
	usual();

