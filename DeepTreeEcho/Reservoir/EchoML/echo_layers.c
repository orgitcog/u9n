/**
 * echo_layers.c - Embedding, Dense Layers, Dictionary, and Main Engine
 * 
 * This file implements:
 * - Embedding layer (learnable lookup table)
 * - Dense (fully connected) layer with activations
 * - Dictionary/tokenization system
 * - Main EchoEngine orchestrator
 */

#define _DEFAULT_SOURCE 1  /* enable strdup and other POSIX extensions */
#include "echo_ml.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ============================================================================
 * EMBEDDING LAYER
 * ============================================================================ */

EchoError echo_embedding_init(EchoEmbedding* e, size_t vocab_size, size_t embedding_dim) {
    e->vocab_size = vocab_size;
    e->embedding_dim = embedding_dim;
    
    size_t shape[] = {vocab_size, embedding_dim};
    EchoError err = echo_tensor_alloc(&e->weights, shape, 2);
    if (err != ECHO_OK) return err;
    
    /* Initialize with scaled random values (Xavier initialization) */
    echo_float scale = sqrtf(2.0f / (vocab_size + embedding_dim));
    size_t n = vocab_size * embedding_dim;
    for (size_t i = 0; i < n; i++) {
        e->weights.data[i] = echo_randn() * scale;
    }
    
    return ECHO_OK;
}

void echo_embedding_free(EchoEmbedding* e) {
    echo_tensor_free(&e->weights);
}

/**
 * Lookup embeddings for a sequence of tokens.
 * 
 * output: [num_tokens, embedding_dim]
 * tokens: [num_tokens]
 */
void echo_embedding_forward(echo_float* output, const EchoEmbedding* e,
                            const echo_token* tokens, size_t num_tokens) {
    size_t dim = e->embedding_dim;
    
    for (size_t i = 0; i < num_tokens; i++) {
        echo_token tok = tokens[i];
        if (tok >= e->vocab_size) tok = 0;  /* Unknown token -> index 0 */
        
        /* Copy embedding vector */
        memcpy(output + i * dim, 
               e->weights.data + tok * dim,
               dim * sizeof(echo_float));
    }
}

/* ============================================================================
 * DENSE LAYER
 * ============================================================================ */

EchoError echo_dense_init(EchoDense* d, size_t in_features, size_t out_features,
                          EchoActivation activation, bool use_bias) {
    d->in_features = in_features;
    d->out_features = out_features;
    d->activation = activation;
    d->use_bias = use_bias;
    
    /* Allocate weights: [out_features, in_features] */
    size_t w_shape[] = {out_features, in_features};
    EchoError err = echo_tensor_alloc(&d->weights, w_shape, 2);
    if (err != ECHO_OK) return err;
    
    /* Initialize weights (He initialization for ReLU, Xavier for others) */
    echo_float scale;
    if (activation == ECHO_ACT_RELU || activation == ECHO_ACT_LEAKY_RELU) {
        scale = sqrtf(2.0f / in_features);
    } else {
        scale = sqrtf(2.0f / (in_features + out_features));
    }
    
    size_t n = in_features * out_features;
    for (size_t i = 0; i < n; i++) {
        d->weights.data[i] = echo_randn() * scale;
    }
    
    /* Allocate bias if needed */
    if (use_bias) {
        size_t b_shape[] = {out_features};
        err = echo_tensor_alloc(&d->bias, b_shape, 1);
        if (err != ECHO_OK) {
            echo_tensor_free(&d->weights);
            return err;
        }
        echo_tensor_fill(&d->bias, 0);
    }
    
    return ECHO_OK;
}

void echo_dense_free(EchoDense* d) {
    echo_tensor_free(&d->weights);
    if (d->use_bias) {
        echo_tensor_free(&d->bias);
    }
}

/**
 * Apply activation function in-place.
 */
