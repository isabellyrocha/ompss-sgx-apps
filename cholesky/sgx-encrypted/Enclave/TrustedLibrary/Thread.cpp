/*
 * Copyright (C) 2011-2019 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include "../Enclave.h"
#include "Enclave_t.h"
#include <mkl.h>
#include "sgx_thread.h"
//#include <sys/mman.h>

static size_t global_counter = 0;
static sgx_thread_mutex_t global_mutex = SGX_THREAD_MUTEX_INITIALIZER;
int offset = 0;

#define BUFFER_SIZE 50

typedef struct {
    int buf[BUFFER_SIZE];
    int occupied;
    int nextin;
    int nextout;
    sgx_thread_mutex_t mutex;
    sgx_thread_cond_t more;
    sgx_thread_cond_t less;
} cond_buffer_t;

static cond_buffer_t buffer = {{0, 0, 0, 0, 0, 0}, 0, 0, 0,
    SGX_THREAD_MUTEX_INITIALIZER, SGX_THREAD_COND_INITIALIZER, SGX_THREAD_COND_INITIALIZER};
/*
extern "C" {

void dgemm_ (const char *transa, const char *transb, int *l, int *n, int *m, double *alpha,
             const void *a, int *lda, void *b, int *ldb, double *beta, void *c, int *ldc);
void dtrsm_ (char *side, char *uplo, char *transa, char *diag, int *m, int *n, double *alpha,
             double *a, int *lda, double *b, int *ldb);
void dtrmm_ (char *side, char *uplo, char *transa, char *diag, int *m, int *n, double *alpha,
             double *a, int *lda, double *b, int *ldb);
void dsyrk_ (char *uplo, char *trans, int *n, int *k, double *alpha, double *a, int *lda,
             double *beta, double *c, int *ldc);

int __printf_chk() {
    return 0;
}

int perror() {
    return 0;
}

int puts() {
    return 0;
}

//int mmap() {
//    for(;;);
//    return 0;
//}

void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset) {
    return 0;
}

int munmap(void *addr, size_t length) {
    return 0;
}

int getenv() {
    return 0;
}

//int munmap() {
//    return 0;
//}

int syscall() {
    return 0;
}
int alloc_mmap() {
    return 0;
}

int sched_yield() {
    return 0;
}


int pthread_mutex_lock() {
    return 0;
}

int pthread_mutex_unlock() {
    return 0;
}

};
*/
/*
 * ecall_increase_counter:
 *   Utilize thread APIs inside the enclave.
 */
size_t ecall_increase_counter(void)
{
    size_t ret = 0;
    for (int i = 0; i < LOOPS_PER_THREAD; i++) {
        sgx_thread_mutex_lock(&global_mutex);
        /* mutually exclusive adding */
        size_t tmp = global_counter;
        global_counter = ++tmp;
        if (4*LOOPS_PER_THREAD == global_counter)
            ret = global_counter;
        sgx_thread_mutex_unlock(&global_mutex);
    }
    return ret;
}

void ecall_producer(void)
{
    for (int i = 0; i < 4*LOOPS_PER_THREAD; i++) {
        cond_buffer_t *b = &buffer;
        sgx_thread_mutex_lock(&b->mutex);
        while (b->occupied >= BUFFER_SIZE)
            sgx_thread_cond_wait(&b->less, &b->mutex);
        b->buf[b->nextin] = b->nextin;
        b->nextin++;
        b->nextin %= BUFFER_SIZE;
        b->occupied++;
        sgx_thread_cond_signal(&b->more);
        sgx_thread_mutex_unlock(&b->mutex);
    }
}

void ecall_consumer(void)
{
    for (int i = 0; i < LOOPS_PER_THREAD; i++) {
        cond_buffer_t *b = &buffer;
        sgx_thread_mutex_lock(&b->mutex);
        while(b->occupied <= 0)
            sgx_thread_cond_wait(&b->more, &b->mutex);
        b->buf[b->nextout++] = 0;
        b->nextout %= BUFFER_SIZE;
        b->occupied--;
        sgx_thread_cond_signal(&b->less);
        sgx_thread_mutex_unlock(&b->mutex);
    }
}

void encrypt(long NB, double* matrix) {
    for (long i = 0; i < NB; i++) {
        matrix[i] = matrix[i] - offset;
    }
}

