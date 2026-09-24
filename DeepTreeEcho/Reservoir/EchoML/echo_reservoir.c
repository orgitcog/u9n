/**
 * echo_reservoir.c - Echo State Network (Reservoir Computing) Implementation
 * 
 * This implements the core reservoir computing functionality for Deep Tree Echo.
 * The reservoir provides temporal dynamics and memory through recurrent connections.
 * 
 * Key features:
 * - Sparse reservoir matrix for efficiency
 * - Spectral radius normalization for stability
 * - Leaky integration for controllable dynamics
 * - Support for 3 concurrent streams (echobeats architecture)
 */

#include "echo_ml.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * Compute the spectral radius of a sparse matrix using power iteration.
 * This is used to normalize the reservoir weights for stability.
 */
static echo_float compute_spectral_radius(const EchoSparseMatrix* W, size_t max_iter) {
    size_t n = W->rows;
    
    /* Allocate vectors for power iteration */
    echo_float* v = (echo_float*)aligned_alloc(32, n * sizeof(echo_float));
    echo_float* v_new = (echo_float*)aligned_alloc(32, n * sizeof(echo_float));
    
    /* Initialize with random vector */
    for (size_t i = 0; i < n; i++) {
        v[i] = echo_randn();
    }
    
    /* Normalize */
    echo_float norm = sqrtf(echo_vec_dot(v, v, n));
    echo_vec_scale(v, v, 1.0f / norm, n);
    
    echo_float eigenvalue = 0;
    
    for (size_t iter = 0; iter < max_iter; iter++) {
        /* v_new = W @ v */
        echo_sparse_matvec(v_new, W, v);
        
        /* Compute eigenvalue estimate */
        eigenvalue = echo_vec_dot(v, v_new, n);
        
        /* Normalize v_new */
        norm = sqrtf(echo_vec_dot(v_new, v_new, n));
        if (norm < 1e-10f) break;
        
        echo_vec_scale(v, v_new, 1.0f / norm, n);
    }
    
    free(v);
    free(v_new);
    
    return fabsf(eigenvalue);
}

/**
 * Generate a sparse random matrix with given sparsity.
 * Returns the matrix in CSR format.
 */
static EchoError generate_sparse_random(EchoSparseMatrix* W, size_t n, 
                                        echo_float sparsity, echo_float target_sr) {
    /* Estimate number of non-zeros */
    size_t expected_nnz = (size_t)(n * n * (1.0f - sparsity));
    if (expected_nnz < n) expected_nnz = n;  /* At least one per row on average */
    
    /* Temporary dense storage for generation */
    echo_float* dense = (echo_float*)calloc(n * n, sizeof(echo_float));
    if (!dense) return ECHO_ERR_ALLOC;
    
    /* Fill with sparse random values */
    size_t actual_nnz = 0;
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (echo_rand() > sparsity) {
                dense[i * n + j] = echo_randn();
                actual_nnz++;
            }
        }
    }
    
    /* Allocate sparse matrix */
    EchoError err = echo_sparse_alloc(W, n, n, actual_nnz);
    if (err != ECHO_OK) {
        free(dense);
        return err;
    }
    
    /* Convert to CSR */
    size_t idx = 0;
    for (size_t i = 0; i < n; i++) {
        W->row_ptr[i] = idx;
        for (size_t j = 0; j < n; j++) {
            if (dense[i * n + j] != 0) {
                W->values[idx] = dense[i * n + j];
                W->col_indices[idx] = j;
                idx++;
            }
        }
    }
    W->row_ptr[n] = idx;
    
    free(dense);
    
    /* Normalize to target spectral radius */
    echo_float sr = compute_spectral_radius(W, 100);
    if (sr > 1e-10f) {
        echo_float scale = target_sr / sr;
        for (size_t i = 0; i < W->nnz; i++) {
            W->values[i] *= scale;
        }
    }
    
    return ECHO_OK;
}

/* ============================================================================
 * RESERVOIR IMPLEMENTATION
 * ============================================================================ */

