// llama.h - llama.cpp C API header for CGO bindings
// This header defines the llama.cpp API needed for Deep Tree Echo inference
// Based on llama.h from llama.cpp project

#ifndef LLAMA_H
#define LLAMA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// LLAMA TYPES
// =============================================================================

typedef int32_t llama_pos;
typedef int32_t llama_token;
typedef int32_t llama_seq_id;

// =============================================================================
// LLAMA STRUCTURES (Opaque)
// =============================================================================

struct llama_model;
struct llama_context;
struct llama_sampler;
struct llama_vocab;
struct llama_kv_cache;

// =============================================================================
// LLAMA ENUMS
// =============================================================================

enum llama_vocab_type {
    LLAMA_VOCAB_TYPE_NONE = 0,
    LLAMA_VOCAB_TYPE_SPM  = 1,
    LLAMA_VOCAB_TYPE_BPE  = 2,
    LLAMA_VOCAB_TYPE_WPM  = 3,
    LLAMA_VOCAB_TYPE_UGM  = 4,
    LLAMA_VOCAB_TYPE_RWKV = 5,
};

enum llama_rope_type {
    LLAMA_ROPE_TYPE_NONE = -1,
    LLAMA_ROPE_TYPE_NORM =  0,
    LLAMA_ROPE_TYPE_NEOX =  2,
};

enum llama_token_attr {
    LLAMA_TOKEN_ATTR_UNDEFINED    = 0,
    LLAMA_TOKEN_ATTR_UNKNOWN      = 1 << 0,
    LLAMA_TOKEN_ATTR_UNUSED       = 1 << 1,
    LLAMA_TOKEN_ATTR_NORMAL       = 1 << 2,
    LLAMA_TOKEN_ATTR_CONTROL      = 1 << 3,
    LLAMA_TOKEN_ATTR_USER_DEFINED = 1 << 4,
    LLAMA_TOKEN_ATTR_BYTE         = 1 << 5,
    LLAMA_TOKEN_ATTR_NORMALIZED   = 1 << 6,
    LLAMA_TOKEN_ATTR_LSTRIP       = 1 << 7,
    LLAMA_TOKEN_ATTR_RSTRIP       = 1 << 8,
    LLAMA_TOKEN_ATTR_SINGLE_WORD  = 1 << 9,
};

enum llama_ftype {
    LLAMA_FTYPE_ALL_F32              = 0,
    LLAMA_FTYPE_MOSTLY_F16           = 1,
    LLAMA_FTYPE_MOSTLY_Q4_0          = 2,
    LLAMA_FTYPE_MOSTLY_Q4_1          = 3,
    LLAMA_FTYPE_MOSTLY_Q8_0          = 7,
    LLAMA_FTYPE_MOSTLY_Q5_0          = 8,
    LLAMA_FTYPE_MOSTLY_Q5_1          = 9,
    LLAMA_FTYPE_MOSTLY_Q2_K          = 10,
    LLAMA_FTYPE_MOSTLY_Q3_K_S        = 11,
    LLAMA_FTYPE_MOSTLY_Q3_K_M        = 12,
    LLAMA_FTYPE_MOSTLY_Q3_K_L        = 13,
    LLAMA_FTYPE_MOSTLY_Q4_K_S        = 14,
    LLAMA_FTYPE_MOSTLY_Q4_K_M        = 15,
    LLAMA_FTYPE_MOSTLY_Q5_K_S        = 16,
    LLAMA_FTYPE_MOSTLY_Q5_K_M        = 17,
    LLAMA_FTYPE_MOSTLY_Q6_K          = 18,
    LLAMA_FTYPE_MOSTLY_IQ2_XXS       = 19,
    LLAMA_FTYPE_MOSTLY_IQ2_XS        = 20,
    LLAMA_FTYPE_MOSTLY_Q2_K_S        = 21,
    LLAMA_FTYPE_MOSTLY_IQ3_XS        = 22,
    LLAMA_FTYPE_MOSTLY_IQ3_XXS       = 23,
    LLAMA_FTYPE_MOSTLY_IQ1_S         = 24,
    LLAMA_FTYPE_MOSTLY_IQ4_NL        = 25,
    LLAMA_FTYPE_MOSTLY_IQ3_S         = 26,
    LLAMA_FTYPE_MOSTLY_IQ3_M         = 27,
    LLAMA_FTYPE_MOSTLY_IQ2_S         = 28,
    LLAMA_FTYPE_MOSTLY_IQ2_M         = 29,
    LLAMA_FTYPE_MOSTLY_IQ4_XS        = 30,
    LLAMA_FTYPE_MOSTLY_IQ1_M         = 31,
    LLAMA_FTYPE_MOSTLY_BF16          = 32,
    LLAMA_FTYPE_MOSTLY_Q4_0_4_4      = 33,
    LLAMA_FTYPE_MOSTLY_Q4_0_4_8      = 34,
    LLAMA_FTYPE_MOSTLY_Q4_0_8_8      = 35,
    LLAMA_FTYPE_MOSTLY_TQ1_0         = 36,
    LLAMA_FTYPE_MOSTLY_TQ2_0         = 37,
};

