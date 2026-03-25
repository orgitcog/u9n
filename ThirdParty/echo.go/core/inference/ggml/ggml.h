// ggml.h - GGML C API header for CGO bindings
// This header defines the minimal GGML API needed for Deep Tree Echo inference
// Based on ggml.h from llama.cpp project

#ifndef GGML_H
#define GGML_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// GGML TYPES
// =============================================================================

// Tensor data types
enum ggml_type {
    GGML_TYPE_F32     = 0,
    GGML_TYPE_F16     = 1,
    GGML_TYPE_Q4_0    = 2,
    GGML_TYPE_Q4_1    = 3,
    GGML_TYPE_Q5_0    = 6,
    GGML_TYPE_Q5_1    = 7,
    GGML_TYPE_Q8_0    = 8,
    GGML_TYPE_Q8_1    = 9,
    GGML_TYPE_Q2_K    = 10,
    GGML_TYPE_Q3_K    = 11,
    GGML_TYPE_Q4_K    = 12,
    GGML_TYPE_Q5_K    = 13,
    GGML_TYPE_Q6_K    = 14,
    GGML_TYPE_Q8_K    = 15,
    GGML_TYPE_IQ2_XXS = 16,
    GGML_TYPE_IQ2_XS  = 17,
    GGML_TYPE_IQ3_XXS = 18,
    GGML_TYPE_IQ1_S   = 19,
    GGML_TYPE_IQ4_NL  = 20,
    GGML_TYPE_IQ3_S   = 21,
    GGML_TYPE_IQ2_S   = 22,
    GGML_TYPE_IQ4_XS  = 23,
    GGML_TYPE_I8      = 24,
    GGML_TYPE_I16     = 25,
    GGML_TYPE_I32     = 26,
    GGML_TYPE_I64     = 27,
    GGML_TYPE_F64     = 28,
    GGML_TYPE_IQ1_M   = 29,
    GGML_TYPE_BF16    = 30,
    GGML_TYPE_COUNT,
};

// Backend types
enum ggml_backend_type {
    GGML_BACKEND_TYPE_CPU = 0,
    GGML_BACKEND_TYPE_GPU = 10,
    GGML_BACKEND_TYPE_GPU_SPLIT = 20,
};

// Log levels
enum ggml_log_level {
    GGML_LOG_LEVEL_NONE  = 0,
    GGML_LOG_LEVEL_DEBUG = 1,
    GGML_LOG_LEVEL_INFO  = 2,
    GGML_LOG_LEVEL_WARN  = 3,
    GGML_LOG_LEVEL_ERROR = 4,
    GGML_LOG_LEVEL_CONT  = 5,
};

