/*
 *                          Copyright (C) 2019 by Rafael Santiago
 *
 * Use of this source code is governed by GPL-v2 license that can
 * be found in the COPYING file.
 *
 */
#include <cutest.h>
#include <mnemosine.h>
#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
# include <stdlib.h>
# include <time.h>
# include <pthread.h>
#endif
#if defined(_WIN32)
# include <time.h>
# include <windows.h>
#endif

#if (defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)) || defined(_WIN32)
struct async_test_ctx {
    char *id;
    int secs2snooze;
#if !defined(_WIN32)
    pthread_t t;
#else
    HANDLE t;
#endif
    struct mnemosine_ctx *mn;
    void *ptr;
    size_t ptr_size;
    int retval;
};
#endif

CUTE_DECLARE_TEST_CASE(mnemosine_tests);
CUTE_DECLARE_TEST_CASE(mnemosine_size_macros_tests);
CUTE_DECLARE_TEST_CASE(mnemosine_malloc_free_tests);
#if (defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)) || defined(_WIN32)
CUTE_DECLARE_TEST_CASE(mnemosine_malloc_free_async_tests);

# if !defined(_WIN32)
void *get_memory(void *arg);
void *free_memory(void *arg);
#else
DWORD WINAPI get_memory(void *arg);
DWORD WINAPI free_memory(void *arg);
# endif
#endif

CUTE_TEST_CASE(mnemosine_tests)
    CUTE_RUN_TEST(mnemosine_size_macros_tests);
    CUTE_RUN_TEST(mnemosine_malloc_free_tests);
#if (defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)) || defined(_WIN32)
    CUTE_RUN_TEST(mnemosine_malloc_free_async_tests);
#endif
CUTE_TEST_CASE_END

CUTE_TEST_CASE(mnemosine_malloc_free_tests)
    struct mnemosine_ctx mn;
    void *ptr[4];

    CUTE_ASSERT(mnemosine_init(&mn, mnemosine_size_kb(1), 0) == 1);

    ptr[0] = mnemosine_malloc(&mn, 512);
    CUTE_ASSERT(ptr[0] != NULL);

    ptr[1] = mnemosine_malloc(&mn, 256);
    CUTE_ASSERT(ptr[1] != NULL);

    ptr[2] = mnemosine_malloc(&mn, 512);
    CUTE_ASSERT(ptr[2] == NULL);

    ptr[2] = mnemosine_malloc(&mn, 256);
    CUTE_ASSERT(ptr[2] != NULL);

    ptr[3] = malloc(512);
    CUTE_ASSERT(ptr[3] != NULL);

    CUTE_ASSERT(mnemosine_free(&mn, ptr[0]) == 1);
    CUTE_ASSERT(mnemosine_free(&mn, ptr[2]) == 1);
    CUTE_ASSERT(mnemosine_free(&mn, ptr[1]) == 1);
    CUTE_ASSERT(mnemosine_free(&mn, ptr[3]) == 0);
    free(ptr[3]);

    mnemosine_finis(&mn);
CUTE_TEST_CASE_END

#if (defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)) || defined(_WIN32)
#if !defined(_WIN32)
void *get_memory(void *arg) {
    struct async_test_ctx *a = (struct async_test_ctx *)arg;
    printf("\t[get_memory] %s has join.\n", a->id);
    sleep(a->secs2snooze);
    a->ptr = mnemosine_malloc(a->mn, a->ptr_size);
    printf("\t[get_memory] %s has left.\n", a->id);
    return NULL;
}

void *free_memory(void *arg) {
    struct async_test_ctx *a = (struct async_test_ctx *)arg;
    printf("\t[free_memory] %s has join.\n", a->id);
    sleep(a->secs2snooze);
    a->retval = mnemosine_free(a->mn, a->ptr);
    printf("\t[free_memory] %s has left.\n", a->id);
    return NULL;
}
# else
DWORD WINAPI get_memory(void *arg) {
    struct async_test_ctx *a = (struct async_test_ctx *)arg;
    printf("\t[get_memory] %s has join.\n", a->id);
    sleep(a->secs2snooze);
    a->ptr = mnemosine_malloc(a->mn, a->ptr_size);
    printf("\t[get_memory] %s has left.\n", a->id);
    fflush(stdout);
    return 0;
}

DWORD WINAPI free_memory(void *arg) {
    struct async_test_ctx *a = (struct async_test_ctx *)arg;
    printf("\t[free_memory] %s has join.\n", a->id);
    sleep(a->secs2snooze);
    a->retval = mnemosine_free(a->mn, a->ptr);
    printf("\t[free_memory] %s has left.\n", a->id);
    fflush(stdout);
    return 0;
}
# endif