enum llama_rope_scaling_type {
    LLAMA_ROPE_SCALING_TYPE_UNSPECIFIED = -1,
    LLAMA_ROPE_SCALING_TYPE_NONE        = 0,
    LLAMA_ROPE_SCALING_TYPE_LINEAR      = 1,
    LLAMA_ROPE_SCALING_TYPE_YARN        = 2,
    LLAMA_ROPE_SCALING_TYPE_MAX_VALUE   = LLAMA_ROPE_SCALING_TYPE_YARN,
};

enum llama_pooling_type {
    LLAMA_POOLING_TYPE_UNSPECIFIED = -1,
    LLAMA_POOLING_TYPE_NONE = 0,
    LLAMA_POOLING_TYPE_MEAN = 1,
    LLAMA_POOLING_TYPE_CLS  = 2,
    LLAMA_POOLING_TYPE_LAST = 3,
    LLAMA_POOLING_TYPE_RANK = 4,
};

enum llama_attention_type {
    LLAMA_ATTENTION_TYPE_UNSPECIFIED = -1,
    LLAMA_ATTENTION_TYPE_CAUSAL      = 0,
    LLAMA_ATTENTION_TYPE_NON_CAUSAL  = 1,
};

enum llama_split_mode {
    LLAMA_SPLIT_MODE_NONE  = 0,
    LLAMA_SPLIT_MODE_LAYER = 1,
    LLAMA_SPLIT_MODE_ROW   = 2,
};

// =============================================================================
// LLAMA PARAMETERS
// =============================================================================

struct llama_model_params {
    int32_t n_gpu_layers;
    enum llama_split_mode split_mode;
    int32_t main_gpu;
    const float * tensor_split;
    const char * rpc_servers;
    void (*progress_callback)(float progress, void * user_data);
    void * progress_callback_user_data;
    const struct ggml_backend ** kv_overrides;
    bool vocab_only;
    bool use_mmap;
    bool use_mlock;
    bool check_tensors;
};

struct llama_context_params {
    uint32_t n_ctx;
    uint32_t n_batch;
    uint32_t n_ubatch;
    uint32_t n_seq_max;
    int32_t n_threads;
    int32_t n_threads_batch;
    enum llama_rope_scaling_type rope_scaling_type;
    enum llama_pooling_type pooling_type;
    enum llama_attention_type attention_type;
    float rope_freq_base;
    float rope_freq_scale;
    float yarn_ext_factor;
    float yarn_attn_factor;
    float yarn_beta_fast;
    float yarn_beta_slow;
    uint32_t yarn_orig_ctx;
    float defrag_thold;
    void (*cb_eval)(struct ggml_tensor * t, bool ask, void * user_data);
    void * cb_eval_user_data;
    int32_t type_k;
    int32_t type_v;
    bool logits_all;
    bool embeddings;
    bool offload_kqv;
    bool flash_attn;
    bool no_perf;
    void (*abort_callback)(void * data);
    void * abort_callback_data;
};

// =============================================================================
// LLAMA BATCH
// =============================================================================

struct llama_batch {
    int32_t n_tokens;
    llama_token  * token;
    float        * embd;
    llama_pos    * pos;
    int32_t      * n_seq_id;
    llama_seq_id ** seq_id;
    int8_t       * logits;
};

// =============================================================================
// LLAMA TOKEN DATA
// =============================================================================

struct llama_token_data {
    llama_token id;
    float logit;
    float p;
};

struct llama_token_data_array {
    struct llama_token_data * data;
    size_t size;
    int64_t selected;
    bool sorted;
};

// =============================================================================
// LLAMA CHAT MESSAGE
// =============================================================================

