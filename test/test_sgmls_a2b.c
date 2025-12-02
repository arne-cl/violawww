/*
 * Tests for sgmlsA2B.c - SGMLS ASCII to Binary converter
 * 
 * Tests internal functions:
 * - filterCtrl: escape sequence processing
 * - emitInt/emitToken/emitStr: binary output formatting
 * - findTagID: tag dictionary lookup
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Redirect main() to avoid conflict */
#define main sgmls_main

/* Include the source file to access static functions */
#include "../src/viola/sgmlsA2B.c"

#undef main

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", msg); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) != (b)) { \
        printf("FAIL: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", msg); \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    tests_run++; \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL: %s (got '%s', expected '%s')\n", msg, (a), (b)); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", msg); \
        tests_passed++; \
    } \
} while (0)

/* ============================================
 * Test filterCtrl - escape sequence processing
 * ============================================ */
static int test_filterCtrl_simple() {
    printf("\n--- Test: filterCtrl simple strings ---\n");
    
    int size;
    char* result;
    
    /* Simple string without escapes */
    result = filterCtrl("hello", &size);
    ASSERT(result != NULL, "filterCtrl returns non-NULL");
    ASSERT_STR_EQ(result, "hello", "Simple string unchanged");
    ASSERT_EQ(size, 5, "Size is correct");
    free(result);
    
    /* Empty string */
    result = filterCtrl("", &size);
    ASSERT(result != NULL, "filterCtrl handles empty string");
    ASSERT_STR_EQ(result, "", "Empty string returned");
    ASSERT_EQ(size, 0, "Size is 0");
    free(result);
    
    return 1;
}

static int test_filterCtrl_escapes() {
    printf("\n--- Test: filterCtrl escape sequences ---\n");
    
    int size;
    char* result;
    
    /* Newline */
    result = filterCtrl("hello\\nworld", &size);
    ASSERT(result != NULL, "filterCtrl handles \\n");
    ASSERT(result[5] == '\n', "\\n converted to newline");
    ASSERT_EQ(size, 11, "Size correct after \\n conversion");
    free(result);
    
    /* Tab */
    result = filterCtrl("a\\tb", &size);
    ASSERT(result != NULL, "filterCtrl handles \\t");
    ASSERT(result[1] == '\t', "\\t converted to tab");
    ASSERT_EQ(size, 3, "Size correct after \\t conversion");
    free(result);
    
    /* Carriage return */
    result = filterCtrl("a\\rb", &size);
    ASSERT(result != NULL, "filterCtrl handles \\r");
    ASSERT(result[1] == '\r', "\\r converted to carriage return");
    free(result);
    
    /* Backslash */
    result = filterCtrl("a\\\\b", &size);
    ASSERT(result != NULL, "filterCtrl handles \\\\");
    ASSERT(result[1] == '\\', "\\\\ converted to single backslash");
    ASSERT_EQ(size, 3, "Size correct after \\\\ conversion");
    free(result);
    
    /* Quote */
    result = filterCtrl("a\\\"b", &size);
    ASSERT(result != NULL, "filterCtrl handles \\\"");
    ASSERT(result[1] == '"', "\\\" converted to quote");
    free(result);
    
    /* Single quote */
    result = filterCtrl("a\\'b", &size);
    ASSERT(result != NULL, "filterCtrl handles \\'");
    ASSERT(result[1] == '\'', "\\' converted to single quote");
    free(result);
    
    /* Form feed */
    result = filterCtrl("a\\fb", &size);
    ASSERT(result != NULL, "filterCtrl handles \\f");
    ASSERT(result[1] == '\f', "\\f converted to form feed");
    free(result);
    
    /* Backspace */
    result = filterCtrl("a\\bb", &size);
    ASSERT(result != NULL, "filterCtrl handles \\b");
    ASSERT(result[1] == '\b', "\\b converted to backspace");
    free(result);
    
    return 1;
}

static int test_filterCtrl_octal() {
    printf("\n--- Test: filterCtrl octal escapes ---\n");
    
    int size;
    char* result;
    
    /* Octal 011 = tab (9) */
    result = filterCtrl("a\\011b", &size);
    ASSERT(result != NULL, "filterCtrl handles octal");
    ASSERT(result[1] == '\t', "\\011 converted to tab");
    /* Note: octal conversion adds 2 extra spaces in current implementation */
    free(result);
    
    /* Octal 040 = space (32) */
    result = filterCtrl("a\\040b", &size);
    ASSERT(result != NULL, "filterCtrl handles octal space");
    ASSERT(result[1] == ' ', "\\040 converted to space");
    free(result);
    
    return 1;
}

