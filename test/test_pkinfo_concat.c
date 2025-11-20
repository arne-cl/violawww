#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/viola/misc.h"
#include "../src/viola/mystrings.h"

/* -------------------------------------------------------------------------
 * Stubs for symbols that misc.c depends on but that are irrelevant for
 * the scope of these tests. They allow us to link against the production
 * object file without pulling in the entire application.
 * ------------------------------------------------------------------------- */
HashTable* objID2Obj = NULL;

VObjList* appendObjToList(VObjList* olist, VObj* obj)
{
    (void)olist;
    (void)obj;
    return NULL;
}

VObj* findObject(long objNameStrID)
{
    (void)objNameStrID;
    return NULL;
}

long getIdent(char* identStr)
{
    (void)identStr;
    return 0;
}

long storeIdent(char* identStr)
{
    (void)identStr;
    return 0;
}

int load_object(char* filename, char* pathname)
{
    (void)filename;
    (void)pathname;
    return -1;
}

/* ------------------------------------------------------------------------- */

static void packet_from_str(Packet* pkt, const char* str)
{
    pkt->type = PKT_STR;
    pkt->info.s = (char*)str;
}

static void packet_from_chr(Packet* pkt, char c)
{
    pkt->type = PKT_CHR;
    pkt->info.c = c;
}

static void packet_from_int(Packet* pkt, long value)
{
    pkt->type = PKT_INT;
    pkt->info.i = value;
}

static void packet_from_flt(Packet* pkt, float value)
{
    pkt->type = PKT_FLT;
    pkt->info.f = value;
}

static void packet_from_obj(Packet* pkt, VObj* obj)
{
    pkt->type = PKT_OBJ;
    pkt->info.o = obj;
}

static void packet_from_array(Packet* pkt, Array* array)
{
    pkt->type = PKT_ARY;
    pkt->info.y = array;
}

static sigjmp_buf overflow_jmp;
static volatile sig_atomic_t overflow_signal = 0;
static const int guarded_signals[] = {SIGSEGV, SIGBUS, SIGABRT, SIGTRAP};

static void overflow_signal_handler(int signum)
{
    overflow_signal = signum;
    siglongjmp(overflow_jmp, 1);
}

static void install_signal_handlers(struct sigaction* old_handlers, size_t count)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = overflow_signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    for (size_t i = 0; i < count; i++) {
        sigaction(guarded_signals[i], &act, &old_handlers[i]);
    }
}

static void restore_signal_handlers(struct sigaction* old_handlers, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        sigaction(guarded_signals[i], &old_handlers[i], NULL);
    }
}

static void test_basic_type_concatenation(void)
{
    Packet argv[4];
    packet_from_str(&argv[0], "Hello");
    packet_from_chr(&argv[1], ' ');
    packet_from_int(&argv[2], 42);
    packet_from_flt(&argv[3], 3.5f);

    const char* result = PkInfos2Str(4, argv);
    assert(result != NULL);
    assert(strcmp(result, "Hello 423.500000") == 0);
}

static char* make_repeated_block(char ch, size_t length)
{
    char* buf = malloc(length + 1);
    if (!buf) {
        fprintf(stderr, "allocation failed\n");
        exit(1);
    }
    memset(buf, ch, length);
    buf[length] = '\0';
    return buf;
}

static void test_pkt_obj_null(void)
{
    Packet argv[1];
    packet_from_obj(&argv[0], NULL);
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strcmp(result, "(NULL)") == 0);
}

static void test_pkt_obj_valid(void)
{
    /* Mock VObj: array of longs. 
     * GET_name(o) is defined as ((char*)((o)[2])) in slotaccess.h 
     */
    long mock_obj[10];
    char* obj_name = "MyMockObject";
    mock_obj[2] = (long)obj_name;

    Packet argv[1];
    packet_from_obj(&argv[0], (VObj*)mock_obj);

    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strcmp(result, "MyMockObject") == 0);
}

static void test_pkt_array_empty(void)
{
    Array array = {0, NULL};
    Packet argv[1];
    packet_from_array(&argv[0], &array);
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strlen(result) == 0);
}

static void test_pkt_array_single(void)
{
    long vals[] = {42};
    Array array = {1, vals};
    Packet argv[1];
    packet_from_array(&argv[0], &array);
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strcmp(result, "42") == 0);
}

static void test_pkt_array_multiple(void)
{
    long vals[] = {10, 20, 30};
    Array array = {3, vals};
    Packet argv[1];
    packet_from_array(&argv[0], &array);
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strcmp(result, "10 20 30") == 0);
}