static void apply_activation(echo_float* data, size_t n, EchoActivation act) {
    switch (act) {
        case ECHO_ACT_NONE:
            break;
        case ECHO_ACT_TANH:
            echo_vec_tanh(data, data, n);
            break;
        case ECHO_ACT_RELU:
            echo_vec_relu(data, data, n);
            break;
        case ECHO_ACT_SIGMOID:
            echo_vec_sigmoid(data, data, n);
            break;
        case ECHO_ACT_LEAKY_RELU:
            for (size_t i = 0; i < n; i++) {
                if (data[i] < 0) data[i] *= 0.01f;
            }
            break;
        case ECHO_ACT_SOFTMAX: {
            /* Find max for numerical stability */
            echo_float max_val = data[0];
            for (size_t i = 1; i < n; i++) {
                if (data[i] > max_val) max_val = data[i];
            }
            /* Compute exp and sum */
            echo_float sum = 0;
            for (size_t i = 0; i < n; i++) {
                data[i] = expf(data[i] - max_val);
                sum += data[i];
            }
            /* Normalize */
            echo_float inv_sum = 1.0f / sum;
            for (size_t i = 0; i < n; i++) {
                data[i] *= inv_sum;
            }
            break;
        }
    }
}

/**
 * Dense layer forward pass.
 * output = activation(weights @ input + bias)
 */
void echo_dense_forward(echo_float* output, const EchoDense* d, const echo_float* input) {
    /* output = weights @ input */
    echo_gemv(output, d->weights.data, input, d->out_features, d->in_features);
    
    /* Add bias */
    if (d->use_bias) {
        echo_vec_add(output, output, d->bias.data, d->out_features);
    }
    
    /* Apply activation */
    apply_activation(output, d->out_features, d->activation);
}

/**
 * Batched dense layer forward pass.
 */
void echo_dense_forward_batch(echo_float* output, const EchoDense* d,
                              const echo_float* input, size_t batch_size) {
    for (size_t b = 0; b < batch_size; b++) {
        echo_dense_forward(output + b * d->out_features,
                          d,
                          input + b * d->in_features);
    }
}

/* ============================================================================
 * DICTIONARY / TOKENIZATION
 * ============================================================================ */

EchoError echo_dict_load(EchoDict* dict, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return ECHO_ERR_FILE;
    
    /* Count lines */
    size_t count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) count++;
    
    rewind(f);
    
    /* Allocate */
    dict->tokens = (char**)malloc(count * sizeof(char*));
    dict->ids = (echo_token*)malloc(count * sizeof(echo_token));
    if (!dict->tokens || !dict->ids) {
        fclose(f);
        return ECHO_ERR_ALLOC;
    }
    
    dict->size = 0;
    dict->capacity = count;
    
    /* Read tokens */
    while (fgets(line, sizeof(line), f)) {
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (len > 1 && line[len-2] == '\r') line[len-2] = '\0';
        
        /* Store token */
        dict->tokens[dict->size] = strdup(line);
        dict->ids[dict->size] = dict->size;
        dict->size++;
    }
    
    fclose(f);
    return ECHO_OK;
}

void echo_dict_free(EchoDict* dict) {
    if (dict->tokens) {
        for (size_t i = 0; i < dict->size; i++) {
            free(dict->tokens[i]);
        }
        free(dict->tokens);
    }
    free(dict->ids);
    dict->tokens = NULL;
    dict->ids = NULL;
    dict->size = 0;
}

/**
 * Lookup a token in the dictionary.
 * Returns 0 (unknown token) if not found.
 */
echo_token echo_dict_lookup(const EchoDict* dict, const char* token) {
    /* Linear search for now - could use hash table for larger dicts */
    for (size_t i = 0; i < dict->size; i++) {
        if (strcmp(dict->tokens[i], token) == 0) {
            return dict->ids[i];
        }
    }
    return 0;  /* Unknown token */
}

const char* echo_dict_get_token(const EchoDict* dict, echo_token id) {
    if (id >= dict->size) return "<UNK>";
    return dict->tokens[id];
}

/**
 * Simple whitespace tokenizer.
 * Returns number of tokens produced.
 */
size_t echo_tokenize(echo_token* output, size_t max_tokens,
                     const EchoDict* dict, const char* text) {
    size_t count = 0;
    char* text_copy = strdup(text);
    char* token = strtok(text_copy, " \t\n\r");
    
    while (token && count < max_tokens) {
        output[count++] = echo_dict_lookup(dict, token);
        token = strtok(NULL, " \t\n\r");
    }
    
    free(text_copy);
    return count;
}

/* ============================================================================
 * MEMORY-MAPPED FILE LOADING
 * ============================================================================ */

