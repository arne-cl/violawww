# ViolaWWW Build System

## Overview
This document describes the build system for ViolaWWW browser on macOS (Darwin).

## Build System Features

### Automatic Dependency Tracking
The build system now includes **automatic header dependency tracking** using GCC's `-MMD -MP` flags.

#### How it works:
1. During compilation, the compiler generates `.d` files alongside `.o` files
2. These `.d` files contain dependency information for all `#include` headers
3. The Makefile automatically includes these `.d` files
4. When any header file changes, only the files that depend on it are recompiled

#### Example:
```makefile
# Automatic dependency generation
DEPFLAGS = -MMD -MP

# Compile rule generates both .o and .d files
$(VIOLA_DIR)/%.o: $(VIOLA_DIR)/%.c
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -I$(VIOLA_DIR) -I$(LIBWWW_DIR) -c $< -o $@

# Include generated dependencies
-include $(VIOLA_OBJS:.o=.d)
```

### Special Cases

#### gram.c Generation
`gram.c` is generated from `gram.y` using yacc:
```makefile
$(VIOLA_DIR)/gram.c: $(VIOLA_DIR)/gram.y
	@echo "=== Generating gram.c from gram.y ==="
	cd $(VIOLA_DIR) && $(YACC) gram.y && mv y.tab.c gram.c
```

**Dependency chain:**
```
gram.y  →  gram.c  →  gram.o
   ↓         ↓
#include   #include
"ast.h"    "ast.h"
```

When `ast.h` changes:
- ✅ `gram.o` is recompiled (tracked via `gram.d`)
- ✅ `gram.c` is NOT regenerated (only depends on `gram.y`)

## Build Commands

### Basic build
```bash
make              # Build everything (viola + vw)
make viola        # Build viola browser only
make vw           # Build VW (Motif interface)
```

### Clean builds
```bash
make clean        # Remove object files and dependencies
make distclean    # Complete clean including libraries
make rebuild      # distclean + all
```

### Partial builds
```bash
make libs         # Build all libraries only
make libs-clean   # Clean libraries only
```

## Build Configuration

### Compiler Flags
- `ARCH_FLAGS = -arch arm64` - Target ARM64 architecture
- `CFLAGS` - Standard C compilation flags
- `DEPFLAGS = -MMD -MP` - Dependency generation

### Directory Structure
```
src/
  ├── libWWW/        # HTTP/HTML library
  ├── libXPM/        # XPM image support
  ├── libXPA/        # X Pixmap Allocator
  ├── libIMG/        # Image loading
  ├── libStyle/      # Style library
  ├── viola/         # Viola browser core
  └── vw/            # VW Motif interface
```

## Dependency Files

Dependency files (`.d`) are automatically generated during compilation:
- Located next to their corresponding `.o` files
- Format: Make-compatible dependency lists
- Automatically included by the Makefile
- Cleaned with `make clean`

### Example gram.d:
```make
src/viola/gram.o: src/viola/gram.c src/viola/ast.h src/viola/cgen.h
src/viola/ast.h:
src/viola/cgen.h:
```

## Troubleshooting

### Headers not triggering rebuild
If changes to header files don't trigger recompilation:
1. Run `make clean` to remove old `.d` files
2. Run `make` to rebuild and regenerate dependencies

### yacc conflicts
The parser generator may show shift/reduce conflicts:
```
conflicts: 77 shift/reduce
```
This is normal for the Viola grammar.

## Platform Support

Currently tested on:
- macOS (Darwin) with ARM64 architecture
- Homebrew packages required: openmotif

The build system auto-detects Homebrew paths and configures includes/libraries accordingly.
