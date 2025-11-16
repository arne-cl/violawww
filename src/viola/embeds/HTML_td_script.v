
	switch (arg[0]) {
	case "expose":
		return;
	break;
	case "F":
		h = 0;
		if (isBlank(get("label")) == 0) {
			if (style_p == 0) style_p = SGMLGetStyle("HTML", "P");
			txtObj = HTML_txt("clone");
			objectListAppend_children(txtObj);
			h = send(txtObj, "make", self(),
				compressSpaces(get("label")),
				style_p[3], 
				get("width") - style_p[3] - style_p[2],
				SGMLBuildDoc_span(), makeAnchor);
			makeAnchor = 0;
		}
		if (h == 0) return 1;
		set("height", h);
		return h;
	break;
	case "D":
		h = get("height");
		SGMLBuildDoc_setBuff(0);
/* Debug: removed to reduce log spam */
		/* Build pending math if we buffered entities earlier */
		if (mEntCount > 0) {
			if (mathObj == 0) {
				mathObj = HTML_math("clone");
				objectListAppend_children(mathObj);
				print("TD: D created HTML_math from pending, child=", mathObj, "\n");
			}
			/* Ensure math window is created/placed like text objects */
			if (style_p == 0) style_p = SGMLGetStyle("HTML", "P");
			send(mathObj, "make", self(),
				"", /* no initial label */
				style_p[3],
				get("width") - style_p[3] - style_p[2],
				h, makeAnchor);
			makeAnchor = 0;
			if (isBlank(get("label")) == 0) {
				print("TD: D flushing label to math as data (pending): '{", get("label"), "}'\n");
				send(mathObj, "data", get("label"));
				set("label", "");
			}
		for (i = 0; i < mEntCount; i++) {
			if (mEnt[i] == 51) {
				print("TD: D pending tok MINFO_INFIN\n");
				send(mathObj, "tok", 21);
			} else if (mEnt[i] == 52) {
				print("TD: D pending tok MINFO_INTEGRAL\n");
				send(mathObj, "tok", 19);
			} else if (mEnt[i] == 67) {
				print("TD: D pending tok MINFO_SUM\n");
				send(mathObj, "tok", 20);
			}
		}
			mEntCount = 0;
		}
		if (isBlank(get("label")) == 0) {
			if (style_p == 0) style_p = SGMLGetStyle("HTML", "P");
			txtObj = HTML_txt("clone");
			objectListAppend_children(txtObj);
			h += send(txtObj, "make", self(),
				compressSpaces(get("label")),
				style_p[3],
				get("width") - style_p[3] - style_p[2],
				h, makeAnchor);
			makeAnchor = 0;
		}

		/* Layout existing children (e.g., HTML_math) to contribute to height */
		if (style == 0) style = SGMLGetStyle("HTML", "TD");
		vspan = 0;
		n = countChildren();
		print("TD: D children count=", n, "\n");
		if (n) {
			xx = get("width");
			for (i = 0; i < n; i++) {
				child = nthChild(i);
				rv = send(child, 'R', h + vspan, xx);
				print("TD: D child[", i, "]=", child, " contributed vspan=", rv, "\n");
				vspan += rv;
			}
		}
		h += vspan;
		print("TD: D total h=", h, "\n");

		cellType = 16; /* TABLE_CELL_TYPE_TD == 16 */
		if (send(parent(), 'b')) set("border", 6);
		set("height", h);
		return h;
	break;
	case 'R':
		/* arg[1]	y
		 * arg[2]	width
		 */
		if (style == 0) style = SGMLGetStyle("HTML", "TD");
		vspan = style[0];
		set("x", style[2]);
		set("y", arg[1] + vspan);
		set("width", arg[2] - get("x") - style[3]);

		n = countChildren();
		print("TD: R start y=", get("y"), " width=", get("width"), " children=", n, "\n");
		if (n) {
			xx = get("width");
			for (i = 0; i < n; i++) {
				child = nthChild(i);
				rv = send(child, 'R', vspan, xx);
				print("TD: R child[", i, "]=", child, " vspan=", rv, "\n");
				vspan += rv;
			}
		}
		set("height", vspan);
		vspan += style[1];
		return vspan;
	break;
	case 'r':
		if (style == 0) style = SGMLGetStyle("HTML", "TD");
		vspan = style[0];
		n = countChildren();
		if (n) {
			xx = get("width");
			for (i = 0; i < n; i++)
				vspan += send(nthChild(i), 'R', vspan, xx);
		}
		set("height", vspan);
		vspan += style[1];
		return vspan;
	break;
	case "data":
		SGMLBuildDoc_setBuff(-1);
		return compressSpaces(get("label"));
	break;
	case "heightP":
		return get("height");
	break;
	case "widthP":
		return get("width");
	break;
	case "noBullet":
		return;
	break;
	case "AA":
		switch (arg[1]) {
		case "ID":
			tagID = arg[2];
		break;
		case "COLSPAN":
			colSpan = arg[2];
		break;
		case "ROWSPAN":
			rowSpan = arg[2];
		break;
		case "MAXWIDTH":
			set("maxWidth", arg[2]);
		break;
		case "MINWIDTH":
			set("minWidth", arg[2]);
		break;
		}
		return;
	break;
	case "entity":
		/* Route special entities into an inline HTML_math child to render math */
		entity_number = arg[1];
		print("TD: entity received #", entity_number, " label='{", get("label"), "}'\n");
		/* Buffer entity; create math in D when layout happens */
		mEnt[mEntCount] = entity_number;
		mEntCount++;
		return;
	break;
	case "inPreP":
		if (doneSetInPreP == 0) {
			inPreP = send(get("parent"), "inPreP");
			doneSetInPreP = 1;
		}
		return inPreP;
	break;
	case "searchAnchor":
		if (tagID == arg[1]) return self();
		n = countChildren();
		if (n > 0) {
			for (i = 0; i < n; i++) {
				anchor = send(nthChild(i), 
						"searchAnchor", arg[1]);
				if (anchor != "") return anchor;
			}
		}
		return "";
	break;
	case "gotoAnchor":
		if (tagID == arg[1]) return get("y");
		n = countChildren();
		if (n > 0) {
			for (i = 0; i < n; i++) {
				offset = send(nthChild(i), 
						"gotoAnchor", arg[1]);
				if (offset > 0) return offset + get("y");
			}
		}
		return 0;
	break;
	case "findTop":
	case "findForm":
		return send(parent(), arg[0]);
	break;
	case "init":
		usual();
		SGMLBuildDoc_setColors();
		color = getResource("Viola.foreground_doc");
		if (color) set("BDColor", color);
		return;
	break;
	}
	usual();
