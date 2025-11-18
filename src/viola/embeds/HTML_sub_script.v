
/*
print("HTML_sub: self=", self(), ": ");
for (i=0; i<arg[]; i++) print("arg", i, " = ", arg[i], "\n");
print("^^^^^^^end\n");
*/
	switch (arg[0]) {
	case "entity":
		/* Add any pending label text first, parsing brackets */
		if (isBlank(get("label")) == 0) {
			label_text = get("label");
			label_len = strlen(label_text);
			text_start = 0;
			
			for (char_idx = 0; char_idx < label_len; char_idx++) {
				ch = nthChar(label_text, char_idx);
				
				if (ch == '(' || ch == ')' || ch == '[' || ch == ']') {
					/* Flush accumulated text before bracket */
					if (char_idx > text_start) {
						tok[tokCount] = 2; /* MINFO_DATA */
						data[tokCount] = nthChar(label_text, text_start, char_idx - 1);
						tokCount++;
					}
					
					/* Add bracket token */
					if (ch == '(') {
						tok[tokCount] = 15; /* MINFO_LPAREN */
					} else if (ch == ')') {
						tok[tokCount] = 16; /* MINFO_RPAREN */
					} else if (ch == '[') {
						tok[tokCount] = 17; /* MINFO_LBRACK */
					} else if (ch == ']') {
						tok[tokCount] = 18; /* MINFO_RBRACK */
					}
					data[tokCount] = "";
					tokCount++;
					
					text_start = char_idx + 1;
				}
			}
			
			/* Flush remaining text after last bracket */
			if (text_start < label_len) {
				tok[tokCount] = 2; /* MINFO_DATA */
				data[tokCount] = nthChar(label_text, text_start, label_len - 1);
				tokCount++;
			}
			
			set("label", "");
		}
		
		/* Then handle math entities inside sub */
		entity_number = arg[1];
		if (entity_number == 51) {/*infin*/
			tok[tokCount] = 21; /*MINFO_INFIN*/
			data[tokCount] = "";
			tokCount++;
		} else if (entity_number == 52) { /*integral*/
			tok[tokCount] = 19; /*MINFO_INTEGRAL*/
			data[tokCount] = "";
			tokCount++;
		} else if (entity_number == 65) { /*pi*/
			tok[tokCount] = 22; /*MINFO_PI*/
			data[tokCount] = "";
			tokCount++;
		} else if (entity_number == 68) { /*sigma*/
			tok[tokCount] = 20; /*MINFO_SUM*/
			data[tokCount] = "";
			tokCount++;
		}
		return;
	break;
	case 'i':
		if (findPattern(parent(), "box") != -1 ||
		    findPattern(parent(), "math") != -1) {
			SGMLBuildDoc_setBuff(0);
			if (isBlank(get("label")) == 0) {
				/* Parse bracket characters in label text */
				label_text = get("label");
				label_len = strlen(label_text);
				text_start = 0;
				
				for (char_idx = 0; char_idx < label_len; char_idx++) {
					ch = nthChar(label_text, char_idx);
					
					if (ch == '(' || ch == ')' || ch == '[' || ch == ']') {
						/* Flush accumulated text before bracket */
						if (char_idx > text_start) {
							tok[tokCount] = 2; /* MINFO_DATA */
							data[tokCount] = nthChar(label_text, text_start, char_idx - 1);
							tokCount++;
						}
						
						/* Add bracket token */
						if (ch == '(') {
							tok[tokCount] = 15; /* MINFO_LPAREN */
						} else if (ch == ')') {
							tok[tokCount] = 16; /* MINFO_RPAREN */
						} else if (ch == '[') {
							tok[tokCount] = 17; /* MINFO_LBRACK */
						} else if (ch == ']') {
							tok[tokCount] = 18; /* MINFO_RBRACK */
						}
						data[tokCount] = "";
						tokCount++;
						
						text_start = char_idx + 1;
					}
				}
				
				/* Flush remaining text after last bracket */
				if (text_start < label_len) {
					tok[tokCount] = 2; /* MINFO_DATA */
					data[tokCount] = nthChar(label_text, text_start, label_len - 1);
					tokCount++;
				}
			}
			test3();
			send(parent(), "flush");
			send(parent(), "tok", 9/*S_SUB*/);
/*
			for (i = 0; i < tokCount; i++) {
				print("sub_tok ", i, ":   type=", tok[i], 
					"  data={", data[i], "}\n");
			}
*/
			for (i = 0; i < tokCount; i++) {
				send(parent(), "tok&data", tok[i], data[i]);
			}
			send(parent(), "tok", 10/*E_SUB*/);
			tokCount = 0;
			return "";
		} else {
			pn = str(parent());
			tt = compressSpaces(get("label"));
			return concat("\\f(7)", tt, "\\f(1)");
		}
	break;
	case "tok&data":
		/* arg[1]	token
		 * arg[2]	data
		 */
		/* If token is MINFO_DATA (2), parse brackets in data */
		if (arg[1] == 2) {
			label_text = arg[2];
			label_len = strlen(label_text);
			text_start = 0;
			
			for (char_idx = 0; char_idx < label_len; char_idx++) {
				ch = nthChar(label_text, char_idx);
				
				if (ch == '(' || ch == ')' || ch == '[' || ch == ']') {
					/* Flush accumulated text before bracket */
					if (char_idx > text_start) {
						tok[tokCount] = 2; /* MINFO_DATA */
						data[tokCount] = nthChar(label_text, text_start, char_idx - 1);
						tokCount++;
					}
					
					/* Add bracket token */
					if (ch == '(') {
						tok[tokCount] = 15; /* MINFO_LPAREN */
					} else if (ch == ')') {
						tok[tokCount] = 16; /* MINFO_RPAREN */
					} else if (ch == '[') {
						tok[tokCount] = 17; /* MINFO_LBRACK */
					} else if (ch == ']') {
						tok[tokCount] = 18; /* MINFO_RBRACK */
					}
					data[tokCount] = "";
					tokCount++;
					
					text_start = char_idx + 1;
				}
			}
			
			/* Flush remaining text after last bracket */
			if (text_start < label_len) {
				tok[tokCount] = 2; /* MINFO_DATA */
				data[tokCount] = nthChar(label_text, text_start, label_len - 1);
				tokCount++;
			}
		} else {
			/* Non-data token, pass through */
			tok[tokCount] = arg[1];
			data[tokCount] = arg[2];
			tokCount++;
		}
		return;
	break;
	case "data":
		/* arg[1]	data - parse brackets */
		label_text = arg[1];
		label_len = strlen(label_text);
		text_start = 0;
		
		for (char_idx = 0; char_idx < label_len; char_idx++) {
			ch = nthChar(label_text, char_idx);
			
			if (ch == '(' || ch == ')' || ch == '[' || ch == ']') {
				/* Flush accumulated text before bracket */
				if (char_idx > text_start) {
					tok[tokCount] = 2; /* MINFO_DATA */
					data[tokCount] = nthChar(label_text, text_start, char_idx - 1);
					tokCount++;
				}
				
				/* Add bracket token */
				if (ch == '(') {
					tok[tokCount] = 15; /* MINFO_LPAREN */
				} else if (ch == ')') {
					tok[tokCount] = 16; /* MINFO_RPAREN */
				} else if (ch == '[') {
					tok[tokCount] = 17; /* MINFO_LBRACK */
				} else if (ch == ']') {
					tok[tokCount] = 18; /* MINFO_RBRACK */
				}
				data[tokCount] = "";
				tokCount++;
				
				text_start = char_idx + 1;
			}
		}
		
		/* Flush remaining text after last bracket */
		if (text_start < label_len) {
			tok[tokCount] = 2; /* MINFO_DATA */
			data[tokCount] = nthChar(label_text, text_start, label_len - 1);
			tokCount++;
		}
		return;
	break;
	case "tok":
		/* arg[1]	token type
		 */
		tok[tokCount] = arg[1];
		data[tokCount] = "";
		tokCount++;
		return;
	break;
	case "over":
		tok[tokCount] = 4;
		data[tokCount] = "";
		tokCount++;
		return;
	break;
	case "init":
		usual();
		SGMLBuildDoc_setColors();
		return;
	break;
	}
	usual();
