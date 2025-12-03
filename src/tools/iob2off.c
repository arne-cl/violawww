/*
 * iob2off - Convert Imagine IOB (TDDD) files to OFF format
 *
 * Copyright (c) 2025 Evgeny Stepanischev
 *
 * This tool converts 3D object files from Imagine's IOB format (also known as
 * TDDD - Turbo Silver Data Description) to the simple OFF (Object File Format)
 * polygon mesh format.
 *
 * Imagine was a popular 3D modeling and rendering program originally developed
 * for the Amiga computer in the late 1980s. The IOB format uses IFF (Interchange
 * File Format) structure with TDDD type identifier.
 *
 * IOB file structure:
 *   FORM TDDD
 *     OBJ  - object container
 *       DESC - object description containing:
 *         NAME - object name (null-terminated string)
 *         SHAP - shape type
 *         POSI - position (3 x fixed-point)
 *         AXIS - transformation matrix (9 x fixed-point)
 *         SIZE - scale factors (3 x fixed-point)
 *         PNTS - vertices (count + count x 3 x fixed-point coordinates)
 *         EDGE - edges (count + count x 2 x uint16 vertex indices)
 *         FACE - faces (count + count x 3 x uint16 edge indices)
 *         COLR, REFL, TRAN, etc. - material properties
 *       TOBJ - terminate object marker
 *
 * Fixed-point format: 32-bit signed, 16.16 format (divide by 65536)
 *
 * Usage: iob2off [-v] [-n] input.iob [output.off]
 *   -v, --verbose       Print debug information
 *   -n, --no-transform  Output raw coordinates without transformation
 *   -h, --help          Show help
 *
 * If output file is not specified, writes to stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* IFF chunk IDs */
#define MAKE_ID(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

#define ID_FORM MAKE_ID('F','O','R','M')
#define ID_TDDD MAKE_ID('T','D','D','D')
#define ID_OBJ  MAKE_ID('O','B','J',' ')
#define ID_DESC MAKE_ID('D','E','S','C')
#define ID_NAME MAKE_ID('N','A','M','E')
#define ID_SHAP MAKE_ID('S','H','A','P')
#define ID_POSI MAKE_ID('P','O','S','I')
#define ID_AXIS MAKE_ID('A','X','I','S')
#define ID_SIZE MAKE_ID('S','I','Z','E')
#define ID_PNTS MAKE_ID('P','N','T','S')
#define ID_EDGE MAKE_ID('E','D','G','E')
#define ID_FACE MAKE_ID('F','A','C','E')
#define ID_CLST MAKE_ID('C','L','S','T')
#define ID_TLST MAKE_ID('T','L','S','T')
#define ID_COLR MAKE_ID('C','O','L','R')
#define ID_REFL MAKE_ID('R','E','F','L')
#define ID_TRAN MAKE_ID('T','R','A','N')
#define ID_SPC1 MAKE_ID('S','P','C','1')
#define ID_TOBJ MAKE_ID('T','O','B','J')

/* Maximum limits */
#define MAX_VERTICES 100000
#define MAX_EDGES    200000
#define MAX_FACES    100000
#define MAX_FACE_VERTS 64
#define MAX_OBJECTS  1000

/* Vertex structure */
typedef struct {
    double x, y, z;
} Vertex;

/* Edge structure */
typedef struct {
    uint16_t v1, v2;
} Edge;

/* Face structure - list of vertex indices */
typedef struct {
    int num_verts;
    int verts[MAX_FACE_VERTS];
} Face;

/* Object structure */
typedef struct {
    char name[256];
    int num_vertices;
    int num_edges;
    int num_faces;
    Vertex *vertices;
    Edge *edges;
    Face *faces;
    /* Transform */
    double pos[3];
    double axis[9];
    double size[3];
} Object;

/* Global state */
static Object objects[MAX_OBJECTS];
static int num_objects = 0;
static int verbose = 0;
static int no_transform = 0;

