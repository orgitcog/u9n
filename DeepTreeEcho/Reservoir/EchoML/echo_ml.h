/**
 * echo_ml.h - Minimal ML Framework for Deep Tree Echo
 * 
 * A lightweight, high-performance ML inference engine designed for
 * embodied cognition in resource-constrained environments.
 * 
 * Target: < 10MB total footprint including models
 * 
 * Core Components:
 * - Tensor: Efficient multi-dimensional array with SIMD ops
 * - Embedding: Learnable lookup tables for tokens/concepts
 * - Dense: Matrix multiplication with activations
 * - Reservoir: Echo State Network for temporal dynamics
 * - EchoEngine: Main inference orchestrator
 */

#ifndef ECHO_ML_H
#define ECHO_ML_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/* Use quantized (int8) weights for 4x memory reduction */
#define ECHO_USE_QUANTIZED_WEIGHTS 1

/* Enable SIMD optimizations (AVX2/SSE4.1) */
#define ECHO_USE_SIMD 1

/* Maximum dimensions for static allocation */
#define ECHO_MAX_DIMS 4
#define ECHO_MAX_RESERVOIR_SIZE 1024
#define ECHO_MAX_EMBEDDING_DIM 256
#define ECHO_MAX_VOCAB_SIZE 65536

/* ============================================================================
 * BASIC TYPES
 * ============================================================================ */

typedef float echo_float;
typedef int8_t echo_qint8;  /* Quantized weight type */
typedef uint16_t echo_token; /* Token/vocabulary index */

/* Activation function types */
typedef enum {
    ECHO_ACT_NONE = 0,
    ECHO_ACT_TANH,
    ECHO_ACT_RELU,
    ECHO_ACT_SIGMOID,
    ECHO_ACT_SOFTMAX,
    ECHO_ACT_LEAKY_RELU
} EchoActivation;

/* Error codes */
typedef enum {
    ECHO_OK = 0,
    ECHO_ERR_ALLOC,
    ECHO_ERR_SHAPE,
    ECHO_ERR_FILE,
    ECHO_ERR_INVALID,
    ECHO_ERR_OVERFLOW
} EchoError;

/* ============================================================================
 * TENSOR - Core multi-dimensional array
 * ============================================================================ */

typedef struct {
    echo_float* data;       /* Pointer to data (owned or borrowed) */
    size_t shape[ECHO_MAX_DIMS];  /* Dimensions */
    size_t strides[ECHO_MAX_DIMS]; /* Strides for indexing */
    uint8_t ndim;           /* Number of dimensions */
    bool owns_data;         /* Whether we own the data pointer */
} EchoTensor;

/* Tensor creation and destruction */
EchoError echo_tensor_alloc(EchoTensor* t, const size_t* shape, uint8_t ndim);
EchoError echo_tensor_wrap(EchoTensor* t, echo_float* data, const size_t* shape, uint8_t ndim);
void echo_tensor_free(EchoTensor* t);

/* Tensor operations */
size_t echo_tensor_numel(const EchoTensor* t);
echo_float* echo_tensor_ptr(EchoTensor* t, const size_t* indices);
void echo_tensor_fill(EchoTensor* t, echo_float value);
void echo_tensor_copy(EchoTensor* dst, const EchoTensor* src);

/* ============================================================================
 * QUANTIZED TENSOR - For compressed weight storage
 * ============================================================================ */

typedef struct {
    echo_qint8* data;       /* Quantized data */
    echo_float scale;       /* Scale factor for dequantization */
    echo_float zero_point;  /* Zero point for dequantization */
    size_t shape[ECHO_MAX_DIMS];
    size_t strides[ECHO_MAX_DIMS];
    uint8_t ndim;
    bool owns_data;
} EchoQTensor;

/* Quantization/dequantization */
EchoError echo_qtensor_from_tensor(EchoQTensor* qt, const EchoTensor* t);
EchoError echo_qtensor_to_tensor(EchoTensor* t, const EchoQTensor* qt);
void echo_qtensor_free(EchoQTensor* qt);

/* ============================================================================
 * SPARSE MATRIX - For reservoir connections (CSR format)
 * ============================================================================ */

typedef struct {
    echo_float* values;     /* Non-zero values */
    uint32_t* col_indices;  /* Column indices */
    uint32_t* row_ptr;      /* Row pointers */
    size_t nnz;             /* Number of non-zeros */
    size_t rows;
    size_t cols;
} EchoSparseMatrix;

EchoError echo_sparse_alloc(EchoSparseMatrix* m, size_t rows, size_t cols, size_t nnz);
void echo_sparse_free(EchoSparseMatrix* m);
EchoError echo_sparse_from_dense(EchoSparseMatrix* m, const EchoTensor* dense, echo_float threshold);

/* Sparse matrix-vector multiplication: y = A * x */
void echo_sparse_matvec(echo_float* y, const EchoSparseMatrix* A, const echo_float* x);

/* ============================================================================
 * EMBEDDING LAYER - Learnable lookup table
 * ============================================================================ */

typedef struct {
    EchoTensor weights;     /* Shape: [vocab_size, embedding_dim] */
    size_t vocab_size;
    size_t embedding_dim;
} EchoEmbedding;

EchoError echo_embedding_init(EchoEmbedding* e, size_t vocab_size, size_t embedding_dim);
void echo_embedding_free(EchoEmbedding* e);

/* Lookup: output[i] = weights[tokens[i]] */
void echo_embedding_forward(echo_float* output, const EchoEmbedding* e, 
                            const echo_token* tokens, size_t num_tokens);

/* ============================================================================
 * DENSE LAYER - Fully connected layer
 * ============================================================================ */

