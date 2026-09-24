/**
 * echo_tensor.c - Tensor operations and SIMD-optimized vector math
 * 
 * This file implements the core tensor data structure and optimized
 * vector/matrix operations using SIMD intrinsics where available.
 */

#include "echo_ml.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef ECHO_USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define ECHO_HAS_AVX2 1
#elif defined(__aarch64__)
#include <arm_neon.h>
#define ECHO_HAS_NEON 1
#endif
#endif

/* ============================================================================
 * TENSOR OPERATIONS
 * ============================================================================ */

EchoError echo_tensor_alloc(EchoTensor* t, const size_t* shape, uint8_t ndim) {
    if (ndim > ECHO_MAX_DIMS) return ECHO_ERR_SHAPE;
    
    t->ndim = ndim;
    size_t total = 1;
    
    for (int i = ndim - 1; i >= 0; i--) {
        t->shape[i] = shape[i];
        t->strides[i] = total;
        total *= shape[i];
    }
    
    /* Align to 32 bytes for SIMD; size must be a multiple of alignment */
    size_t byte_size = total * sizeof(echo_float);
    byte_size = (byte_size + 31) & ~(size_t)31;  /* round up to 32-byte boundary */
    t->data = (echo_float*)aligned_alloc(32, byte_size);
    if (!t->data) return ECHO_ERR_ALLOC;
    
    t->owns_data = true;
    memset(t->data, 0, total * sizeof(echo_float));
    
    return ECHO_OK;
}

EchoError echo_tensor_wrap(EchoTensor* t, echo_float* data, const size_t* shape, uint8_t ndim) {
    if (ndim > ECHO_MAX_DIMS) return ECHO_ERR_SHAPE;
    
    t->data = data;
    t->ndim = ndim;
    t->owns_data = false;
    
    size_t stride = 1;
    for (int i = ndim - 1; i >= 0; i--) {
        t->shape[i] = shape[i];
        t->strides[i] = stride;
        stride *= shape[i];
    }
    
    return ECHO_OK;
}

void echo_tensor_free(EchoTensor* t) {
    if (t->owns_data && t->data) {
        free(t->data);
    }
    t->data = NULL;
    t->owns_data = false;
}

size_t echo_tensor_numel(const EchoTensor* t) {
    size_t n = 1;
    for (uint8_t i = 0; i < t->ndim; i++) {
        n *= t->shape[i];
    }
    return n;
}

echo_float* echo_tensor_ptr(EchoTensor* t, const size_t* indices) {
    size_t offset = 0;
    for (uint8_t i = 0; i < t->ndim; i++) {
        offset += indices[i] * t->strides[i];
    }
    return t->data + offset;
}

void echo_tensor_fill(EchoTensor* t, echo_float value) {
    size_t n = echo_tensor_numel(t);
    for (size_t i = 0; i < n; i++) {
        t->data[i] = value;
    }
}

void echo_tensor_copy(EchoTensor* dst, const EchoTensor* src) {
    size_t n = echo_tensor_numel(src);
    memcpy(dst->data, src->data, n * sizeof(echo_float));
}

/* ============================================================================
 * SIMD-OPTIMIZED VECTOR OPERATIONS
 * ============================================================================ */

#ifdef ECHO_HAS_AVX2

/* AVX2 implementation - processes 8 floats at a time */

void echo_vec_add(echo_float* dst, const echo_float* a, const echo_float* b, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vr = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(dst + i, vr);
    }
    for (; i < n; i++) {
        dst[i] = a[i] + b[i];
    }
}

void echo_vec_mul(echo_float* dst, const echo_float* a, const echo_float* b, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vr = _mm256_mul_ps(va, vb);
        _mm256_storeu_ps(dst + i, vr);
    }
    for (; i < n; i++) {
        dst[i] = a[i] * b[i];
    }
}