// Operation types
enum ggml_op {
    GGML_OP_NONE = 0,
    GGML_OP_DUP,
    GGML_OP_ADD,
    GGML_OP_ADD1,
    GGML_OP_ACC,
    GGML_OP_SUB,
    GGML_OP_MUL,
    GGML_OP_DIV,
    GGML_OP_SQR,
    GGML_OP_SQRT,
    GGML_OP_LOG,
    GGML_OP_SIN,
    GGML_OP_COS,
    GGML_OP_SUM,
    GGML_OP_SUM_ROWS,
    GGML_OP_MEAN,
    GGML_OP_ARGMAX,
    GGML_OP_COUNT_EQUAL,
    GGML_OP_REPEAT,
    GGML_OP_REPEAT_BACK,
    GGML_OP_CONCAT,
    GGML_OP_SILU_BACK,
    GGML_OP_NORM,
    GGML_OP_RMS_NORM,
    GGML_OP_RMS_NORM_BACK,
    GGML_OP_GROUP_NORM,
    GGML_OP_MUL_MAT,
    GGML_OP_MUL_MAT_ID,
    GGML_OP_OUT_PROD,
    GGML_OP_SCALE,
    GGML_OP_SET,
    GGML_OP_CPY,
    GGML_OP_CONT,
    GGML_OP_RESHAPE,
    GGML_OP_VIEW,
    GGML_OP_PERMUTE,
    GGML_OP_TRANSPOSE,
    GGML_OP_GET_ROWS,
    GGML_OP_GET_ROWS_BACK,
    GGML_OP_DIAG,
    GGML_OP_DIAG_MASK_INF,
    GGML_OP_DIAG_MASK_ZERO,
    GGML_OP_SOFT_MAX,
    GGML_OP_SOFT_MAX_BACK,
    GGML_OP_ROPE,
    GGML_OP_ROPE_BACK,
    GGML_OP_CLAMP,
    GGML_OP_CONV_TRANSPOSE_1D,
    GGML_OP_IM2COL,
    GGML_OP_IM2COL_BACK,
    GGML_OP_CONV_TRANSPOSE_2D,
    GGML_OP_POOL_1D,
    GGML_OP_POOL_2D,
    GGML_OP_POOL_2D_BACK,
    GGML_OP_UPSCALE,
    GGML_OP_PAD,
    GGML_OP_ARANGE,
    GGML_OP_TIMESTEP_EMBEDDING,
    GGML_OP_ARGSORT,
    GGML_OP_LEAKY_RELU,
    GGML_OP_FLASH_ATTN_EXT,
    GGML_OP_FLASH_ATTN_BACK,
    GGML_OP_SSM_CONV,
    GGML_OP_SSM_SCAN,
    GGML_OP_WIN_PART,
    GGML_OP_WIN_UNPART,
    GGML_OP_GET_REL_POS,
    GGML_OP_ADD_REL_POS,
    GGML_OP_RWKV_WKV6,
    GGML_OP_UNARY,
    GGML_OP_MAP_UNARY,
    GGML_OP_MAP_BINARY,
    GGML_OP_MAP_CUSTOM1_F32,
    GGML_OP_MAP_CUSTOM2_F32,
    GGML_OP_MAP_CUSTOM3_F32,
    GGML_OP_MAP_CUSTOM1,
    GGML_OP_MAP_CUSTOM2,
    GGML_OP_MAP_CUSTOM3,
    GGML_OP_CROSS_ENTROPY_LOSS,
    GGML_OP_CROSS_ENTROPY_LOSS_BACK,
    GGML_OP_OPT_STEP_ADAMW,
    GGML_OP_COUNT,
};

// =============================================================================
// GGML STRUCTURES (Opaque)
// =============================================================================

struct ggml_context;
struct ggml_tensor;
struct ggml_cgraph;
struct ggml_backend;
struct ggml_backend_buffer;
struct ggml_backend_buffer_type;
struct ggml_backend_device;
struct ggml_backend_reg;
struct ggml_gallocr;

// =============================================================================
// GGML INIT PARAMS
// =============================================================================

struct ggml_init_params {
    size_t mem_size;   // bytes
    void * mem_buffer; // if NULL, memory will be allocated internally
    bool   no_alloc;   // don't allocate memory for the tensor data
};

// =============================================================================
// GGML BACKEND API
// =============================================================================

// Backend registry
size_t ggml_backend_reg_count(void);
struct ggml_backend_reg * ggml_backend_reg_get(size_t index);
struct ggml_backend_reg * ggml_backend_reg_by_name(const char * name);

// Backend device
size_t ggml_backend_dev_count(void);
struct ggml_backend_device * ggml_backend_dev_get(size_t index);
struct ggml_backend_device * ggml_backend_dev_by_name(const char * name);
struct ggml_backend_device * ggml_backend_dev_by_type(enum ggml_backend_type type);

// Backend initialization
struct ggml_backend * ggml_backend_init_by_name(const char * name, const char * params);
struct ggml_backend * ggml_backend_init_by_type(enum ggml_backend_type type, const char * params);
struct ggml_backend * ggml_backend_init_best(void);

// Backend loading
bool ggml_backend_load(const char * path);
void ggml_backend_unload(struct ggml_backend_reg * reg);
size_t ggml_backend_load_all(void);
size_t ggml_backend_load_all_from_path(const char * path);

// Backend registration
void ggml_backend_register(struct ggml_backend_reg * reg);
void ggml_backend_device_register(struct ggml_backend_device * dev);

// Layla backend (custom)
struct ggml_backend_reg * ggml_backend_reg_layla(void);