void decrypt(int NB, double* matrix) {
    for (int i = 0; i < NB; i++) {
        matrix[i] = matrix[i] + offset;
    }
}

//#pragma omp task inout([ts][ts]A)
void ecall_omp_potrf(double * const A, int ts, int ld)
{
   decrypt(ts, A);
   static int INFO;
   static const char L = 'L';
#if 0
   dpotrf_(&L, &ts, A, &ld, &INFO);
#else
   for (int j = 0; j < ts; ++j) {
      for (int k = 0; k < j; ++k) {
         A[j*ts+j] -= A[k*ts+j]*A[k*ts+j];
      }
      //A[j][j] = sqrt(A[j][j]);
      double l = 0, h = A[j*ts+j], m;
      for (int i = 0; i < 256/*max_iters_to_converge*/; ++i) {
         m = (l+h)/2;
         if (m*m == A[j*ts+j]) break;
         else if (m*m > A[j*ts+j]) h = m;
         else l = m;
      }
      A[j*ts+j] = m;

      for (int i = j + 1; i < ts; ++i) {
         for (int k = 0; k < j; ++k) {
            A[j*ts+i] -= A[k*ts+i]*A[k*ts+j];
         }
         A[j*ts+i] /= A[j*ts+j];
      }
   }
   encrypt(ts, A);
#endif
}

//#pragma omp task in([ts][ts]A) inout([ts][ts]B)
void ecall_omp_trsm(double *A, double *B, int ts, int ld)
{
   decrypt(ts, A);
   decrypt(ts, B);
   static char LO = 'L', TR = 'T', NU = 'N', RI = 'R';
   static double DONE = 1.0;
#if 0
   dtrsm_(&RI, &LO, &TR, &NU, &ts, &ts, &DONE, A, &ld, B, &ld );
#else
   double tmp_row[ts];
   for (int k = 0; k < ts; ++k) {
      double temp = 1. / A[k*ts+k];
      for (int i__ = 0; i__ < ts; ++i__) {
         B[k*ts+i__] = tmp_row[i__] = temp * B[k*ts+i__];
      }
      for (int j = k + 1 ; j < ts; ++j) {
         temp = A[k*ts+j];
         for (int i__ = 0; i__ < ts; ++i__) {
            B[j*ts+i__] -= temp * tmp_row[i__];
         }
      }
   }
   encrypt(ts, A);
   encrypt(ts, B);
#endif
}

//#pragma omp task in([ts][ts]A) inout([ts][ts]B)
void ecall_omp_syrk(double *A, double *B, int ts, int ld)
{
   decrypt(ts, A);
   decrypt(ts, B);
   static char LO = 'L', NT = 'N';
   static double DONE = 1.0, DMONE = -1.0;
#if 0
   dsyrk_(&LO, &NT, &ts, &ts, &DMONE, A, &ld, &DONE, B, &ld );
#else
   for (int j = 0; j < ts; ++j) {
      for (int i__ = j; i__ < ts; ++i__) {
         double temp = B[j*ts+i__];
         for (int l = 0; l < ts; ++l) {
            temp += -A[l*ts+j] * A[l*ts+i__];
         }
         B[j*ts+i__] = temp;
      }
   }
#endif
   encrypt(ts, A);
   encrypt(ts, B);
}

//#pragma omp task in([ts][ts]A, [ts][ts]B) inout([ts][ts]C)
void ecall_omp_gemm(double *A, double *B, double *C, int ts, int ld)
{
   decrypt(ts, A);
   decrypt(ts, B);
   decrypt(ts, C);
   static const char TR = 'T', NT = 'N';
   static double DONE = 1.0, DMONE = -1.0;
#if 0
   dgemm_(&NT, &TR, &ts, &ts, &ts, &DMONE, A, &ld, B, &ld, &DONE, C, &ld);
#else
   for (int j = 0; j < ts; ++j) {
      for (int i__ = 0; i__ < ts; ++i__) {
         double temp = C[j*ts+i__];
         for (int l = 0; l < ts; ++l) {
            temp += -B[l*ts+j] * A[l*ts+i__];
         }
         C[j*ts+i__] = temp;
      }
   }
   encrypt(ts, A);
   encrypt(ts, B);
   encrypt(ts, C);
#endif
}