struct llama_chat_message {
    const char * role;
    const char * content;
};

// =============================================================================
// LLAMA BACKEND API
// =============================================================================

void llama_backend_init(void);
void llama_backend_free(void);

// =============================================================================
// LLAMA MODEL API
// =============================================================================

struct llama_model_params llama_model_default_params(void);

struct llama_model * llama_model_load_from_file(
        const char * path_model,
        struct llama_model_params params);

struct llama_model * llama_load_model_from_file(
        const char * path_model,
        struct llama_model_params params);

void llama_model_free(struct llama_model * model);
void llama_free_model(struct llama_model * model);

// Model info
int32_t llama_model_n_ctx_train(const struct llama_model * model);
int32_t llama_model_n_embd(const struct llama_model * model);
int32_t llama_model_n_layer(const struct llama_model * model);
int32_t llama_model_n_head(const struct llama_model * model);
int32_t llama_model_n_vocab(const struct llama_model * model);
enum llama_rope_type llama_model_rope_type(const struct llama_model * model);
float llama_model_rope_freq_scale_train(const struct llama_model * model);
bool llama_model_has_encoder(const struct llama_model * model);
bool llama_model_has_decoder(const struct llama_model * model);
bool llama_model_is_recurrent(const struct llama_model * model);

// Model metadata
int32_t llama_model_meta_count(const struct llama_model * model);
int32_t llama_model_meta_key_by_index(const struct llama_model * model, int32_t i, char * buf, size_t buf_size);
int32_t llama_model_meta_val_str(const struct llama_model * model, const char * key, char * buf, size_t buf_size);
int32_t llama_model_meta_val_str_by_index(const struct llama_model * model, int32_t i, char * buf, size_t buf_size);
int32_t llama_model_desc(const struct llama_model * model, char * buf, size_t buf_size);

// Chat template
const char * llama_model_chat_template(const struct llama_model * model, const char * name);
int32_t llama_chat_apply_template(
        const char * tmpl,
        const struct llama_chat_message * chat,
        size_t n_msg,
        bool add_ass,
        char * buf,
        int32_t length);

// =============================================================================
// LLAMA CONTEXT API
// =============================================================================

struct llama_context_params llama_context_default_params(void);

struct llama_context * llama_init_from_model(
        struct llama_model * model,
        struct llama_context_params params);

void llama_free(struct llama_context * ctx);

// Context info
const struct llama_model * llama_get_model(const struct llama_context * ctx);
uint32_t llama_n_ctx(const struct llama_context * ctx);
uint32_t llama_n_batch(const struct llama_context * ctx);
uint32_t llama_n_ubatch(const struct llama_context * ctx);
uint32_t llama_n_seq_max(const struct llama_context * ctx);

// =============================================================================
// LLAMA VOCAB API
// =============================================================================

const struct llama_vocab * llama_model_get_vocab(const struct llama_model * model);

int32_t llama_vocab_n_tokens(const struct llama_vocab * vocab);
enum llama_vocab_type llama_vocab_type(const struct llama_vocab * vocab);
bool llama_vocab_is_normal(const struct llama_vocab * vocab, llama_token token);
bool llama_vocab_is_unknown(const struct llama_vocab * vocab, llama_token token);
bool llama_vocab_is_control(const struct llama_vocab * vocab, llama_token token);
bool llama_vocab_is_byte(const struct llama_vocab * vocab, llama_token token);
bool llama_vocab_is_eog(const struct llama_vocab * vocab, llama_token token);

llama_token llama_vocab_bos(const struct llama_vocab * vocab);
llama_token llama_vocab_eos(const struct llama_vocab * vocab);
llama_token llama_vocab_eot(const struct llama_vocab * vocab);
llama_token llama_vocab_sep(const struct llama_vocab * vocab);
llama_token llama_vocab_nl(const struct llama_vocab * vocab);
llama_token llama_vocab_pad(const struct llama_vocab * vocab);

bool llama_add_bos_token(const struct llama_model * model);
bool llama_add_eos_token(const struct llama_model * model);

// =============================================================================
// LLAMA TOKENIZATION API
// =============================================================================

int32_t llama_tokenize(
        const struct llama_vocab * vocab,
        const char * text,
        int32_t text_len,
        llama_token * tokens,
        int32_t n_tokens_max,
        bool add_special,
        bool parse_special);

