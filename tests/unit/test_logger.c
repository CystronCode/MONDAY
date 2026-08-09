// tests/unit/test_logger.c
#include "unity.h"
#include "monday/core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define TEST_LOG_PATH "test_monday.log"

void setUp(void) {
    remove(TEST_LOG_PATH);
    char rotated[256];
    for (int i = 1; i <= 10; ++i) {
        snprintf(rotated, sizeof(rotated), "%s.%d", TEST_LOG_PATH, i);
        remove(rotated);
    }
}

void tearDown(void) {
    monday_log_close();
    remove(TEST_LOG_PATH);
    char rotated[256];
    for (int i = 1; i <= 10; ++i) {
        snprintf(rotated, sizeof(rotated), "%s.%d", TEST_LOG_PATH, i);
        remove(rotated);
    }
}

void test_log_json_format(void) {
    TEST_ASSERT_EQUAL_INT(0, monday_log_init(TEST_LOG_PATH, 1024 * 1024, 3));

    for (int i = 0; i < 100; ++i) {
        monday_log_write(MONDAY_LOG_DIAGNOSTIC, "diagnostic event test");
    }
    monday_log_close();

    FILE *f = fopen(TEST_LOG_PATH, "r");
    TEST_ASSERT_NOT_NULL(f);

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        count++;
        TEST_ASSERT_EQUAL_CHAR('{', line[0]);
        TEST_ASSERT_NOT_NULL(strstr(line, "\"level\":\"DIAGNOSTIC\""));
        TEST_ASSERT_NOT_NULL(strstr(line, "\"msg\":\"diagnostic event test\""));
        TEST_ASSERT_EQUAL_CHAR('\n', line[strlen(line) - 1]);
    }
    fclose(f);
    TEST_ASSERT_EQUAL_INT(100, count);
}

void test_log_rotation(void) {
    // Set max size small enough (500 bytes) so 100-byte lines trigger rotation quickly
    TEST_ASSERT_EQUAL_INT(0, monday_log_init(TEST_LOG_PATH, 500, 3));

    for (int i = 0; i < 30; ++i) {
        monday_log_write(MONDAY_LOG_SYSTEM, "this is a slightly longer log message to trigger log file rotation");
    }
    monday_log_close();

    // Check that rotated backup file .1 exists
    char rotated1[256];
    snprintf(rotated1, sizeof(rotated1), "%s.1", TEST_LOG_PATH);
    FILE *f_rotated = fopen(rotated1, "r");
    TEST_ASSERT_NOT_NULL(f_rotated);
    fclose(f_rotated);

    FILE *f_live = fopen(TEST_LOG_PATH, "r");
    TEST_ASSERT_NOT_NULL(f_live);
    fclose(f_live);
}

void test_log_sensitive_level(void) {
    TEST_ASSERT_EQUAL_INT(0, monday_log_init(TEST_LOG_PATH, 1024 * 1024, 3));
    monday_log_write(MONDAY_LOG_SENSITIVE, "sensitive credentials user_token_123");
    monday_log_close();

    FILE *f = fopen(TEST_LOG_PATH, "r");
    TEST_ASSERT_NOT_NULL(f);

    char line[1024];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), f));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"level\":\"SENSITIVE\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"msg\":\"sensitive credentials user_token_123\""));
    fclose(f);
}

static void *thread_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < 1000; ++i) {
        monday_log_write_kv_long(MONDAY_LOG_VERBOSE, "concurrent log entry", "thread_iter", (long)i);
    }
    return NULL;
}

void test_log_concurrency(void) {
    TEST_ASSERT_EQUAL_INT(0, monday_log_init(TEST_LOG_PATH, 10 * 1024 * 1024, 3));

    pthread_t threads[4];
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_INT(0, pthread_create(&threads[i], NULL, thread_worker, NULL));
    }

    for (int i = 0; i < 4; ++i) {
        pthread_join(threads[i], NULL);
    }
    monday_log_close();

    FILE *f = fopen(TEST_LOG_PATH, "r");
    TEST_ASSERT_NOT_NULL(f);

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        count++;
        // Verify JSON boundaries and uncorrupted structure per line
        TEST_ASSERT_EQUAL_CHAR('{', line[0]);
        size_t len = strlen(line);
        TEST_ASSERT_EQUAL_CHAR('\n', line[len - 1]);
        TEST_ASSERT_EQUAL_CHAR('}', line[len - 2]);
        TEST_ASSERT_NOT_NULL(strstr(line, "\"level\":\"VERBOSE\""));
    }
    fclose(f);
    TEST_ASSERT_EQUAL_INT(4000, count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_log_json_format);
    RUN_TEST(test_log_rotation);
    RUN_TEST(test_log_sensitive_level);
    RUN_TEST(test_log_concurrency);
    return UNITY_END();
}