typedef struct {
    EchoTensor weights;     /* Shape: [out_features, in_features] */
    EchoTensor bias;        /* Shape: [out_features] */
    size_t in_features;
    size_t out_features;
    EchoActivation activation;
    bool use_bias;
} EchoDense;

EchoError echo_dense_init(EchoDense* d, size_t in_features, size_t out_features,
                          EchoActivation activation, bool use_bias);
void echo_dense_free(EchoDense* d);

/* Forward pass: output = activation(weights @ input + bias) */
void echo_dense_forward(echo_float* output, const EchoDense* d, const echo_float* input);

/* Batched forward: output[b] = activation(weights @ input[b] + bias) */
void echo_dense_forward_batch(echo_float* output, const EchoDense* d,
                              const echo_float* input, size_t batch_size);

/* ============================================================================
 * RESERVOIR - Echo State Network
 * ============================================================================ */

typedef struct {
    EchoSparseMatrix W_res;     /* Reservoir weights (sparse) */
    EchoTensor W_in;            /* Input weights: [reservoir_size, input_dim] */
    EchoTensor W_out;           /* Output weights: [output_dim, reservoir_size] */
    EchoTensor state;           /* Current reservoir state: [reservoir_size] */
    
    size_t reservoir_size;
    size_t input_dim;
    size_t output_dim;
    
    echo_float spectral_radius; /* For stability */
    echo_float leak_rate;       /* Leaky integration */
    echo_float input_scaling;   /* Input scaling factor */
} EchoReservoir;

EchoError echo_reservoir_init(EchoReservoir* r, size_t reservoir_size,
                              size_t input_dim, size_t output_dim,
                              echo_float spectral_radius, echo_float sparsity);
void echo_reservoir_free(EchoReservoir* r);
void echo_reservoir_reset(EchoReservoir* r);

/* Single step: updates state and returns output */
void echo_reservoir_step(echo_float* output, EchoReservoir* r, const echo_float* input);

/* Process sequence: outputs shape [seq_len, output_dim] */
void echo_reservoir_forward(echo_float* outputs, EchoReservoir* r,
                            const echo_float* inputs, size_t seq_len);

/* ============================================================================
 * ECHO ENGINE - Main inference orchestrator
 * ============================================================================ */

/* Model configuration */
typedef struct {
    size_t vocab_size;
    size_t embedding_dim;
    size_t reservoir_size;
    size_t hidden_dim;
    size_t output_dim;
    echo_float spectral_radius;
    echo_float reservoir_sparsity;
} EchoConfig;

/* The main engine */
typedef struct {
    EchoConfig config;
    
    /* Layers */
    EchoEmbedding embedding;
    EchoReservoir reservoir;
    EchoDense output_layer;
    
    /* Working buffers */
    echo_float* embed_buffer;
    echo_float* reservoir_output;
    
    bool initialized;
} EchoEngine;

EchoError echo_engine_init(EchoEngine* engine, const EchoConfig* config);
void echo_engine_free(EchoEngine* engine);

/* Load weights from file */
EchoError echo_engine_load(EchoEngine* engine, const char* path);

/* Save weights to file */
EchoError echo_engine_save(const EchoEngine* engine, const char* path);

/* Inference: process token sequence and return output */
void echo_engine_forward(echo_float* output, EchoEngine* engine,
                         const echo_token* tokens, size_t num_tokens);

/* Reset internal state */
void echo_engine_reset(EchoEngine* engine);

/* ============================================================================
 * SIMD-OPTIMIZED OPERATIONS
 * ============================================================================ */

/* Vector operations (auto-vectorized or explicit SIMD) */
void echo_vec_add(echo_float* dst, const echo_float* a, const echo_float* b, size_t n);
void echo_vec_mul(echo_float* dst, const echo_float* a, const echo_float* b, size_t n);
void echo_vec_scale(echo_float* dst, const echo_float* src, echo_float scale, size_t n);
echo_float echo_vec_dot(const echo_float* a, const echo_float* b, size_t n);
void echo_vec_tanh(echo_float* dst, const echo_float* src, size_t n);
void echo_vec_relu(echo_float* dst, const echo_float* src, size_t n);
void echo_vec_sigmoid(echo_float* dst, const echo_float* src, size_t n);

/* Matrix operations */
void echo_gemv(echo_float* y, const echo_float* A, const echo_float* x,
               size_t rows, size_t cols);  /* y = A @ x */
void echo_gemm(echo_float* C, const echo_float* A, const echo_float* B,
               size_t M, size_t N, size_t K);  /* C = A @ B */

/* ============================================================================
 * DICTIONARY / CONTEXT SYSTEM
 * ============================================================================ */

typedef struct {
    char** tokens;          /* Token strings */
    echo_token* ids;        /* Token IDs */
    size_t size;            /* Number of entries */
    size_t capacity;
} EchoDict;

EchoError echo_dict_load(EchoDict* dict, const char* path);
void echo_dict_free(EchoDict* dict);
echo_token echo_dict_lookup(const EchoDict* dict, const char* token);
const char* echo_dict_get_token(const EchoDict* dict, echo_token id);

/* Tokenize a string into token IDs */
size_t echo_tokenize(echo_token* output, size_t max_tokens,
                     const EchoDict* dict, const char* text);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/* Random number generation (for initialization) */
void echo_seed(uint64_t seed);
echo_float echo_randn(void);  /* Standard normal */
echo_float echo_rand(void);   /* Uniform [0, 1) */

/* Memory-mapped file loading (for large models) */
typedef struct {
    void* data;
    size_t size;
    int fd;
} EchoMmap;

EchoError echo_mmap_open(EchoMmap* m, const char* path);
void echo_mmap_close(EchoMmap* m);

#ifdef __cplusplus
}
#endif

#endif /* ECHO_ML_H */