EchoError echo_mmap_open(EchoMmap* m, const char* path) {
    m->fd = open(path, O_RDONLY);
    if (m->fd < 0) return ECHO_ERR_FILE;
    
    struct stat st;
    if (fstat(m->fd, &st) < 0) {
        close(m->fd);
        return ECHO_ERR_FILE;
    }
    
    m->size = st.st_size;
    m->data = mmap(NULL, m->size, PROT_READ, MAP_PRIVATE, m->fd, 0);
    if (m->data == MAP_FAILED) {
        close(m->fd);
        return ECHO_ERR_FILE;
    }
    
    return ECHO_OK;
}

void echo_mmap_close(EchoMmap* m) {
    if (m->data && m->data != MAP_FAILED) {
        munmap(m->data, m->size);
    }
    if (m->fd >= 0) {
        close(m->fd);
    }
    m->data = NULL;
    m->fd = -1;
}

/* ============================================================================
 * ECHO ENGINE - Main Inference Orchestrator
 * ============================================================================ */

EchoError echo_engine_init(EchoEngine* engine, const EchoConfig* config) {
    engine->config = *config;
    engine->initialized = false;
    
    /* Initialize embedding layer */
    EchoError err = echo_embedding_init(&engine->embedding, 
                                        config->vocab_size, 
                                        config->embedding_dim);
    if (err != ECHO_OK) return err;
    
    /* Initialize reservoir */
    err = echo_reservoir_init(&engine->reservoir,
                              config->reservoir_size,
                              config->embedding_dim,
                              config->hidden_dim,
                              config->spectral_radius,
                              config->reservoir_sparsity);
    if (err != ECHO_OK) {
        echo_embedding_free(&engine->embedding);
        return err;
    }
    
    /* Initialize output layer */
    err = echo_dense_init(&engine->output_layer,
                          config->hidden_dim,
                          config->output_dim,
                          ECHO_ACT_NONE,
                          true);
    if (err != ECHO_OK) {
        echo_embedding_free(&engine->embedding);
        echo_reservoir_free(&engine->reservoir);
        return err;
    }
    
    /* Allocate working buffers */
    engine->embed_buffer = (echo_float*)aligned_alloc(32, 
        config->embedding_dim * sizeof(echo_float));
    engine->reservoir_output = (echo_float*)aligned_alloc(32,
        config->hidden_dim * sizeof(echo_float));
    
    if (!engine->embed_buffer || !engine->reservoir_output) {
        echo_engine_free(engine);
        return ECHO_ERR_ALLOC;
    }
    
    engine->initialized = true;
    return ECHO_OK;
}

void echo_engine_free(EchoEngine* engine) {
    if (!engine->initialized) return;
    
    echo_embedding_free(&engine->embedding);
    echo_reservoir_free(&engine->reservoir);
    echo_dense_free(&engine->output_layer);
    
    free(engine->embed_buffer);
    free(engine->reservoir_output);
    
    engine->initialized = false;
}

/**
 * Save engine weights to a binary file.
 * 
 * File format:
 * - Header: magic (4 bytes), version (4 bytes), config (sizeof EchoConfig)
 * - Embedding weights
 * - Reservoir W_in, W_out, W_res (sparse)
 * - Output layer weights and bias
 */
EchoError echo_engine_save(const EchoEngine* engine, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return ECHO_ERR_FILE;
    
    /* Write header */
    uint32_t magic = 0x4543484F;  /* "ECHO" */
    uint32_t version = 1;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&engine->config, sizeof(EchoConfig), 1, f);
    
    /* Write embedding weights */
    size_t embed_size = engine->config.vocab_size * engine->config.embedding_dim;
    fwrite(engine->embedding.weights.data, sizeof(echo_float), embed_size, f);
    
    /* Write reservoir W_in */
    size_t w_in_size = engine->config.reservoir_size * engine->config.embedding_dim;
    fwrite(engine->reservoir.W_in.data, sizeof(echo_float), w_in_size, f);
    
    /* Write reservoir W_out */
    size_t w_out_size = engine->config.hidden_dim * engine->config.reservoir_size;
    fwrite(engine->reservoir.W_out.data, sizeof(echo_float), w_out_size, f);
    
    /* Write reservoir W_res (sparse) */
    fwrite(&engine->reservoir.W_res.nnz, sizeof(size_t), 1, f);
    fwrite(engine->reservoir.W_res.values, sizeof(echo_float), 
           engine->reservoir.W_res.nnz, f);
    fwrite(engine->reservoir.W_res.col_indices, sizeof(uint32_t),
           engine->reservoir.W_res.nnz, f);
    fwrite(engine->reservoir.W_res.row_ptr, sizeof(uint32_t),
           engine->config.reservoir_size + 1, f);
    
    /* Write output layer */
    size_t out_w_size = engine->config.output_dim * engine->config.hidden_dim;
    fwrite(engine->output_layer.weights.data, sizeof(echo_float), out_w_size, f);
    fwrite(engine->output_layer.bias.data, sizeof(echo_float), 
           engine->config.output_dim, f);
    
    fclose(f);
    return ECHO_OK;
}

