
	switch (arg[0]) {
	case "T":
		return 0;
	break;
	case "R":
		return 0;
	break;
	case "D":
		/* Send hint text to parent primitive */
		prim = send("HTML_button", "getCurrentPrimitive");
		if (prim != "" && prim != "0" && prim != "(NULL)" && exist(prim) == 1) {
			send(prim, "setHint", hintText);
		}
		return 1;
	break;
	case "AA":
		switch (arg[1]) {
		case "TEXT":
			hintText = arg[2];
		break;
		}
		return;
	break;
	case "AI":
		/* Implied content - the text between <HINT> and </HINT> */
		if (arg[1] == "content") {
			hintText = arg[2];
			/* Send immediately to parent button */
			prim = send("HTML_button", "getCurrentPrimitive");
			if (prim != "" && prim != "0" && prim != "(NULL)" && exist(prim) == 1) {
				send(prim, "setHint", hintText);
			}
		}
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
		hintText = "";
		return;
	break;
	}
	usual();