void echo_vec_scale(echo_float* dst, const echo_float* src, echo_float scale, size_t n) {
    size_t i = 0;
    __m256 vs = _mm256_set1_ps(scale);
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(src + i);
        __m256 vr = _mm256_mul_ps(va, vs);
        _mm256_storeu_ps(dst + i, vr);
    }
    for (; i < n; i++) {
        dst[i] = src[i] * scale;
    }
}

echo_float echo_vec_dot(const echo_float* a, const echo_float* b, size_t n) {
    size_t i = 0;
    __m256 sum = _mm256_setzero_ps();
    
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_fmadd_ps(va, vb, sum);
    }
    
    /* Horizontal sum */
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 sum128 = _mm_add_ps(lo, hi);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    
    echo_float result = _mm_cvtss_f32(sum128);
    
    for (; i < n; i++) {
        result += a[i] * b[i];
    }
    
    return result;
}

/* Fast tanh approximation using SIMD */
void echo_vec_tanh(echo_float* dst, const echo_float* src, size_t n) {
    size_t i = 0;
    
    /* Constants for rational approximation */
    __m256 c1 = _mm256_set1_ps(0.03138777f);
    __m256 c2 = _mm256_set1_ps(0.276281267f);
    __m256 one = _mm256_set1_ps(1.0f);
    __m256 neg_one = _mm256_set1_ps(-1.0f);
    
    for (; i + 8 <= n; i += 8) {
        __m256 x = _mm256_loadu_ps(src + i);
        __m256 x2 = _mm256_mul_ps(x, x);
        
        /* Approximation: tanh(x) ≈ x * (1 + c1*x^2) / (1 + c2*x^2) for small x */
        /* Clamp to [-1, 1] for large values */
        __m256 num = _mm256_fmadd_ps(c1, x2, one);
        __m256 den = _mm256_fmadd_ps(c2, x2, one);
        __m256 result = _mm256_mul_ps(x, _mm256_div_ps(num, den));
        
        /* Clamp */
        result = _mm256_min_ps(result, one);
        result = _mm256_max_ps(result, neg_one);
        
        _mm256_storeu_ps(dst + i, result);
    }
    
    for (; i < n; i++) {
        dst[i] = tanhf(src[i]);
    }
}

void echo_vec_relu(echo_float* dst, const echo_float* src, size_t n) {
    size_t i = 0;
    __m256 zero = _mm256_setzero_ps();
    
    for (; i + 8 <= n; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m256 r = _mm256_max_ps(v, zero);
        _mm256_storeu_ps(dst + i, r);
    }
    
    for (; i < n; i++) {
        dst[i] = src[i] > 0 ? src[i] : 0;
    }
}

void echo_vec_sigmoid(echo_float* dst, const echo_float* src, size_t n) {
    /* sigmoid(x) = 1 / (1 + exp(-x)) */
    /* Use fast approximation */
    size_t i = 0;
    __m256 one = _mm256_set1_ps(1.0f);
    __m256 half = _mm256_set1_ps(0.5f);
    __m256 c1 = _mm256_set1_ps(0.25f);
    
    for (; i + 8 <= n; i += 8) {
        __m256 x = _mm256_loadu_ps(src + i);
        /* Fast approximation: sigmoid(x) ≈ 0.5 + 0.25*x for |x| < 2 */
        /* Clamp to [0, 1] */
        __m256 result = _mm256_fmadd_ps(c1, x, half);
        result = _mm256_min_ps(result, one);
        result = _mm256_max_ps(result, _mm256_setzero_ps());
        _mm256_storeu_ps(dst + i, result);
    }
    
    for (; i < n; i++) {
        dst[i] = 1.0f / (1.0f + expf(-src[i]));
    }
}

#else /* Fallback scalar implementation */

void echo_vec_add(echo_float* dst, const echo_float* a, const echo_float* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = a[i] + b[i];
    }
}

void echo_vec_mul(echo_float* dst, const echo_float* a, const echo_float* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = a[i] * b[i];
    }
}

void echo_vec_scale(echo_float* dst, const echo_float* src, echo_float scale, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = src[i] * scale;
    }
}