/* Read big-endian uint32 */
static uint32_t read_u32(FILE *f) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, f) != 4) return 0;
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | 
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

/* Read big-endian uint16 */
static uint16_t read_u16(FILE *f) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, f) != 2) return 0;
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/* Read 32-bit fixed-point number (16.16 format) and convert to double */
static double read_fract(FILE *f) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, f) != 4) return 0.0;
    
    int32_t val = ((int32_t)buf[0] << 24) | ((int32_t)buf[1] << 16) | 
                  ((int32_t)buf[2] << 8) | (int32_t)buf[3];
    
    return (double)val / 65536.0;
}

/* Convert chunk ID to string for debugging */
static const char *id_to_str(uint32_t id) {
    static char buf[8];
    buf[0] = (id >> 24) & 0xFF;
    buf[1] = (id >> 16) & 0xFF;
    buf[2] = (id >> 8) & 0xFF;
    buf[3] = id & 0xFF;
    buf[4] = 0;
    return buf;
}

/* Skip to next word boundary (IFF chunks are word-aligned) */
static void align_word(FILE *f) {
    long pos = ftell(f);
    if (pos & 1) fseek(f, 1, SEEK_CUR);
}

/* Parse PNTS chunk - vertices */
static int parse_pnts(FILE *f, uint32_t size, Object *obj) {
    long start = ftell(f);
    long end = start + size;
    
    uint16_t count = read_u16(f);
    
    if (verbose) {
        fprintf(stderr, "  PNTS: %d vertices (chunk size %u)\n", count, size);
    }
    
    obj->vertices = malloc(count * sizeof(Vertex));
    if (!obj->vertices) {
        fprintf(stderr, "Error: Cannot allocate memory for %d vertices\n", count);
        return -1;
    }
    obj->num_vertices = count;
    
    for (int i = 0; i < count && ftell(f) < end; i++) {
        obj->vertices[i].x = read_fract(f);
        obj->vertices[i].y = read_fract(f);
        obj->vertices[i].z = read_fract(f);
        
        if (verbose && i < 5) {
            fprintf(stderr, "    v[%d]: %.4f, %.4f, %.4f\n", 
                    i, obj->vertices[i].x, obj->vertices[i].y, obj->vertices[i].z);
        }
    }
    
    fseek(f, end, SEEK_SET);
    return 0;
}

/* Parse EDGE chunk - edges */
static int parse_edge(FILE *f, uint32_t size, Object *obj) {
    long start = ftell(f);
    long end = start + size;
    
    uint16_t count = read_u16(f);
    
    if (verbose) {
        fprintf(stderr, "  EDGE: %d edges (chunk size %u)\n", count, size);
    }
    
    obj->edges = malloc(count * sizeof(Edge));
    if (!obj->edges) {
        fprintf(stderr, "Error: Cannot allocate memory for %d edges\n", count);
        return -1;
    }
    obj->num_edges = count;
    
    for (int i = 0; i < count && ftell(f) < end; i++) {
        obj->edges[i].v1 = read_u16(f);
        obj->edges[i].v2 = read_u16(f);
        
        if (verbose && i < 5) {
            fprintf(stderr, "    e[%d]: %d -> %d\n", 
                    i, obj->edges[i].v1, obj->edges[i].v2);
        }
    }
    
    fseek(f, end, SEEK_SET);
    return 0;
}

/* Parse FACE chunk - faces defined by edge indices
 * Each face is a triangle defined by 3 edge indices.
 * We reconstruct the vertex list from the edge chain.
 */
