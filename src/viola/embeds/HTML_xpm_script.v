/*
 * HTML_xpm_script.v - Inline XPM image handler
 *
 * Handles XPM (X PixMap) images with inline data provided via <FIGDATA>.
 * XPM is a color image format using C source code syntax.
 *
 * Features:
 *   - Parses XPM C code from FIGDATA content
 *   - Supports client-side image maps (FIGA hotspots)
 *   - Server-side image maps (ISMAP attribute)
 *
 * FIGA support messages:
 *   addFigA   - Creates an HTML_figa_actual child for each hotspot
 *   invertBox - Inverts a rectangular region (for hover effect)
 *   hintOn/Off - Shows/hides URL hint in status bar
 *   findTop   - Finds the top-level document for navigation
 *
 * Used when: <FIGURE TYPE="xpm"><FIGDATA>...XPM data...</FIGDATA></FIGURE>
 */
	switch (arg[0]) {
	case "D":
		set("label", get("label"));
		return get("height");
	break;
	case "R":
		/* arg[1]	y
		 * arg[2]	width
		 */
		clearWindow();
		if (style == 0) style = SGMLGetStyle("HTML", "XPM");
		vspan = style[0];
		set("y", arg[1] + vspan);
		set("x", style[2]);
		set("width", arg[2] - x() - style[3]);
		set("content", get("content"));
		vspan += get("height") + style[1];
		render();
		return vspan;
	break;
	case "config":
		return;
	break;
	case "addFigA":
		new = send("HTML_figa_actual", "clone");
		send(new, "make", self(), 
			arg[1], arg[2], arg[3], arg[4], arg[5]);
		objectListAppend("children", new);
		return;
	break;
	case "hintOn":
		return send(parent(), "hintOn", arg[1]);
	break;
	case "hintOff":
		return send(parent(), "hintOff");
	break;
	case "invertBox":
		invertRect(arg[1], arg[2], arg[3], arg[4]);
		return;
	break;
	case "findTop":
		return send(parent(), "findTop");
	break;
	case "gotoAnchor":
		return "";
	break;
	case "buttonPress":
		xy = mouseLocal();
		x0 = xy[0];
		y0 = xy[1];
		set("FGColor", "black");
		drawLine(x0 + 1, y0 - 5, x0 + 1, y0 + 5);
		drawLine(x0 + 5, y0 + 1, x0 - 5, y0 + 1);
		set("FGColor", "white");
		drawLine(x0, y0 - 5, x0, y0 + 5);
		drawLine(x0 + 5, y0, x0 - 5, y0);
	break;
	case "buttonRelease":
		xy = mouseLocal();
		x1 = xy[0];
		y1 = xy[1];
		top = send(parent(), "findTop");
		ref = concat(send(top, "query_docURL"), "?",
				x0, ",", y0, ",", x1, ",", y1, ";");

		set("FGColor", "black");
		drawLine(x1 + 1, y1 - 5, x1 + 1, y1 + 5);
		drawLine(x1 + 5, y1 + 1, x1 - 5, y1 + 1);
		set("FGColor", "white");
		drawLine(x1, y1 - 5, x1, y1 + 5);
		drawLine(x1 + 5, y1, x1 - 5, y1);
		send(top, "follow_href", ref);
	break;
	case "make":
		/* arg[1]	parent
		 * arg[2]	w
		 * arg[3]	h
		 * arg[4]	label (XPM data)
		 * arg[5]	ismap
		 */
		set("parent", arg[1]);
		set("width", arg[2]);
		set("label", arg[4]);
		ismap = arg[5];
		return get("height");
	break;
	case "clone":
		return clone(cloneID());
	break;
	case "init":
		usual();
		SGMLBuildDoc_setColors();
		return;
	break;
	}
	usual();
