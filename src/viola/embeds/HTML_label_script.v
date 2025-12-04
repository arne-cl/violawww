
	switch (arg[0]) {
	case "T":
		/* arg[1] = y
		 * arg[2] = width
		 */
		w = get("window");
		set("window", 0);
		set("width", arg[2] - 15);
		if (height() < 3) {
			set("height", 999);
		}
		set("x", 10);
		set("window", w);
		set("content", get("label"));
		set("y", arg[1]);
		h = building_vspan() + 3;
		set("height", h);
		h += 6;
		return h;
	break;
	case "R":
		/* arg[1]	y
		 * arg[2]	width
		 */
		clearWindow();
		set("y", arg[1]);
		set("width", arg[2]);
		set("content", get("content"));
		vspan = building_vspan();
		set("height", vspan);
		render();
		return vspan;
	break;
	case "D":
		/* Send label text to parent button if inside BUTTON */
		prim = send("HTML_button", "getCurrentPrimitive");
		if (prim != "" && prim != "0" && prim != "(NULL)" && exist(prim) == 1) {
			send(prim, "setLabel", labelText);
		}
		return 1;
	break;
	case "AI":
		/* Implied content - for SGML_CDATA the text is passed through AI */
		labelText = arg[2];
		set("label", labelText);
		/* Send to parent button if inside BUTTON */
		prim = send("HTML_button", "getCurrentPrimitive");
		if (prim != "" && prim != "0" && prim != "(NULL)" && exist(prim) == 1) {
			send(prim, "setLabel", labelText);
		}
		return;
	break;
	case "AA":
		/* Content may come through AA as well */
		if (arg[1] == "content" || arg[1] == "CONTENT") {
			labelText = arg[2];
			set("label", labelText);
			prim = send("HTML_button", "getCurrentPrimitive");
			if (prim != "" && prim != "0" && prim != "(NULL)" && exist(prim) == 1) {
				send(prim, "setLabel", labelText);
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
		labelText = "";
		return;
	break;
	}
	usual();
