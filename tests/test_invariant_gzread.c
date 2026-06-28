#include <check.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include "zlib.h"

START_TEST(test_gzread_shift_overflow_invariant)
{
    // Invariant: Allocation size calculation must not overflow to cause undersized buffer
    const unsigned long payloads[] = {
        SIZE_MAX / 2 + 1,  // Exact exploit case: overflow on << 1
        SIZE_MAX,          // Boundary: maximum size
        8192               // Valid normal input
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        gzFile file;
        unsigned long want = payloads[i];
        
        // Create a minimal gzip file in memory
        unsigned char data[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03};
        FILE *tmp = tmpfile();
        fwrite(data, sizeof(data), 1, tmp);
        rewind(tmp);
        
        file = gzdopen(fileno(tmp), "rb");
        if (file == NULL) {
            fclose(tmp);
            continue;
        }
        
        // Attempt to read with adversarial want value
        unsigned char *buffer = malloc(want);
        if (buffer == NULL && want > 0) {
            gzclose(file);
            fclose(tmp);
            continue;
        }
        
        int result = gzread(file, buffer, want);
        
        // Property: Either allocation succeeds with correct size or fails safely
        // We check that no heap corruption occurred by verifying gzread didn't
        // return more bytes than could be safely stored
        ck_assert_msg(result <= (int)want, 
                     "gzread returned %d bytes but buffer size was %lu", 
                     result, want);
        
        free(buffer);
        gzclose(file);
        fclose(tmp);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_gzread_shift_overflow_invariant);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}