static void test_pkt_array_large(void)
{
    /* Test array with 1000 elements to verify no overflow */
    long* vals = malloc(1000 * sizeof(long));
    assert(vals != NULL);
    for (int i = 0; i < 1000; i++) {
        vals[i] = i;
    }
    
    Array array = {1000, vals};
    Packet argv[1];
    packet_from_array(&argv[0], &array);
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    
    /* Verify first few elements */
    assert(strncmp(result, "0 1 2 3 4 5", 11) == 0);
    
    /* Verify last element is present (no truncation) */
    assert(strstr(result, "999") != NULL);
    
    free(vals);
}

static void test_mixed_types_with_arrays(void)
{
    long vals[] = {1, 2, 3};
    Array array = {3, vals};
    
    Packet argv[5];
    packet_from_str(&argv[0], "Array:");
    packet_from_array(&argv[1], &array);
    packet_from_str(&argv[2], " Count:");
    packet_from_int(&argv[3], array.size);
    packet_from_obj(&argv[4], NULL);
    
    const char* result = PkInfos2Str(5, argv);
    assert(result != NULL);
    assert(strcmp(result, "Array:1 2 3 Count:3(NULL)") == 0);
}

static void test_many_small_strings(void)
{
    /* Test concatenating many small strings (buffer growth) */
    Packet argv[100];
    for (int i = 0; i < 100; i++) {
        packet_from_str(&argv[i], "X");
    }
    
    const char* result = PkInfos2Str(100, argv);
    assert(result != NULL);
    assert(strlen(result) == 100);
    
    /* Verify all 'X's are present */
    for (size_t i = 0; i < 100; i++) {
        assert(result[i] == 'X');
    }
}

static void test_empty_strings(void)
{
    /* Test with empty string packets */
    Packet argv[3];
    packet_from_str(&argv[0], "Start");
    packet_from_str(&argv[1], "");
    packet_from_str(&argv[2], "End");
    
    const char* result = PkInfos2Str(3, argv);
    assert(result != NULL);
    assert(strcmp(result, "StartEnd") == 0);
}

static void test_null_string_packet(void)
{
    /* Test PKT_STR with NULL pointer */
    Packet argv[3];
    packet_from_str(&argv[0], "Before");
    argv[1].type = PKT_STR;
    argv[1].info.s = NULL;
    packet_from_str(&argv[2], "After");
    
    const char* result = PkInfos2Str(3, argv);
    assert(result != NULL);
    /* NULL string should be skipped */
    assert(strcmp(result, "BeforeAfter") == 0);
}

static void test_zero_packets(void)
{
    /* Edge case: zero packets */
    const char* result = PkInfos2Str(0, NULL);
    assert(result != NULL);
    assert(strlen(result) == 0);
}

static void test_unknown_packet_type(void)
{
    /* Test with unknown/invalid packet type */
    Packet argv[3];
    packet_from_str(&argv[0], "Before");
    argv[1].type = 99; /* Invalid type */
    packet_from_str(&argv[2], "After");
    
    const char* result = PkInfos2Str(3, argv);
    assert(result != NULL);
    assert(strcmp(result, "Before?After") == 0);
}

static void test_packet_pkt_handling(void)
{
    /* PKT_PKT (5) is not explicitly handled and should fall through to default ('?') */
    Packet argv[1];
    argv[0].type = PKT_PKT;
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strcmp(result, "?") == 0);
}

static void test_negative_int(void)
{
    /* Test negative integers */
    Packet argv[3];
    packet_from_int(&argv[0], -12345);
    packet_from_str(&argv[1], " ");
    packet_from_int(&argv[2], -9999);
    
    const char* result = PkInfos2Str(3, argv);
    assert(result != NULL);
    assert(strcmp(result, "-12345 -9999") == 0);
}

static void test_special_float_values(void)
{
    /* Test special float values */
    Packet argv[3];
    packet_from_flt(&argv[0], 0.0f);
    packet_from_flt(&argv[1], -0.0f);
    packet_from_flt(&argv[2], 123456.789f);
    
    const char* result = PkInfos2Str(3, argv);
    assert(result != NULL);
    /* Simple check that they appear in the string */
    assert(strstr(result, "0.000000") != NULL);
    assert(strstr(result, "123456.7") != NULL);
}

static void test_array_with_negative_values(void)
{
    long vals[] = {-100, -50, 0, 50, 100};
    Array array = {5, vals};
    Packet argv[1];
    packet_from_array(&argv[0], &array);
    
    const char* result = PkInfos2Str(1, argv);
    assert(result != NULL);
    assert(strcmp(result, "-100 -50 0 50 100") == 0);
}