// =============================================================================
// GGML CONTEXT API (from libggml-base.so)
// =============================================================================

// Context management
struct ggml_context * ggml_init(struct ggml_init_params params);
void ggml_free(struct ggml_context * ctx);
size_t ggml_used_mem(const struct ggml_context * ctx);

// Tensor creation
struct ggml_tensor * ggml_new_tensor(
        struct ggml_context * ctx,
        enum ggml_type type,
        int n_dims,
        const int64_t * ne);
struct ggml_tensor * ggml_new_tensor_1d(
        struct ggml_context * ctx,
        enum ggml_type type,
        int64_t ne0);
struct ggml_tensor * ggml_new_tensor_2d(
        struct ggml_context * ctx,
        enum ggml_type type,
        int64_t ne0,
        int64_t ne1);
struct ggml_tensor * ggml_new_tensor_3d(
        struct ggml_context * ctx,
        enum ggml_type type,
        int64_t ne0,
        int64_t ne1,
        int64_t ne2);
struct ggml_tensor * ggml_new_tensor_4d(
        struct ggml_context * ctx,
        enum ggml_type type,
        int64_t ne0,
        int64_t ne1,
        int64_t ne2,
        int64_t ne3);

// Tensor operations
struct ggml_tensor * ggml_dup(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_add(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_add_inplace(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_sub(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_mul(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_div(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_sqr(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_sqrt(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_log(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_sin(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_cos(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_sum(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_mean(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_argmax(struct ggml_context * ctx, struct ggml_tensor * a);

// Matrix operations
struct ggml_tensor * ggml_mul_mat(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_out_prod(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);

// Normalization
struct ggml_tensor * ggml_norm(struct ggml_context * ctx, struct ggml_tensor * a, float eps);
struct ggml_tensor * ggml_rms_norm(struct ggml_context * ctx, struct ggml_tensor * a, float eps);
struct ggml_tensor * ggml_group_norm(struct ggml_context * ctx, struct ggml_tensor * a, int n_groups, float eps);

// Activation functions
struct ggml_tensor * ggml_abs(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_relu(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_gelu(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_silu(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_leaky_relu(struct ggml_context * ctx, struct ggml_tensor * a, float negative_slope, bool inplace);

// Attention
struct ggml_tensor * ggml_soft_max(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_flash_attn_ext(
        struct ggml_context * ctx,
        struct ggml_tensor * q,
        struct ggml_tensor * k,
        struct ggml_tensor * v,
        struct ggml_tensor * mask,
        float scale,
        float max_bias,
        float logit_softcap);

// Tensor manipulation
struct ggml_tensor * ggml_reshape(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b);
struct ggml_tensor * ggml_reshape_1d(struct ggml_context * ctx, struct ggml_tensor * a, int64_t ne0);
struct ggml_tensor * ggml_reshape_2d(struct ggml_context * ctx, struct ggml_tensor * a, int64_t ne0, int64_t ne1);
struct ggml_tensor * ggml_reshape_3d(struct ggml_context * ctx, struct ggml_tensor * a, int64_t ne0, int64_t ne1, int64_t ne2);
struct ggml_tensor * ggml_reshape_4d(struct ggml_context * ctx, struct ggml_tensor * a, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3);
struct ggml_tensor * ggml_view_1d(struct ggml_context * ctx, struct ggml_tensor * a, int64_t ne0, size_t offset);
struct ggml_tensor * ggml_view_2d(struct ggml_context * ctx, struct ggml_tensor * a, int64_t ne0, int64_t ne1, size_t nb1, size_t offset);
struct ggml_tensor * ggml_permute(struct ggml_context * ctx, struct ggml_tensor * a, int axis0, int axis1, int axis2, int axis3);
struct ggml_tensor * ggml_transpose(struct ggml_context * ctx, struct ggml_tensor * a);
struct ggml_tensor * ggml_concat(struct ggml_context * ctx, struct ggml_tensor * a, struct ggml_tensor * b, int dim);

// Positional encoding
struct ggml_tensor * ggml_rope(
        struct ggml_context * ctx,
        struct ggml_tensor * a,
        struct ggml_tensor * b,
        int n_dims,
        int mode);
struct ggml_tensor * ggml_arange(struct ggml_context * ctx, float start, float stop, float step);

// Utility
bool ggml_are_same_shape(const struct ggml_tensor * t0, const struct ggml_tensor * t1);
bool ggml_are_same_stride(const struct ggml_tensor * t0, const struct ggml_tensor * t1);
size_t ggml_type_size(enum ggml_type type);
const char * ggml_type_name(enum ggml_type type);
const char * ggml_op_name(enum ggml_op op);
int64_t ggml_nelements(const struct ggml_tensor * tensor);
int64_t ggml_nrows(const struct ggml_tensor * tensor);
size_t ggml_nbytes(const struct ggml_tensor * tensor);
int ggml_n_dims(const struct ggml_tensor * tensor);

// Tensor data access
void * ggml_get_data(const struct ggml_tensor * tensor);
float * ggml_get_data_f32(const struct ggml_tensor * tensor);
void ggml_set_f32(struct ggml_tensor * tensor, float value);
float ggml_get_f32_1d(const struct ggml_tensor * tensor, int i);
void ggml_set_f32_1d(struct ggml_tensor * tensor, int i, float value);

// Graph operations
struct ggml_cgraph * ggml_new_graph(struct ggml_context * ctx);
struct ggml_cgraph * ggml_new_graph_custom(struct ggml_context * ctx, size_t size, bool grads);
void ggml_build_forward_expand(struct ggml_cgraph * cgraph, struct ggml_tensor * tensor);
size_t ggml_graph_n_nodes(struct ggml_cgraph * cgraph);

// =============================================================================
// GGML BACKEND BUFFER API
// =============================================================================

struct ggml_backend_buffer * ggml_backend_alloc_buffer(struct ggml_backend * backend, size_t size);
void ggml_backend_buffer_free(struct ggml_backend_buffer * buffer);
void * ggml_backend_buffer_get_base(struct ggml_backend_buffer * buffer);
size_t ggml_backend_buffer_get_size(struct ggml_backend_buffer * buffer);
const char * ggml_backend_buffer_name(struct ggml_backend_buffer * buffer);
size_t ggml_backend_buffer_get_alignment(struct ggml_backend_buffer * buffer);
bool ggml_backend_buffer_is_host(struct ggml_backend_buffer * buffer);
void ggml_backend_buffer_clear(struct ggml_backend_buffer * buffer, uint8_t value);

// =============================================================================
// GGML SCHEDULER API
// =============================================================================

struct ggml_backend_sched;

struct ggml_backend_sched * ggml_backend_sched_new(
        struct ggml_backend ** backends,
        struct ggml_backend_buffer_type ** bufts,
        int n_backends,
        size_t graph_size,
        bool parallel);
void ggml_backend_sched_free(struct ggml_backend_sched * sched);
bool ggml_backend_sched_reserve(struct ggml_backend_sched * sched, struct ggml_cgraph * measure_graph);
int ggml_backend_sched_get_n_backends(struct ggml_backend_sched * sched);
struct ggml_backend * ggml_backend_sched_get_backend(struct ggml_backend_sched * sched, int i);
bool ggml_backend_sched_alloc_graph(struct ggml_backend_sched * sched, struct ggml_cgraph * graph);
enum ggml_status ggml_backend_sched_graph_compute(struct ggml_backend_sched * sched, struct ggml_cgraph * graph);
void ggml_backend_sched_reset(struct ggml_backend_sched * sched);

// Status codes
enum ggml_status {
    GGML_STATUS_ALLOC_FAILED = -2,
    GGML_STATUS_FAILED       = -1,
    GGML_STATUS_SUCCESS      = 0,
    GGML_STATUS_ABORTED      = 1,
};

// =============================================================================
// MEMORY ALLOCATION
// =============================================================================

void * ggml_aligned_malloc(size_t size);
void ggml_aligned_free(void * ptr);

// =============================================================================
// ABORT HANDLER
// =============================================================================

void ggml_abort(const char * file, int line, const char * fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // GGML_H