/**
 * Load engine weights from a binary file.
 */
EchoError echo_engine_load(EchoEngine* engine, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return ECHO_ERR_FILE;
    
    /* Read and verify header */
    uint32_t magic = 0, version = 0;
    EchoConfig config;
    if (fread(&magic, sizeof(magic), 1, f) != 1 ||
        fread(&version, sizeof(version), 1, f) != 1 ||
        fread(&config, sizeof(EchoConfig), 1, f) != 1) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    if (magic != 0x4543484F || version != 1) {
        fclose(f);
        return ECHO_ERR_INVALID;
    }
    
    /* Initialize engine with loaded config */
    EchoError err = echo_engine_init(engine, &config);
    if (err != ECHO_OK) {
        fclose(f);
        return err;
    }
    
    /* Read embedding weights */
    size_t embed_size = config.vocab_size * config.embedding_dim;
    if (fread(engine->embedding.weights.data, sizeof(echo_float), embed_size, f) != embed_size) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    /* Read reservoir W_in */
    size_t w_in_size = config.reservoir_size * config.embedding_dim;
    if (fread(engine->reservoir.W_in.data, sizeof(echo_float), w_in_size, f) != w_in_size) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    /* Read reservoir W_out */
    size_t w_out_size = config.hidden_dim * config.reservoir_size;
    if (fread(engine->reservoir.W_out.data, sizeof(echo_float), w_out_size, f) != w_out_size) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    /* Read reservoir W_res (sparse) - need to reallocate */
    size_t nnz = 0;
    if (fread(&nnz, sizeof(size_t), 1, f) != 1) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    echo_sparse_free(&engine->reservoir.W_res);
    echo_sparse_alloc(&engine->reservoir.W_res, config.reservoir_size, 
                      config.reservoir_size, nnz);
    
    if (fread(engine->reservoir.W_res.values, sizeof(echo_float), nnz, f) != nnz ||
        fread(engine->reservoir.W_res.col_indices, sizeof(uint32_t), nnz, f) != nnz ||
        fread(engine->reservoir.W_res.row_ptr, sizeof(uint32_t), config.reservoir_size + 1, f) != config.reservoir_size + 1) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    /* Read output layer */
    size_t out_w_size = config.output_dim * config.hidden_dim;
    if (fread(engine->output_layer.weights.data, sizeof(echo_float), out_w_size, f) != out_w_size ||
        fread(engine->output_layer.bias.data, sizeof(echo_float), config.output_dim, f) != config.output_dim) {
        fclose(f);
        return ECHO_ERR_FILE;
    }
    
    fclose(f);
    return ECHO_OK;
}

/**
 * Main inference function.
 * 
 * Process a sequence of tokens through the engine:
 * 1. Embed tokens
 * 2. Process through reservoir (accumulates temporal dynamics)
 * 3. Project to output
 */
void echo_engine_forward(echo_float* output, EchoEngine* engine,
                         const echo_token* tokens, size_t num_tokens) {
    /* Process each token through the pipeline */
    for (size_t i = 0; i < num_tokens; i++) {
        /* Embed single token */
        echo_embedding_forward(engine->embed_buffer, &engine->embedding,
                              &tokens[i], 1);
        
        /* Process through reservoir */
        echo_reservoir_step(engine->reservoir_output, &engine->reservoir,
                           engine->embed_buffer);
    }
    
    /* Final output projection */
    echo_dense_forward(output, &engine->output_layer, engine->reservoir_output);
}

void echo_engine_reset(EchoEngine* engine) {
    echo_reservoir_reset(&engine->reservoir);
}