EchoError echo_reservoir_init(EchoReservoir* r, size_t reservoir_size,
                              size_t input_dim, size_t output_dim,
                              echo_float spectral_radius, echo_float sparsity) {
    r->reservoir_size = reservoir_size;
    r->input_dim = input_dim;
    r->output_dim = output_dim;
    r->spectral_radius = spectral_radius;
    r->leak_rate = 0.3f;  /* Default leak rate */
    r->input_scaling = 1.0f;
    
    /* Generate sparse reservoir matrix */
    EchoError err = generate_sparse_random(&r->W_res, reservoir_size, 
                                           sparsity, spectral_radius);
    if (err != ECHO_OK) return err;
    
    /* Allocate input weights: [reservoir_size, input_dim] */
    size_t w_in_shape[] = {reservoir_size, input_dim};
    err = echo_tensor_alloc(&r->W_in, w_in_shape, 2);
    if (err != ECHO_OK) {
        echo_sparse_free(&r->W_res);
        return err;
    }
    
    /* Initialize input weights with scaled random values */
    size_t w_in_size = reservoir_size * input_dim;
    for (size_t i = 0; i < w_in_size; i++) {
        r->W_in.data[i] = echo_randn() * 0.1f;
    }
    
    /* Allocate output weights: [output_dim, reservoir_size] */
    size_t w_out_shape[] = {output_dim, reservoir_size};
    err = echo_tensor_alloc(&r->W_out, w_out_shape, 2);
    if (err != ECHO_OK) {
        echo_sparse_free(&r->W_res);
        echo_tensor_free(&r->W_in);
        return err;
    }
    
    /* Initialize output weights (will be trained) */
    echo_tensor_fill(&r->W_out, 0);
    
    /* Allocate state vector */
    size_t state_shape[] = {reservoir_size};
    err = echo_tensor_alloc(&r->state, state_shape, 1);
    if (err != ECHO_OK) {
        echo_sparse_free(&r->W_res);
        echo_tensor_free(&r->W_in);
        echo_tensor_free(&r->W_out);
        return err;
    }
    
    echo_reservoir_reset(r);
    
    return ECHO_OK;
}

void echo_reservoir_free(EchoReservoir* r) {
    echo_sparse_free(&r->W_res);
    echo_tensor_free(&r->W_in);
    echo_tensor_free(&r->W_out);
    echo_tensor_free(&r->state);
}

void echo_reservoir_reset(EchoReservoir* r) {
    echo_tensor_fill(&r->state, 0);
}

/**
 * Single reservoir step with leaky integration.
 * 
 * state_new = (1 - leak_rate) * state + leak_rate * tanh(W_res @ state + W_in @ input)
 * output = W_out @ state_new
 */
void echo_reservoir_step(echo_float* output, EchoReservoir* r, const echo_float* input) {
    size_t n = r->reservoir_size;
    
    /* Allocate temporary buffers on stack for small reservoirs */
    echo_float* pre_activation = (echo_float*)aligned_alloc(32, n * sizeof(echo_float));
    echo_float* new_state = (echo_float*)aligned_alloc(32, n * sizeof(echo_float));
    
    /* Compute W_res @ state */
    echo_sparse_matvec(pre_activation, &r->W_res, r->state.data);
    
    /* Add W_in @ input */
    for (size_t i = 0; i < n; i++) {
        echo_float input_contrib = 0;
        for (size_t j = 0; j < r->input_dim; j++) {
            input_contrib += r->W_in.data[i * r->input_dim + j] * input[j] * r->input_scaling;
        }
        pre_activation[i] += input_contrib;
    }
    
    /* Apply tanh activation */
    echo_vec_tanh(new_state, pre_activation, n);
    
    /* Leaky integration */
    echo_float alpha = r->leak_rate;
    echo_float one_minus_alpha = 1.0f - alpha;
    for (size_t i = 0; i < n; i++) {
        r->state.data[i] = one_minus_alpha * r->state.data[i] + alpha * new_state[i];
    }
    
    /* Compute output: W_out @ state */
    echo_gemv(output, r->W_out.data, r->state.data, r->output_dim, n);
    
    free(pre_activation);
    free(new_state);
}

/**
 * Process a sequence through the reservoir.
 * 
 * inputs: [seq_len, input_dim]
 * outputs: [seq_len, output_dim]
 */
void echo_reservoir_forward(echo_float* outputs, EchoReservoir* r,
                            const echo_float* inputs, size_t seq_len) {
    for (size_t t = 0; t < seq_len; t++) {
        const echo_float* input_t = inputs + t * r->input_dim;
        echo_float* output_t = outputs + t * r->output_dim;
        echo_reservoir_step(output_t, r, input_t);
    }
}

/* ============================================================================
 * ECHOBEATS: 3 CONCURRENT RESERVOIR STREAMS
 * ============================================================================ */

/**
 * EchoBeats structure for 3 concurrent cognitive streams.
 * Implements the 12-step cognitive loop with 120-degree phase offsets.
 */
typedef struct {
    EchoReservoir streams[3];   /* Three concurrent reservoirs */
    
    /* Coupling weights between streams */
    EchoTensor coupling_01;     /* Stream 0 -> Stream 1 */
    EchoTensor coupling_12;     /* Stream 1 -> Stream 2 */
    EchoTensor coupling_20;     /* Stream 2 -> Stream 0 */
    
    /* Current step in the 12-step cycle */
    uint8_t current_step;
    
    /* Configuration */
    size_t reservoir_size;
    size_t input_dim;
    size_t output_dim;
    echo_float coupling_strength;
} EchoBeats;