CUTE_TEST_CASE(mnemosine_malloc_free_async_tests)
#if !defined(_WIN32)
    // WARN(Rafael): Working but leaking pthread resources even with detached threads. Find a way of solving it.
    struct mnemosine_ctx mn;
    struct async_test_ctx thread[3];
    int smelly_hack = g_cute_leak_check;

    srand(time(0));

    CUTE_ASSERT(mnemosine_init(&mn, mnemosine_size_kb(1), 0) == 1);

    g_cute_leak_check = 0;

    thread[0].id = "thread 0";
    thread[0].secs2snooze = (rand() % 5) + 1;
    thread[0].mn = &mn;
    thread[0].ptr_size = 512;

    thread[1].id = "thread 1";
    thread[1].secs2snooze = (rand() % 5) + 1;
    thread[1].mn = &mn;
    thread[1].ptr_size = 256;

    thread[2].id = "thread 2";
    thread[2].secs2snooze = (rand() % 5) + 1;
    thread[2].mn = &mn;
    thread[2].ptr_size = 2048;

    CUTE_ASSERT(pthread_create(&thread[0].t, NULL, get_memory, &thread[0]) == 0);
    CUTE_ASSERT(pthread_create(&thread[1].t, NULL, get_memory, &thread[1]) == 0);
    CUTE_ASSERT(pthread_create(&thread[2].t, NULL, get_memory, &thread[2]) == 0);

    pthread_join(thread[0].t, NULL);
    pthread_join(thread[1].t, NULL);
    pthread_join(thread[2].t, NULL);

    CUTE_ASSERT(thread[0].ptr != NULL);
    CUTE_ASSERT(thread[1].ptr != NULL);
    CUTE_ASSERT(thread[2].ptr == NULL);

    thread[2].ptr = malloc(2048);
    CUTE_ASSERT(thread[2].ptr != NULL);

    CUTE_ASSERT(pthread_create(&thread[0].t, NULL, free_memory, &thread[0]) == 0);
    CUTE_ASSERT(pthread_create(&thread[1].t, NULL, free_memory, &thread[1]) == 0);
    CUTE_ASSERT(pthread_create(&thread[2].t, NULL, free_memory, &thread[2]) == 0);

    pthread_join(thread[0].t, NULL);
    pthread_join(thread[1].t, NULL);
    pthread_join(thread[2].t, NULL);

    CUTE_ASSERT(thread[0].retval == 1);
    CUTE_ASSERT(thread[1].retval == 1);
    CUTE_ASSERT(thread[2].retval == 0);

    free(thread[2].ptr);

    g_cute_leak_check = smelly_hack;

    mnemosine_finis(&mn);
#else
    struct mnemosine_ctx mn;
    struct async_test_ctx thread[3];
    int smelly_hack = g_cute_leak_check;

    srand(time(0));

    CUTE_ASSERT(mnemosine_init(&mn, mnemosine_size_kb(1), 0) == 1);

    g_cute_leak_check = 0;

    thread[0].id = "thread 0";
    thread[0].secs2snooze = (rand() % 5) + 1;
    thread[0].mn = &mn;
    thread[0].ptr_size = 512;

    thread[1].id = "thread 1";
    thread[1].secs2snooze = (rand() % 5) + 1;
    thread[1].mn = &mn;
    thread[1].ptr_size = 256;

    thread[2].id = "thread 2";
    thread[2].secs2snooze = (rand() % 5) + 1;
    thread[2].mn = &mn;
    thread[2].ptr_size = 2048;

    thread[0].t = CreateThread(NULL, 0, get_memory, &thread[0], 0, NULL);
    thread[1].t = CreateThread(NULL, 0, get_memory, &thread[1], 0, NULL);
    thread[2].t = CreateThread(NULL, 0, get_memory, &thread[2], 0, NULL);

    CUTE_ASSERT(thread[0].t != NULL && thread[1].t != NULL && thread[2].t != NULL);

    WaitForSingleObject(thread[0].t, INFINITE);
    WaitForSingleObject(thread[1].t, INFINITE);
    WaitForSingleObject(thread[2].t, INFINITE);

    CloseHandle(thread[0].t);
    CloseHandle(thread[1].t);
    CloseHandle(thread[2].t);
 
    CUTE_ASSERT(thread[0].ptr != NULL);
    CUTE_ASSERT(thread[1].ptr != NULL);
    CUTE_ASSERT(thread[2].ptr == NULL);

    thread[2].ptr = malloc(2048);
    CUTE_ASSERT(thread[2].ptr != NULL);

    thread[0].t = CreateThread(NULL, 0, free_memory, &thread[0], 0, NULL);
    thread[1].t = CreateThread(NULL, 0, free_memory, &thread[1], 0, NULL);
    thread[2].t = CreateThread(NULL, 0, free_memory, &thread[2], 0, NULL);

    CUTE_ASSERT(thread[0].t != NULL && thread[1].t != NULL && thread[2].t != NULL);

    WaitForSingleObject(thread[0].t, INFINITE);
    WaitForSingleObject(thread[1].t, INFINITE);
    WaitForSingleObject(thread[2].t, INFINITE);

    CloseHandle(thread[0].t);
    CloseHandle(thread[1].t);
    CloseHandle(thread[2].t);

    CUTE_ASSERT(thread[0].retval == 1);
    CUTE_ASSERT(thread[1].retval == 1);
    CUTE_ASSERT(thread[2].retval == 0);

    free(thread[2].ptr);

    g_cute_leak_check = smelly_hack;

    mnemosine_finis(&mn);
#endif
CUTE_TEST_CASE_END
#endif

CUTE_TEST_CASE(mnemosine_size_macros_tests)
    CUTE_ASSERT(mnemosine_size_kb(1) == 1024);
    CUTE_ASSERT(mnemosine_size_kb(2) == 2048);
    CUTE_ASSERT(mnemosine_size_kb(3) == 3072);
    CUTE_ASSERT(mnemosine_size_kb(4) == 4096);

    CUTE_ASSERT(mnemosine_size_mb(1) == 1048576);
    CUTE_ASSERT(mnemosine_size_mb(40) == 41943040);

    CUTE_ASSERT(mnemosine_size_gb(1) == 1073741824);
    CUTE_ASSERT(mnemosine_size_gb(2) == 2147483648);
CUTE_TEST_CASE_END

CUTE_MAIN(mnemosine_tests);