static int parse_face(FILE *f, uint32_t size, Object *obj) {
    long start = ftell(f);
    long end = start + size;
    
    uint16_t count = read_u16(f);
    
    if (verbose) {
        fprintf(stderr, "  FACE: %d faces (chunk size %u)\n", count, size);
    }
    
    obj->faces = malloc(count * sizeof(Face));
    if (!obj->faces) {
        fprintf(stderr, "Error: Cannot allocate memory for %d faces\n", count);
        return -1;
    }
    obj->num_faces = count;
    
    for (int i = 0; i < count && ftell(f) < end; i++) {
        Face *face = &obj->faces[i];
        face->num_verts = 0;
        
        uint16_t e0 = read_u16(f);
        uint16_t e1 = read_u16(f);
        uint16_t e2 = read_u16(f);
        
        if (e0 >= obj->num_edges || e1 >= obj->num_edges || e2 >= obj->num_edges) {
            if (verbose) {
                fprintf(stderr, "Warning: Invalid edge indices in face %d: %d, %d, %d\n", 
                        i, e0, e1, e2);
            }
            face->num_verts = 0;
            continue;
        }
        
        Edge *edge0 = &obj->edges[e0];
        Edge *edge1 = &obj->edges[e1];
        Edge *edge2 = &obj->edges[e2];
        
        int v0 = edge0->v1;
        int v1 = edge0->v2;
        int v2 = -1;
        
        if (edge1->v1 == v0 || edge1->v1 == v1) {
            v2 = edge1->v2;
        } else if (edge1->v2 == v0 || edge1->v2 == v1) {
            v2 = edge1->v1;
        } else {
            if (edge2->v1 == v0 || edge2->v1 == v1) {
                v2 = edge2->v2;
            } else if (edge2->v2 == v0 || edge2->v2 == v1) {
                v2 = edge2->v1;
            }
        }
        
        if (v2 == -1) {
            if (verbose) {
                fprintf(stderr, "Warning: Edges don't connect in face %d\n", i);
            }
            face->num_verts = 0;
            continue;
        }
        
        if (edge1->v1 == v1 || edge1->v2 == v1 || edge2->v1 == v1 || edge2->v2 == v1) {
            face->verts[0] = v0;
            face->verts[1] = v1;
            face->verts[2] = v2;
        } else {
            face->verts[0] = v1;
            face->verts[1] = v0;
            face->verts[2] = v2;
        }
        face->num_verts = 3;
        
        if (verbose && i < 5) {
            fprintf(stderr, "    f[%d]: edges(%d,%d,%d) -> verts(%d,%d,%d)\n", 
                    i, e0, e1, e2, face->verts[0], face->verts[1], face->verts[2]);
        }
    }
    
    fseek(f, end, SEEK_SET);
    return 0;
}

/* Parse DESC chunk - object description */
static int parse_desc(FILE *f, uint32_t size, Object *obj) {
    long start = ftell(f);
    long end = start + size;
    
    memset(obj, 0, sizeof(Object));
    obj->size[0] = obj->size[1] = obj->size[2] = 1.0;
    obj->axis[0] = obj->axis[4] = obj->axis[8] = 1.0;
    
    while (ftell(f) < end) {
        uint32_t chunk_id = read_u32(f);
        uint32_t chunk_size = read_u32(f);
        long chunk_start = ftell(f);
        
        if (verbose) {
            fprintf(stderr, "  Chunk: %s, size: %u at offset 0x%lx\n", 
                    id_to_str(chunk_id), chunk_size, chunk_start);
        }
        
        switch (chunk_id) {
        case ID_NAME:
            fread(obj->name, 1, chunk_size < 255 ? chunk_size : 255, f);
            obj->name[255] = 0;
            if (verbose) fprintf(stderr, "    Name: %s\n", obj->name);
            break;
            
        case ID_POSI:
            obj->pos[0] = read_fract(f);
            obj->pos[1] = read_fract(f);
            obj->pos[2] = read_fract(f);
            if (verbose) {
                fprintf(stderr, "    Position: %.4f, %.4f, %.4f\n", 
                        obj->pos[0], obj->pos[1], obj->pos[2]);
            }
            break;
            
        case ID_AXIS:
            for (int i = 0; i < 9; i++) {
                obj->axis[i] = read_fract(f);
            }
            break;
            
        case ID_SIZE:
            obj->size[0] = read_fract(f);
            obj->size[1] = read_fract(f);
            obj->size[2] = read_fract(f);
            if (verbose) {
                fprintf(stderr, "    Size: %.4f, %.4f, %.4f\n", 
                        obj->size[0], obj->size[1], obj->size[2]);
            }
            break;
            
        case ID_PNTS:
            if (parse_pnts(f, chunk_size, obj) < 0) return -1;
            break;
            
        case ID_EDGE:
            if (parse_edge(f, chunk_size, obj) < 0) return -1;
            break;
            
        case ID_FACE:
            if (parse_face(f, chunk_size, obj) < 0) return -1;
            break;
            
        default:
            break;
        }
        
        fseek(f, chunk_start + chunk_size, SEEK_SET);
        align_word(f);
    }
    
    return 0;
}