EchoError echo_beats_init(EchoBeats* eb, size_t reservoir_size,
                          size_t input_dim, size_t output_dim,
                          echo_float spectral_radius, echo_float sparsity) {
    eb->reservoir_size = reservoir_size;
    eb->input_dim = input_dim;
    eb->output_dim = output_dim;
    eb->coupling_strength = 0.1f;
    eb->current_step = 0;
    
    /* Initialize three reservoir streams */
    for (int i = 0; i < 3; i++) {
        EchoError err = echo_reservoir_init(&eb->streams[i], reservoir_size,
                                            input_dim, output_dim,
                                            spectral_radius, sparsity);
        if (err != ECHO_OK) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                echo_reservoir_free(&eb->streams[j]);
            }
            return err;
        }
    }
    
    /* Initialize coupling weights */
    size_t coupling_shape[] = {reservoir_size, reservoir_size};
    
    EchoError err = echo_tensor_alloc(&eb->coupling_01, coupling_shape, 2);
    if (err != ECHO_OK) goto cleanup;
    
    err = echo_tensor_alloc(&eb->coupling_12, coupling_shape, 2);
    if (err != ECHO_OK) goto cleanup;
    
    err = echo_tensor_alloc(&eb->coupling_20, coupling_shape, 2);
    if (err != ECHO_OK) goto cleanup;
    
    /* Initialize coupling with small random values */
    size_t n = reservoir_size * reservoir_size;
    for (size_t i = 0; i < n; i++) {
        eb->coupling_01.data[i] = echo_randn() * 0.01f;
        eb->coupling_12.data[i] = echo_randn() * 0.01f;
        eb->coupling_20.data[i] = echo_randn() * 0.01f;
    }
    
    return ECHO_OK;
    
cleanup:
    for (int i = 0; i < 3; i++) {
        echo_reservoir_free(&eb->streams[i]);
    }
    echo_tensor_free(&eb->coupling_01);
    echo_tensor_free(&eb->coupling_12);
    echo_tensor_free(&eb->coupling_20);
    return err;
}

void echo_beats_free(EchoBeats* eb) {
    for (int i = 0; i < 3; i++) {
        echo_reservoir_free(&eb->streams[i]);
    }
    echo_tensor_free(&eb->coupling_01);
    echo_tensor_free(&eb->coupling_12);
    echo_tensor_free(&eb->coupling_20);
}

void echo_beats_reset(EchoBeats* eb) {
    for (int i = 0; i < 3; i++) {
        echo_reservoir_reset(&eb->streams[i]);
    }
    eb->current_step = 0;
}

/**
 * Single step of the EchoBeats 12-step cognitive loop.
 * 
 * The three streams are phased 4 steps apart (120 degrees):
 * - Stream 0: steps {0, 3, 6, 9}  - Perception
 * - Stream 1: steps {1, 4, 7, 10} - Action
 * - Stream 2: steps {2, 5, 8, 11} - Simulation
 * 
 * Each stream receives coupling from the previous stream's state.
 */
void echo_beats_step(echo_float* outputs, EchoBeats* eb, const echo_float* input) {
    size_t n = eb->reservoir_size;
    
    /* Determine which stream is active based on current step */
    int active_stream = eb->current_step % 3;
    
    /* Compute coupling influence from previous stream */
    echo_float* coupling_input = (echo_float*)aligned_alloc(32, n * sizeof(echo_float));
    int prev_stream = (active_stream + 2) % 3;
    
    EchoTensor* coupling;
    switch (active_stream) {
        case 0: coupling = &eb->coupling_20; break;
        case 1: coupling = &eb->coupling_01; break;
        case 2: coupling = &eb->coupling_12; break;
    }
    
    /* coupling_input = coupling @ prev_stream.state */
    echo_gemv(coupling_input, coupling->data, 
              eb->streams[prev_stream].state.data, n, n);
    
    /* Scale coupling */
    echo_vec_scale(coupling_input, coupling_input, eb->coupling_strength, n);
    
    /* Add coupling to current stream's state (modulates dynamics) */
    echo_vec_add(eb->streams[active_stream].state.data,
                 eb->streams[active_stream].state.data,
                 coupling_input, n);
    
    /* Run the active stream */
    echo_reservoir_step(outputs + active_stream * eb->output_dim,
                        &eb->streams[active_stream], input);
    
    /* Copy other streams' last outputs (they don't update this step) */
    for (int i = 0; i < 3; i++) {
        if (i != active_stream) {
            /* Just copy the current state projection */
            echo_gemv(outputs + i * eb->output_dim,
                     eb->streams[i].W_out.data,
                     eb->streams[i].state.data,
                     eb->output_dim, n);
        }
    }
    
    /* Advance step counter */
    eb->current_step = (eb->current_step + 1) % 12;
    
    free(coupling_input);
}

/**
 * Process a sequence through EchoBeats.
 * 
 * inputs: [seq_len, input_dim]
 * outputs: [seq_len, 3 * output_dim] (concatenated outputs from all 3 streams)
 */
void echo_beats_forward(echo_float* outputs, EchoBeats* eb,
                        const echo_float* inputs, size_t seq_len) {
    size_t output_stride = 3 * eb->output_dim;
    
    for (size_t t = 0; t < seq_len; t++) {
        const echo_float* input_t = inputs + t * eb->input_dim;
        echo_float* output_t = outputs + t * output_stride;
        echo_beats_step(output_t, eb, input_t);
    }
}
