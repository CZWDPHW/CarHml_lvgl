#include "test_util.h"

int g_checks = 0;
int g_failures = 0;

static void run_suite(const char *name, void (*suite)(void))
{
    int before = g_failures;

    suite();
    printf("[%s] %s\n", g_failures == before ? "PASS" : "FAIL", name);
}

int main(void)
{
    printf("carmw test suite\n\n");

    run_suite("protocol/parser", test_parser_suite);
    run_suite("registry", test_registry_suite);
    run_suite("decode", test_decode_suite);
    run_suite("state", test_state_suite);
    run_suite("session", test_session_suite);
    run_suite("command", test_command_suite);
    run_suite("link (memory port)", test_link_suite);

    printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);

    return g_failures == 0 ? 0 : 1;
}