echo_float echo_vec_dot(const echo_float* a, const echo_float* b, size_t n) {
    echo_float sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

void echo_vec_tanh(echo_float* dst, const echo_float* src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = tanhf(src[i]);
    }
}

void echo_vec_relu(echo_float* dst, const echo_float* src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = src[i] > 0 ? src[i] : 0;
    }
}

void echo_vec_sigmoid(echo_float* dst, const echo_float* src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = 1.0f / (1.0f + expf(-src[i]));
    }
}

#endif /* ECHO_HAS_AVX2 */

/* ============================================================================
 * MATRIX OPERATIONS
 * ============================================================================ */

/* General matrix-vector multiplication: y = A @ x */
void echo_gemv(echo_float* y, const echo_float* A, const echo_float* x,
               size_t rows, size_t cols) {
    for (size_t i = 0; i < rows; i++) {
        y[i] = echo_vec_dot(A + i * cols, x, cols);
    }
}

/* General matrix-matrix multiplication: C = A @ B */
/* A: [M, K], B: [K, N], C: [M, N] */
void echo_gemm(echo_float* C, const echo_float* A, const echo_float* B,
               size_t M, size_t N, size_t K) {
    /* Simple blocked implementation for cache efficiency */
    const size_t BLOCK = 32;
    
    memset(C, 0, M * N * sizeof(echo_float));
    
    for (size_t i0 = 0; i0 < M; i0 += BLOCK) {
        size_t imax = (i0 + BLOCK < M) ? i0 + BLOCK : M;
        for (size_t k0 = 0; k0 < K; k0 += BLOCK) {
            size_t kmax = (k0 + BLOCK < K) ? k0 + BLOCK : K;
            for (size_t j0 = 0; j0 < N; j0 += BLOCK) {
                size_t jmax = (j0 + BLOCK < N) ? j0 + BLOCK : N;
                
                for (size_t i = i0; i < imax; i++) {
                    for (size_t k = k0; k < kmax; k++) {
                        echo_float a_ik = A[i * K + k];
                        for (size_t j = j0; j < jmax; j++) {
                            C[i * N + j] += a_ik * B[k * N + j];
                        }
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * SPARSE MATRIX OPERATIONS
 * ============================================================================ */

EchoError echo_sparse_alloc(EchoSparseMatrix* m, size_t rows, size_t cols, size_t nnz) {
    m->rows = rows;
    m->cols = cols;
    m->nnz = nnz;
    
    m->values = (echo_float*)malloc(nnz * sizeof(echo_float));
    m->col_indices = (uint32_t*)malloc(nnz * sizeof(uint32_t));
    m->row_ptr = (uint32_t*)malloc((rows + 1) * sizeof(uint32_t));
    
    if (!m->values || !m->col_indices || !m->row_ptr) {
        echo_sparse_free(m);
        return ECHO_ERR_ALLOC;
    }
    
    return ECHO_OK;
}

void echo_sparse_free(EchoSparseMatrix* m) {
    free(m->values);
    free(m->col_indices);
    free(m->row_ptr);
    m->values = NULL;
    m->col_indices = NULL;
    m->row_ptr = NULL;
}

EchoError echo_sparse_from_dense(EchoSparseMatrix* m, const EchoTensor* dense, echo_float threshold) {
    if (dense->ndim != 2) return ECHO_ERR_SHAPE;
    
    size_t rows = dense->shape[0];
    size_t cols = dense->shape[1];
    
    /* Count non-zeros */
    size_t nnz = 0;
    for (size_t i = 0; i < rows * cols; i++) {
        if (fabsf(dense->data[i]) > threshold) nnz++;
    }
    
    EchoError err = echo_sparse_alloc(m, rows, cols, nnz);
    if (err != ECHO_OK) return err;
    
    /* Fill CSR structure */
    size_t idx = 0;
    for (size_t i = 0; i < rows; i++) {
        m->row_ptr[i] = idx;
        for (size_t j = 0; j < cols; j++) {
            echo_float val = dense->data[i * cols + j];
            if (fabsf(val) > threshold) {
                m->values[idx] = val;
                m->col_indices[idx] = j;
                idx++;
            }
        }
    }
    m->row_ptr[rows] = idx;
    
    return ECHO_OK;
}

/* Sparse matrix-vector multiplication: y = A * x */
void echo_sparse_matvec(echo_float* y, const EchoSparseMatrix* A, const echo_float* x) {
    for (size_t i = 0; i < A->rows; i++) {
        echo_float sum = 0;
        for (uint32_t j = A->row_ptr[i]; j < A->row_ptr[i + 1]; j++) {
            sum += A->values[j] * x[A->col_indices[j]];
        }
        y[i] = sum;
    }
}

/* ============================================================================
 * QUANTIZED TENSOR OPERATIONS
 * ============================================================================ */

EchoError echo_qtensor_from_tensor(EchoQTensor* qt, const EchoTensor* t) {
    size_t n = echo_tensor_numel(t);
    
    /* Find min/max for quantization range */
    echo_float min_val = t->data[0];
    echo_float max_val = t->data[0];
    for (size_t i = 1; i < n; i++) {
        if (t->data[i] < min_val) min_val = t->data[i];
        if (t->data[i] > max_val) max_val = t->data[i];
    }
    
    /* Compute scale and zero point for symmetric quantization */
    echo_float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));
    qt->scale = abs_max / 127.0f;
    qt->zero_point = 0;
    
    if (qt->scale < 1e-8f) qt->scale = 1e-8f;
    
    /* Allocate quantized data */
    qt->data = (echo_qint8*)malloc(n * sizeof(echo_qint8));
    if (!qt->data) return ECHO_ERR_ALLOC;
    
    /* Copy shape info */
    qt->ndim = t->ndim;
    for (uint8_t i = 0; i < t->ndim; i++) {
        qt->shape[i] = t->shape[i];
        qt->strides[i] = t->strides[i];
    }
    qt->owns_data = true;
    
    /* Quantize */
    for (size_t i = 0; i < n; i++) {
        float scaled = t->data[i] / qt->scale;
        int32_t rounded = (int32_t)roundf(scaled);
        if (rounded < -128) rounded = -128;
        if (rounded > 127) rounded = 127;
        qt->data[i] = (echo_qint8)rounded;
    }
    
    return ECHO_OK;
}

EchoError echo_qtensor_to_tensor(EchoTensor* t, const EchoQTensor* qt) {
    EchoError err = echo_tensor_alloc(t, qt->shape, qt->ndim);
    if (err != ECHO_OK) return err;
    
    size_t n = echo_tensor_numel(t);
    for (size_t i = 0; i < n; i++) {
        t->data[i] = qt->data[i] * qt->scale;
    }
    
    return ECHO_OK;
}

void echo_qtensor_free(EchoQTensor* qt) {
    if (qt->owns_data && qt->data) {
        free(qt->data);
    }
    qt->data = NULL;
    qt->owns_data = false;
}

/* ============================================================================
 * RANDOM NUMBER GENERATION (Xorshift128+)
 * ============================================================================ */

static uint64_t rng_state[2] = {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL};

void echo_seed(uint64_t seed) {
    rng_state[0] = seed;
    rng_state[1] = seed ^ 0x5851f42d4c957f2dULL;
}

static uint64_t xorshift128plus(void) {
    uint64_t s1 = rng_state[0];
    uint64_t s0 = rng_state[1];
    rng_state[0] = s0;
    s1 ^= s1 << 23;
    rng_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return rng_state[1] + s0;
}

echo_float echo_rand(void) {
    return (xorshift128plus() >> 11) * (1.0f / 9007199254740992.0f);
}

/* Box-Muller transform for normal distribution */
echo_float echo_randn(void) {
    static int have_spare = 0;
    static echo_float spare;
    
    if (have_spare) {
        have_spare = 0;
        return spare;
    }
    
    echo_float u, v, s;
    do {
        u = echo_rand() * 2.0f - 1.0f;
        v = echo_rand() * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);
    
    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    have_spare = 1;
    
    return u * s;
}
