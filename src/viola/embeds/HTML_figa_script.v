/*
 * HTML_figa_script.v - FIGA tag attribute parser
 *
 * Handles the <FIGA> tag which defines clickable hotspot regions within
 * a <FIGURE> element. This is ViolaWWW's client-side image map mechanism,
 * predating HTML's standard <AREA>/<MAP> tags.
 *
 * Attributes:
 *   HREF - URL to navigate to when the hotspot is clicked
 *   AREA - Region coordinates as "x y width height"
 *
 * On initialization ('i'), this script sends the parsed coordinates to
 * the parent FIGURE via the "addFigA" message. The parent then creates
 * an HTML_figa_actual object to handle mouse interaction.
 *
 * Usage: <FIGA HREF="url" AREA="x y w h"></FIGA>
 */
	switch (arg[0]) {
	case 'i':
		send(parent(), "addFigA", ref,
			int(nthWord(area, 1)), int(nthWord(area, 2)),
			int(nthWord(area, 3)), int(nthWord(area, 4)));
		return -1;
	break;
	case "AA":
		switch (arg[1]) {
		case "AREA":
			area = arg[2];
		break;
		case "HREF":
			ref = arg[2];
		break;
		}
		return;
	break;
	case "AI":
		return 0;
	break;
	}
	usual();
