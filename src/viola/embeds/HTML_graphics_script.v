
	switch (arg[0]) {
	case "addChild":
		/* Child primitive registering itself - store in script variable */
		childObj = arg[1];
		if (gfxChildCount == 0) {
			gfxChildren = childObj;
		} else {
			gfxChildren = concat(gfxChildren, " ", childObj);
		}
		gfxChildCount = gfxChildCount + 1;
		print("[GRAPHICS] ", self(), " addChild: ", childObj, " count=", gfxChildCount, "\n");
		return;
	break;
	case "setBGColor":
		/* Set background color from BGCOLOR child tag */
		print("[GRAPHICS] setBGColor: ", arg[1], "\n");
		set("BGColor", arg[1]);
		return;
	break;
	case "expose":
		/* Container draws all children using own window */
		childList = gfxChildren;
		if (gfxChildCount > 0 && isBlank(childList) == 0) {
			/* Iterate through all children (space-separated list) */
			numChildren = 1;
			/* Count spaces to get number of children */
			for (idx = 0; idx < strlen(childList); idx++) {
				if (nthChar(childList, idx) == " ") {
					numChildren = numChildren + 1;
				}
			}
			
			/* Draw each child */
			startIdx = 0;
			for (childNum = 0; childNum < numChildren; childNum++) {
				/* Find next space or end */
				endIdx = startIdx;
				while (endIdx < strlen(childList) && nthChar(childList, endIdx) != " ") {
					endIdx = endIdx + 1;
				}
				
				/* Extract child name */
				if (endIdx > startIdx) {
					childName = nthChar(childList, startIdx, endIdx - 1);
					
					if (exist(childName) == 1) {
						/* Query child for shape type, color, and transforms */
						shapeType = send(childName, "getShapeType");
						shapeFG = send(childName, "getFG");
						shapeRot = send(childName, "getRot");
						shapeSX = send(childName, "getScaleX");
						shapeSY = send(childName, "getScaleY");
						shapeAX = send(childName, "getAxisX");
						shapeAY = send(childName, "getAxisY");
						
						/* Set color */
						if (isBlank(shapeFG) == 0) {
							set("FGColor", shapeFG);
						}
						
						/* Check if we have transforms to apply */
						/* Default values: rot=0 (or "0" or "1" when undefined), scale=1 */
						/* Note: undefined slots return "1" in Viola */
						hasTransform = 0;
						rotVal = float(shapeRot);
						sxVal = float(shapeSX);
						syVal = float(shapeSY);
						/* Rotation: anything non-zero except undefined "1" */
						if (rotVal > 1.001 || (rotVal > 0.001 && rotVal < 0.999)) hasTransform = 1;
						/* Scale: anything not 1.0 */
						if (sxVal > 1.001 || sxVal < 0.999) hasTransform = 1;
						if (syVal > 1.001 || syVal < 0.999) hasTransform = 1;
						
						if (shapeType == "polygon") {
							/* Polygon uses points */
							numPoints = send(childName, "getPoints");
							if (numPoints >= 2) {
								/* Get all points and optionally transform */
								for (pi = 0; pi < numPoints; pi++) {
									tpx[pi] = send(childName, "getPointX", pi);
									tpy[pi] = send(childName, "getPointY", pi);
									
									if (hasTransform == 1) {
										/* Apply scale then rotation around axis */
										tx = tpx[pi] - shapeAX;
										ty = tpy[pi] - shapeAY;
										/* Scale */
										tx = tx * shapeSX;
										ty = ty * shapeSY;
										/* Rotate */
										if (shapeRot != 0) {
											cosR = cos(shapeRot);
											sinR = sin(shapeRot);
											nx = tx * cosR - ty * sinR;
											ny = tx * sinR + ty * cosR;
											tx = nx;
											ty = ny;
										}
										/* Translate back */
										tpx[pi] = int(tx + shapeAX);
										tpy[pi] = int(ty + shapeAY);
									}
								}
								/* Draw lines connecting all points */
								for (pi = 0; pi < numPoints - 1; pi++) {
									drawLine(tpx[pi], tpy[pi], tpx[pi + 1], tpy[pi + 1]);
								}
								/* Close polygon */
								if (numPoints >= 3) {
									drawLine(tpx[numPoints - 1], tpy[numPoints - 1], tpx[0], tpy[0]);
								}
							}
						} else {
							/* Other shapes use X/Y/W/H */
							shapeX = send(childName, "getX");
							shapeY = send(childName, "getY");
							shapeW = send(childName, "getW");
							shapeH = send(childName, "getH");
							
							if (hasTransform == 1) {
								/* Convert shape to 4 corners for transformation */
								/* Top-left, top-right, bottom-right, bottom-left */
								cx[0] = shapeX;
								cy[0] = shapeY;
								cx[1] = shapeX + shapeW;
								cy[1] = shapeY;
								cx[2] = shapeX + shapeW;
								cy[2] = shapeY + shapeH;
								cx[3] = shapeX;
								cy[3] = shapeY + shapeH;
								
								/* Use center of shape as default axis if not specified */
								if (shapeAX == 0 && shapeAY == 0) {
									shapeAX = shapeX + shapeW / 2;
									shapeAY = shapeY + shapeH / 2;
								}
								
								/* Transform all 4 corners */
								for (ci = 0; ci < 4; ci++) {
									tx = cx[ci] - shapeAX;
									ty = cy[ci] - shapeAY;
									/* Scale */
									tx = tx * shapeSX;
									ty = ty * shapeSY;
									/* Rotate */
									if (shapeRot != 0) {
										cosR = cos(shapeRot);
										sinR = sin(shapeRot);
										nx = tx * cosR - ty * sinR;
										ny = tx * sinR + ty * cosR;
										tx = nx;
										ty = ny;
									}
									/* Translate back */
									cx[ci] = int(tx + shapeAX);
									cy[ci] = int(ty + shapeAY);
								}
								
								/* Draw transformed shape as polygon */
								if (shapeType == "rect") {
									/* Fill with 4 triangles from center */
									/* For simplicity, just draw outline */
									drawLine(cx[0], cy[0], cx[1], cy[1]);
									drawLine(cx[1], cy[1], cx[2], cy[2]);
									drawLine(cx[2], cy[2], cx[3], cy[3]);
									drawLine(cx[3], cy[3], cx[0], cy[0]);
								}
								if (shapeType == "circle" || shapeType == "oval") {
									/* Approximate with 16-point polygon for rotated/scaled ellipse */
									centerX = shapeAX;
									centerY = shapeAY;
									radiusX = (shapeW / 2) * shapeSX;
									radiusY = (shapeH / 2) * shapeSY;
									
									for (ai = 0; ai < 16; ai++) {
										angle = ai * 22.5; /* 360/16 = 22.5 degrees */
										epx = centerX + radiusX * cos(angle);
										epy = centerY + radiusY * sin(angle);
										/* Apply rotation */
										if (shapeRot != 0) {
											tx = epx - centerX;
											ty = epy - centerY;
											cosR = cos(shapeRot);
											sinR = sin(shapeRot);
											epx = int(centerX + tx * cosR - ty * sinR);
											epy = int(centerY + tx * sinR + ty * cosR);
										}
										ellipseX[ai] = epx;
										ellipseY[ai] = epy;
									}
									/* Draw the ellipse outline */
									for (ai = 0; ai < 15; ai++) {
										drawLine(ellipseX[ai], ellipseY[ai], ellipseX[ai + 1], ellipseY[ai + 1]);
									}
									drawLine(ellipseX[15], ellipseY[15], ellipseX[0], ellipseY[0]);
								}
								if (shapeType == "line") {
									drawLine(cx[0], cy[0], cx[2], cy[2]);
								}
							} else {
								/* No transform - draw normally */
								if (shapeType == "rect") {
									drawFillRect(shapeX, shapeY, shapeX + shapeW, shapeY + shapeH);
								}
								if (shapeType == "circle") {
									drawFillOval(shapeX, shapeY, shapeX + shapeW, shapeY + shapeH);
								}
								if (shapeType == "oval") {
									drawFillOval(shapeX, shapeY, shapeX + shapeW, shapeY + shapeH);
								}
								if (shapeType == "line") {
									drawLine(shapeX, shapeY, shapeX + shapeW, shapeY + shapeH);
								}
							}
						}
					}
				}
				startIdx = endIdx + 1;
			}
		}
		return;
	break;
	case "D":
		/* Done building - nothing special needed */
		print("[GRAPHICS] D: done building\n");
		return 1;
	break;
	case "R":
		/* arg[1]	y
		 * arg[2]	width
		 */
		print("[GRAPHICS] R: y=", arg[1], " width=", arg[2], "\n");
		if (style == 0) style = SGMLGetStyle("HTML", "GRAPHICS");
		vspan = style[0];
		set("y", arg[1] + vspan);
		set("x", style[2]);
		
		/* Use WIDTH/HEIGHT attributes if set, otherwise use available width */
		if (gWidth > 0) {
			set("width", gWidth);
		} else {
			set("width", arg[2] - get("x") - style[3]);
		}
		if (gHeight > 0) {
			set("height", gHeight);
		} else {
			set("height", 100); /* default height */
		}
		
		vspan += get("height") + style[1];
		print("[GRAPHICS] R: final size ", width(), "x", height(), " vspan=", vspan, "\n");
		return vspan;
	break;
	case "AA":
		print("[GRAPHICS] AA: ", arg[1], "=", arg[2], "\n");
		switch (arg[1]) {
		case "ID":
		case "NAME":
			tagID = arg[2];
		break;
		case "WIDTH":
			gWidth = int(arg[2]);
		break;
		case "HEIGHT":
			gHeight = int(arg[2]);
		break;
		}
		return;
	break;
	case "AI":
		/* Implied attribute - ignore */
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
		gWidth = 0;
		gHeight = 0;
		gfxChildCount = 0;
		gfxChildren = "";
		/* Use document colors */
		SGMLBuildDoc_setColors();
		print("[GRAPHICS] init: done\n");
		return;
	break;
	}
	usual();

