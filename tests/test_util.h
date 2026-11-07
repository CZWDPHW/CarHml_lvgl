#ifndef CARMW_TEST_UTIL_H
#define CARMW_TEST_UTIL_H

#include <stdio.h>
#include <string.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        g_checks++;                                                            \
        if(!(cond)) {                                                          \
            g_failures++;                                                      \
            printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                      \
    } while(0)

void test_parser_suite(void);
void test_registry_suite(void);
void test_decode_suite(void);
void test_state_suite(void);
void test_session_suite(void);
void test_command_suite(void);
void test_link_suite(void);

#endif /* CARMW_TEST_UTIL_H */