static int test_filterCtrl_unknown_escape() {
    printf("\n--- Test: filterCtrl unknown escapes ---\n");
    
    int size;
    char* result;
    
    /* Unknown escape - should pass through as backslash + char */
    result = filterCtrl("a\\xb", &size);
    ASSERT(result != NULL, "filterCtrl handles unknown escape");
    ASSERT(result[1] == '\\', "Unknown escape preserves backslash");
    ASSERT(result[2] == 'x', "Unknown escape preserves character");
    free(result);
    
    return 1;
}

/* ============================================
 * Test emitInt - binary integer output
 * Using tmpfile instead of pipes for reliability
 * ============================================ */
static int test_emitInt_format() {
    printf("\n--- Test: emitInt binary format ---\n");
    
    /* Use tmpfile for reliable output capture */
    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("SKIP: Could not create temp file\n");
        return 1;
    }
    
    /* Save and redirect stdout */
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    
    /* Emit a known value: 0x12345678 */
    emitInt(0x12345678);
    fflush(stdout);
    
    /* Restore stdout */
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    /* Read from temp file */
    rewind(tmp);
    unsigned char buf[4];
    size_t n = fread(buf, 1, 4, tmp);
    fclose(tmp);
    
    ASSERT_EQ((int)n, 4, "emitInt writes 4 bytes");
    ASSERT_EQ(buf[0], 0x12, "emitInt byte 0 (MSB) correct");
    ASSERT_EQ(buf[1], 0x34, "emitInt byte 1 correct");
    ASSERT_EQ(buf[2], 0x56, "emitInt byte 2 correct");
    ASSERT_EQ(buf[3], 0x78, "emitInt byte 3 (LSB) correct");
    
    return 1;
}

static int test_emitInt_zero() {
    printf("\n--- Test: emitInt zero value ---\n");
    
    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("SKIP: Could not create temp file\n");
        return 1;
    }
    
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    
    emitInt(0);
    fflush(stdout);
    
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    rewind(tmp);
    unsigned char buf[4];
    size_t n = fread(buf, 1, 4, tmp);
    fclose(tmp);
    
    ASSERT_EQ((int)n, 4, "emitInt(0) writes 4 bytes");
    ASSERT_EQ(buf[0], 0, "Zero byte 0");
    ASSERT_EQ(buf[1], 0, "Zero byte 1");
    ASSERT_EQ(buf[2], 0, "Zero byte 2");
    ASSERT_EQ(buf[3], 0, "Zero byte 3");
    
    return 1;
}

/* ============================================
 * Test emitToken - single byte output
 * ============================================ */
static int test_emitToken() {
    printf("\n--- Test: emitToken ---\n");
    
    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("SKIP: Could not create temp file\n");
        return 1;
    }
    
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    
    emitToken(TOKEN_TAG);
    emitToken(TOKEN_END);
    emitToken(TOKEN_DATA);
    fflush(stdout);
    
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    rewind(tmp);
    unsigned char buf[3];
    size_t n = fread(buf, 1, 3, tmp);
    fclose(tmp);
    
    ASSERT_EQ((int)n, 3, "emitToken writes 1 byte each");
    ASSERT_EQ(buf[0], TOKEN_TAG, "TOKEN_TAG value correct");
    ASSERT_EQ(buf[1], TOKEN_END, "TOKEN_END value correct");
    ASSERT_EQ(buf[2], TOKEN_DATA, "TOKEN_DATA value correct");
    
    return 1;
}

static int test_emitToken_mask() {
    printf("\n--- Test: emitToken masks to byte ---\n");
    
    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("SKIP: Could not create temp file\n");
        return 1;
    }
    
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    
    /* Value > 255 should be masked */
    emitToken(0x1234);
    fflush(stdout);
    
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    rewind(tmp);
    unsigned char buf[1];
    size_t n = fread(buf, 1, 1, tmp);
    fclose(tmp);
    
    ASSERT_EQ((int)n, 1, "emitToken writes 1 byte");
    ASSERT_EQ(buf[0], 0x34, "Value masked to lower byte");
    
    return 1;
}

/* ============================================
 * Test emitStr - string output
 * ============================================ */
static int test_emitStr() {
    printf("\n--- Test: emitStr ---\n");
    
    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("SKIP: Could not create temp file\n");
        return 1;
    }
    
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    
    emitStr("hello", 5);
    fflush(stdout);
    
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    rewind(tmp);
    char buf[6];
    size_t n = fread(buf, 1, 5, tmp);
    fclose(tmp);
    buf[n] = '\0';
    
    ASSERT_EQ((int)n, 5, "emitStr writes correct length");
    ASSERT_STR_EQ(buf, "hello", "emitStr content correct");
    
    return 1;
}