int32_t llama_token_to_piece(
        const struct llama_vocab * vocab,
        llama_token token,
        char * buf,
        int32_t length,
        int32_t lstrip,
        bool special);

int32_t llama_detokenize(
        const struct llama_vocab * vocab,
        const llama_token * tokens,
        int32_t n_tokens,
        char * text,
        int32_t text_len_max,
        bool remove_special,
        bool unparse_special);

// =============================================================================
// LLAMA BATCH API
// =============================================================================

struct llama_batch llama_batch_init(int32_t n_tokens, int32_t embd, int32_t n_seq_max);
void llama_batch_free(struct llama_batch batch);
struct llama_batch llama_batch_get_one(llama_token * tokens, int32_t n_tokens);

// =============================================================================
// LLAMA DECODE API
// =============================================================================

int32_t llama_encode(struct llama_context * ctx, struct llama_batch batch);
int32_t llama_decode(struct llama_context * ctx, struct llama_batch batch);

// =============================================================================
// LLAMA LOGITS AND EMBEDDINGS
// =============================================================================

float * llama_get_logits(struct llama_context * ctx);
float * llama_get_logits_ith(struct llama_context * ctx, int32_t i);
float * llama_get_embeddings(struct llama_context * ctx);
float * llama_get_embeddings_ith(struct llama_context * ctx, int32_t i);
float * llama_get_embeddings_seq(struct llama_context * ctx, llama_seq_id seq_id);

// =============================================================================
// LLAMA KV CACHE API
// =============================================================================

struct llama_kv_cache * llama_get_kv_self(struct llama_context * ctx);

void llama_kv_cache_clear(struct llama_context * ctx);
void llama_kv_self_clear(struct llama_context * ctx);

bool llama_kv_cache_seq_rm(struct llama_context * ctx, llama_seq_id seq_id, llama_pos p0, llama_pos p1);
bool llama_kv_self_seq_rm(struct llama_context * ctx, llama_seq_id seq_id, llama_pos p0, llama_pos p1);

void llama_kv_cache_seq_cp(struct llama_context * ctx, llama_seq_id seq_id_src, llama_seq_id seq_id_dst, llama_pos p0, llama_pos p1);
void llama_kv_self_seq_cp(struct llama_context * ctx, llama_seq_id seq_id_src, llama_seq_id seq_id_dst, llama_pos p0, llama_pos p1);

void llama_kv_cache_seq_keep(struct llama_context * ctx, llama_seq_id seq_id);
void llama_kv_self_seq_keep(struct llama_context * ctx, llama_seq_id seq_id);

void llama_kv_cache_seq_add(struct llama_context * ctx, llama_seq_id seq_id, llama_pos p0, llama_pos p1, llama_pos delta);
void llama_kv_self_seq_add(struct llama_context * ctx, llama_seq_id seq_id, llama_pos p0, llama_pos p1, llama_pos delta);

void llama_kv_cache_seq_div(struct llama_context * ctx, llama_seq_id seq_id, llama_pos p0, llama_pos p1, int d);
void llama_kv_self_seq_div(struct llama_context * ctx, llama_seq_id seq_id, llama_pos p0, llama_pos p1, int d);

llama_pos llama_kv_cache_seq_pos_max(struct llama_context * ctx, llama_seq_id seq_id);
llama_pos llama_kv_self_seq_pos_max(struct llama_context * ctx, llama_seq_id seq_id);

void llama_kv_cache_defrag(struct llama_context * ctx);
void llama_kv_self_defrag(struct llama_context * ctx);

void llama_kv_cache_update(struct llama_context * ctx);
void llama_kv_self_update(struct llama_context * ctx);

bool llama_kv_cache_can_shift(struct llama_context * ctx);
bool llama_kv_self_can_shift(struct llama_context * ctx);

int32_t llama_get_kv_cache_token_count(const struct llama_context * ctx);
int32_t llama_get_kv_cache_used_cells(const struct llama_context * ctx);
int32_t llama_kv_self_n_tokens(const struct llama_context * ctx);
int32_t llama_kv_self_used_cells(const struct llama_context * ctx);

// =============================================================================
// LLAMA STATE API
// =============================================================================

size_t llama_get_state_size(struct llama_context * ctx);
size_t llama_copy_state_data(struct llama_context * ctx, uint8_t * dst);
size_t llama_set_state_data(struct llama_context * ctx, const uint8_t * src);