/* Parse OBJ chunk */
static int parse_obj(FILE *f, uint32_t size) {
    long start = ftell(f);
    long end = start + size;
    
    while (ftell(f) < end && num_objects < MAX_OBJECTS) {
        uint32_t chunk_id = read_u32(f);
        uint32_t chunk_size = read_u32(f);
        
        if (verbose) {
            fprintf(stderr, "OBJ sub-chunk: %s, size: %u\n", 
                    id_to_str(chunk_id), chunk_size);
        }
        
        if (chunk_id == ID_DESC) {
            Object *obj = &objects[num_objects];
            if (parse_desc(f, chunk_size, obj) == 0) {
                if (obj->num_vertices > 0) {
                    num_objects++;
                }
            }
        } else if (chunk_id == ID_TOBJ) {
            /* TOBJ = Terminate Object, size is always 0 */
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
        
        align_word(f);
    }
    
    return 0;
}

/* Parse TDDD file */
static int parse_tddd(FILE *f, uint32_t size) {
    long start = ftell(f);
    long end = start + size;
    
    while (ftell(f) < end) {
        uint32_t chunk_id = read_u32(f);
        uint32_t chunk_size = read_u32(f);
        
        if (verbose) {
            fprintf(stderr, "TDDD chunk: %s, size: %u\n", 
                    id_to_str(chunk_id), chunk_size);
        }
        
        if (chunk_id == ID_OBJ) {
            parse_obj(f, chunk_size);
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
        
        align_word(f);
    }
    
    return 0;
}

/* Parse IFF FORM */
static int parse_form(FILE *f) {
    uint32_t form_id = read_u32(f);
    if (form_id != ID_FORM) {
        fprintf(stderr, "Error: Not an IFF FORM file (got %s)\n", id_to_str(form_id));
        return -1;
    }
    
    uint32_t form_size = read_u32(f);
    uint32_t form_type = read_u32(f);
    
    if (verbose) {
        fprintf(stderr, "FORM %s, size: %u\n", id_to_str(form_type), form_size);
    }
    
    if (form_type != ID_TDDD) {
        fprintf(stderr, "Error: Not a TDDD file (got %s)\n", id_to_str(form_type));
        return -1;
    }
    
    return parse_tddd(f, form_size - 4);
}

/* Apply transformation to a vertex */
static void transform_vertex(Object *obj, Vertex *v, Vertex *out) {
    double sx = v->x * obj->size[0];
    double sy = v->y * obj->size[1];
    double sz = v->z * obj->size[2];
    
    double rx = sx * obj->axis[0] + sy * obj->axis[1] + sz * obj->axis[2];
    double ry = sx * obj->axis[3] + sy * obj->axis[4] + sz * obj->axis[5];
    double rz = sx * obj->axis[6] + sy * obj->axis[7] + sz * obj->axis[8];
    
    out->x = rx + obj->pos[0];
    out->y = ry + obj->pos[1];
    out->z = rz + obj->pos[2];
}

/* Write OFF output - merge all objects */
static int write_off(FILE *out) {
    int total_vertices = 0;
    int total_faces = 0;
    
    for (int i = 0; i < num_objects; i++) {
        total_vertices += objects[i].num_vertices;
        for (int j = 0; j < objects[i].num_faces; j++) {
            if (objects[i].faces[j].num_verts >= 3) {
                total_faces++;
            }
        }
    }
    
    fprintf(out, "OFF\n");
    fprintf(out, "%d %d 0\n", total_vertices, total_faces);
    
    for (int i = 0; i < num_objects; i++) {
        Object *obj = &objects[i];
        for (int j = 0; j < obj->num_vertices; j++) {
            if (no_transform) {
                fprintf(out, "%.6f %.6f %.6f\n", 
                        obj->vertices[j].x, obj->vertices[j].y, obj->vertices[j].z);
            } else {
                Vertex transformed;
                transform_vertex(obj, &obj->vertices[j], &transformed);
                fprintf(out, "%.6f %.6f %.6f\n", 
                        transformed.x, transformed.y, transformed.z);
            }
        }
    }
    
    int vertex_offset = 0;
    for (int i = 0; i < num_objects; i++) {
        Object *obj = &objects[i];
        for (int j = 0; j < obj->num_faces; j++) {
            Face *face = &obj->faces[j];
            if (face->num_verts >= 3) {
                fprintf(out, "%d", face->num_verts);
                for (int k = 0; k < face->num_verts; k++) {
                    fprintf(out, " %d", face->verts[k] + vertex_offset);
                }
                fprintf(out, "\n");
            }
        }
        vertex_offset += obj->num_vertices;
    }
    
    return 0;
}

/* Free allocated memory */
static void cleanup(void) {
    for (int i = 0; i < num_objects; i++) {
        free(objects[i].vertices);
        free(objects[i].edges);
        free(objects[i].faces);
    }
}

int main(int argc, char **argv) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    FILE *fin = NULL;
    FILE *fout = stdout;
    int result = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--no-transform") == 0) {
            no_transform = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fprintf(stderr, "Usage: %s [-v] [-n] input.iob [output.off]\n", argv[0]);
            fprintf(stderr, "  -v, --verbose       Print debug information\n");
            fprintf(stderr, "  -n, --no-transform  Output raw coordinates without transformation\n");
            fprintf(stderr, "  -h, --help          Show this help\n");
            fprintf(stderr, "\nConverts Imagine IOB (TDDD) files to OFF format.\n");
            return 0;
        } else if (!input_file) {
            input_file = argv[i];
        } else if (!output_file) {
            output_file = argv[i];
        }
    }
    
    if (!input_file) {
        fprintf(stderr, "Usage: %s [-v] [-n] input.iob [output.off]\n", argv[0]);
        return 1;
    }
    
    fin = fopen(input_file, "rb");
    if (!fin) {
        perror(input_file);
        return 1;
    }
    
    if (output_file) {
        fout = fopen(output_file, "w");
        if (!fout) {
            perror(output_file);
            fclose(fin);
            return 1;
        }
    }
    
    if (parse_form(fin) < 0) {
        result = 1;
    } else {
        int total_vertices = 0;
        int total_faces = 0;
        for (int i = 0; i < num_objects; i++) {
            total_vertices += objects[i].num_vertices;
            for (int j = 0; j < objects[i].num_faces; j++) {
                if (objects[i].faces[j].num_verts >= 3) {
                    total_faces++;
                }
            }
        }
        
        if (verbose) {
            fprintf(stderr, "\nParsed %d objects\n", num_objects);
            for (int i = 0; i < num_objects; i++) {
                fprintf(stderr, "  Object %d: '%s' - %d vertices, %d edges, %d faces\n",
                        i, objects[i].name, objects[i].num_vertices, 
                        objects[i].num_edges, objects[i].num_faces);
            }
        }
        
        if (num_objects == 0) {
            fprintf(stderr, "Warning: No objects found in file\n");
        } else {
            fprintf(stderr, "Converted %d objects: %d vertices, %d faces\n",
                    num_objects, total_vertices, total_faces);
        }
        
        write_off(fout);
    }
    
    fclose(fin);
    if (fout != stdout) fclose(fout);
    cleanup();
    
    return result;
}