static void test_large_concatenation_exceeds_buff_size(void)
{
    const size_t chunk = BUFF_SIZE;
    const size_t signal_count = sizeof(guarded_signals) / sizeof(guarded_signals[0]);
    struct sigaction old_handlers[sizeof(guarded_signals) / sizeof(guarded_signals[0])];

    overflow_signal = 0;
    if (sigsetjmp(overflow_jmp, 1) != 0) {
        restore_signal_handlers(old_handlers, signal_count);
        fprintf(stderr,
            "ERROR: Received signal %d while concatenating packets larger than BUFF_SIZE\n",
            overflow_signal);
        assert(false && "PkInfos2Str must not crash on large concatenations");
    }

    install_signal_handlers(old_handlers, signal_count);

    char* block_a = make_repeated_block('A', chunk);
    char* block_b = make_repeated_block('B', chunk);

    Packet argv[2];
    packet_from_str(&argv[0], block_a);
    packet_from_str(&argv[1], block_b);

    const char* result = PkInfos2Str(2, argv);
    restore_signal_handlers(old_handlers, signal_count);
    assert(result != NULL);

    const size_t expected_len = (chunk * 2);
    if (strlen(result) != expected_len) {
        fprintf(stderr,
            "ERROR: Expected concatenated length %zu but got %zu "
            "(suggests fixed-size buffer limitation)\n",
            expected_len,
            strlen(result));
        assert(false && "PkInfos2Str should handle data larger than BUFF_SIZE");
    }

    /* Verify content matches the two concatenated blocks. */
    for (size_t i = 0; i < expected_len; i++) {
        char expected = (i < chunk) ? 'A' : 'B';
        if (result[i] != expected) {
            fprintf(stderr, "ERROR: Mismatch at index %zu\n", i);
            assert(false && "PkInfos2Str output mismatch");
        }
    }

    free(block_a);
    free(block_b);
}

static void test_pointer_stability_on_growth(void)
{
    cleanup_pkinfo_concat();

    Packet argv_small[1];
    packet_from_str(&argv_small[0], "seed");

    const char* first = PkInfos2Str(1, argv_small);
    assert(first != NULL);

    const size_t chunk = BUFF_SIZE * 2;
    char* block = make_repeated_block('Z', chunk);
    Packet argv_large[2];
    packet_from_str(&argv_large[0], block);
    packet_from_str(&argv_large[1], block);

    const char* second = PkInfos2Str(2, argv_large);
    assert(second != NULL);
    assert(strlen(second) == chunk * 2);

    const size_t signal_count = sizeof(guarded_signals) / sizeof(guarded_signals[0]);
    struct sigaction old_handlers[signal_count];
    overflow_signal = 0;
    if (sigsetjmp(overflow_jmp, 1) != 0) {
        restore_signal_handlers(old_handlers, signal_count);
        assert(false && "Dereferencing stale pkinfo buffer must not crash");
    }

    install_signal_handlers(old_handlers, signal_count);
    volatile size_t len = strlen(first);
    (void)len;
    restore_signal_handlers(old_handlers, signal_count);

    assert(first[0] == '\0');

    free(block);
}

int main(void)
{
    printf("Running PkInfos2Str tests...\n");
    
    printf("  [1/20] Basic type concatenation...\n");
    test_basic_type_concatenation();
    
    printf("  [2/20] PKT_OBJ with NULL...\n");
    test_pkt_obj_null();

    printf("  [3/20] PKT_OBJ with valid object (mocked)...\n");
    test_pkt_obj_valid();
    
    printf("  [4/20] PKT_ARY empty array...\n");
    test_pkt_array_empty();
    
    printf("  [5/20] PKT_ARY single element...\n");
    test_pkt_array_single();
    
    printf("  [6/20] PKT_ARY multiple elements...\n");
    test_pkt_array_multiple();
    
    printf("  [7/20] PKT_ARY large (1000 elements)...\n");
    test_pkt_array_large();
    
    printf("  [8/20] Mixed types with arrays...\n");
    test_mixed_types_with_arrays();
    
    printf("  [9/20] Many small strings (100x)...\n");
    test_many_small_strings();
    
    printf("  [10/20] Empty strings...\n");
    test_empty_strings();
    
    printf("  [11/20] NULL string pointer...\n");
    test_null_string_packet();
    
    printf("  [12/20] Zero packets edge case...\n");
    test_zero_packets();
    
    printf("  [13/20] Unknown packet type...\n");
    test_unknown_packet_type();

    printf("  [14/20] PKT_PKT recursive packet handling...\n");
    test_packet_pkt_handling();
    
    printf("  [15/20] Negative integers...\n");
    test_negative_int();
    
    printf("  [16/20] Special float values...\n");
    test_special_float_values();
    
    printf("  [17/20] Array with negative values...\n");
    test_array_with_negative_values();
    
    printf("  [18/20] Large concatenation (2xBUFF_SIZE)...\n");
    test_large_concatenation_exceeds_buff_size();
    
    printf("  [19/20] Pointer stability after buffer growth...\n");
    test_pointer_stability_on_growth();
    
    printf("  [20/20] Cleanup test...\n");
    cleanup_pkinfo_concat();
    assert(1); /* Just verify function exists and doesn't crash */
    
    printf("\n✓ All 20 PkInfos2Str tests passed\n");
    return 0;
}