static int test_emitStr_partial() {
    printf("\n--- Test: emitStr partial string ---\n");
    
    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("SKIP: Could not create temp file\n");
        return 1;
    }
    
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);
    
    /* Only emit first 3 chars of "hello" */
    emitStr("hello", 3);
    fflush(stdout);
    
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    
    rewind(tmp);
    char buf[4];
    size_t n = fread(buf, 1, 3, tmp);
    fclose(tmp);
    buf[n] = '\0';
    
    ASSERT_EQ((int)n, 3, "emitStr writes specified length");
    ASSERT_STR_EQ(buf, "hel", "emitStr partial content correct");
    
    return 1;
}

/* ============================================
 * Test findTagID - tag dictionary lookup
 * ============================================ */
static int test_findTagID() {
    printf("\n--- Test: findTagID ---\n");
    
    /* Reset tag dictionary */
    tagDictCount = 0;
    
    /* Add some tags */
    tagDict[0] = "HTML";
    tagDict[1] = "BODY";
    tagDict[2] = "P";
    tagDictCount = 3;
    
    ASSERT_EQ(findTagID("HTML"), 0, "Find HTML at index 0");
    ASSERT_EQ(findTagID("BODY"), 1, "Find BODY at index 1");
    ASSERT_EQ(findTagID("P"), 2, "Find P at index 2");
    ASSERT_EQ(findTagID("UNKNOWN"), -1, "Unknown tag returns -1");
    ASSERT_EQ(findTagID(""), -1, "Empty tag returns -1");
    
    /* Reset for other tests */
    tagDictCount = 0;
    
    return 1;
}

/* ============================================
 * Test attribute type parsing
 * ============================================ */
static int test_attr_types() {
    printf("\n--- Test: attribute type enum values ---\n");
    
    ASSERT_EQ(SGMLS_ATTR_IMPLIED, 0, "ATTR_IMPLIED is 0");
    ASSERT_EQ(SGMLS_ATTR_CDATA, 1, "ATTR_CDATA is 1");
    ASSERT_EQ(SGMLS_ATTR_TOKEN, 2, "ATTR_TOKEN is 2");
    ASSERT_EQ(SGMLS_ATTR_ID, 3, "ATTR_ID is 3");
    ASSERT_EQ(SGMLS_ATTR_IDREF, 4, "ATTR_IDREF is 4");
    ASSERT_EQ(SGMLS_ATTR_ENTITY, 5, "ATTR_ENTITY is 5");
    ASSERT_EQ(SGMLS_ATTR_NOTATION, 6, "ATTR_NOTATION is 6");
    
    return 1;
}

/* ============================================
 * Test token constants
 * ============================================ */
static int test_token_constants() {
    printf("\n--- Test: token constants ---\n");
    
    ASSERT_EQ(TOKEN_TAGDICT, 1, "TOKEN_TAGDICT is 1");
    ASSERT_EQ(TOKEN_TAG, 2, "TOKEN_TAG is 2");
    ASSERT_EQ(TOKEN_END, 3, "TOKEN_END is 3");
    ASSERT_EQ(TOKEN_ATTR, 4, "TOKEN_ATTR is 4");
    ASSERT_EQ(TOKEN_DATA, 5, "TOKEN_DATA is 5");
    
    return 1;
}

/* ============================================
 * Main test runner
 * ============================================ */
int main(int argc, char** argv) {
    printf("=======================================\n");
    printf("sgmlsA2B.c Unit Tests\n");
    printf("=======================================\n");
    
    /* filterCtrl tests */
    test_filterCtrl_simple();
    test_filterCtrl_escapes();
    test_filterCtrl_octal();
    test_filterCtrl_unknown_escape();
    
    /* Binary output tests */
    test_emitInt_format();
    test_emitInt_zero();
    test_emitToken();
    test_emitToken_mask();
    test_emitStr();
    test_emitStr_partial();
    
    /* Tag dictionary tests */
    test_findTagID();
    
    /* Constants tests */
    test_attr_types();
    test_token_constants();
    
    printf("\n=======================================\n");
    printf("Test Results: %d/%d passed, %d failed\n", 
           tests_passed, tests_run, tests_failed);
    printf("=======================================\n");
    
    if (tests_failed == 0) {
        printf("OVERALL: SUCCESS\n");
        return 0;
    } else {
        printf("OVERALL: FAILURE\n");
        return 1;
    }
}