bool llama_load_session_file(
        struct llama_context * ctx,
        const char * path_session,
        llama_token * tokens_out,
        size_t n_token_capacity,
        size_t * n_token_count_out);

bool llama_save_session_file(
        struct llama_context * ctx,
        const char * path_session,
        const llama_token * tokens,
        size_t n_token_count);

// =============================================================================
// LLAMA SAMPLER API
// =============================================================================

struct llama_sampler * llama_sampler_init_greedy(void);
struct llama_sampler * llama_sampler_init_dist(uint32_t seed);
struct llama_sampler * llama_sampler_init_top_k(int32_t k);
struct llama_sampler * llama_sampler_init_top_p(float p, size_t min_keep);
struct llama_sampler * llama_sampler_init_min_p(float p, size_t min_keep);
struct llama_sampler * llama_sampler_init_typical(float p, size_t min_keep);
struct llama_sampler * llama_sampler_init_temp(float t);
struct llama_sampler * llama_sampler_init_temp_ext(float t, float delta, float exponent);
struct llama_sampler * llama_sampler_init_mirostat(int32_t n_vocab, uint32_t seed, float tau, float eta, int32_t m);
struct llama_sampler * llama_sampler_init_mirostat_v2(uint32_t seed, float tau, float eta);
struct llama_sampler * llama_sampler_init_grammar(const struct llama_vocab * vocab, const char * grammar_str, const char * grammar_root);
struct llama_sampler * llama_sampler_init_penalties(
        int32_t penalty_last_n,
        float penalty_repeat,
        float penalty_freq,
        float penalty_present);
struct llama_sampler * llama_sampler_init_logit_bias(
        int32_t n_vocab,
        int32_t n_logit_bias,
        const llama_token * token,
        const float * bias);

void llama_sampler_free(struct llama_sampler * smpl);
void llama_sampler_reset(struct llama_sampler * smpl);
struct llama_sampler * llama_sampler_clone(const struct llama_sampler * smpl);
const char * llama_sampler_name(const struct llama_sampler * smpl);
void llama_sampler_accept(struct llama_sampler * smpl, llama_token token);
void llama_sampler_apply(struct llama_sampler * smpl, struct llama_token_data_array * cur_p);
llama_token llama_sampler_sample(struct llama_sampler * smpl, struct llama_context * ctx, int32_t idx);

// Sampler chain
struct llama_sampler * llama_sampler_chain_init(struct llama_sampler_chain_params params);
void llama_sampler_chain_add(struct llama_sampler * chain, struct llama_sampler * smpl);
struct llama_sampler * llama_sampler_chain_get(const struct llama_sampler * chain, int32_t i);
int32_t llama_sampler_chain_n(const struct llama_sampler * chain);
struct llama_sampler * llama_sampler_chain_remove(struct llama_sampler * chain, int32_t i);

struct llama_sampler_chain_params {
    bool no_perf;
};

struct llama_sampler_chain_params llama_sampler_chain_default_params(void);

// =============================================================================
// LLAMA THREADING API
// =============================================================================

void llama_attach_threadpool(
        struct llama_context * ctx,
        void * threadpool,
        void * threadpool_batch);

void llama_detach_threadpool(struct llama_context * ctx);

// =============================================================================
// LLAMA LORA API
// =============================================================================

struct llama_adapter_lora * llama_adapter_lora_init(
        struct llama_model * model,
        const char * path_lora);

void llama_adapter_lora_free(struct llama_adapter_lora * adapter);

int32_t llama_set_adapter_lora(
        struct llama_context * ctx,
        struct llama_adapter_lora * adapter,
        float scale);

int32_t llama_rm_adapter_lora(
        struct llama_context * ctx,
        struct llama_adapter_lora * adapter);

void llama_clear_adapter_lora(struct llama_context * ctx);

// =============================================================================
// LLAMA CONTROL VECTOR API
// =============================================================================

int32_t llama_apply_adapter_cvec(
        struct llama_context * ctx,
        const float * data,
        size_t len,
        int32_t n_embd,
        int32_t il_start,
        int32_t il_end);

// =============================================================================
// LLAMA LOGGING API
// =============================================================================

typedef void (*llama_log_callback)(int level, const char * text, void * user_data);

void llama_log_set(llama_log_callback log_callback, void * user_data);

// =============================================================================
// LLAMA MISC
// =============================================================================

int32_t llama_max_devices(void);

#ifdef __cplusplus
}
#endif

#endif // LLAMA_H
