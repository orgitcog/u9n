/**
 * echo_noi_bridge.cpp - Node.js Native Addon for Noi Integration
 * 
 * This provides the bridge between the C/C++ ML framework and the
 * Electron/Node.js environment of the Noi browser.
 * 
 * Build as a Node.js native addon using node-gyp or prebuild.
 * 
 * Usage from JavaScript:
 *   const echo = require('./echo_ml.node');
 *   echo.init({ vocab_size: 8192, embedding_dim: 128, ... });
 *   const result = echo.forward([1, 2, 3, 4]);  // token IDs
 *   echo.reset();
 */

#include <node_api.h>
#include <string>
#include <cstring>

extern "C" {
#include "echo_ml.h"
}

/* Global engine instance */
static EchoEngine g_engine;
static EchoDict g_dict;
static bool g_initialized = false;

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

static napi_value create_error(napi_env env, const char* message) {
    napi_value error_msg;
    napi_create_string_utf8(env, message, NAPI_AUTO_LENGTH, &error_msg);
    napi_value error;
    napi_create_error(env, nullptr, error_msg, &error);
    return error;
}

static void throw_error(napi_env env, const char* message) {
    napi_throw_error(env, nullptr, message);
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/**
 * Initialize the Echo ML engine.
 * 
 * Arguments:
 *   config: {
 *     vocab_size: number,
 *     embedding_dim: number,
 *     reservoir_size: number,
 *     hidden_dim: number,
 *     output_dim: number,
 *     spectral_radius: number (optional, default 0.9),
 *     reservoir_sparsity: number (optional, default 0.9)
 *   }
 * 
 * Returns: boolean (success)
 */
static napi_value echo_init(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        throw_error(env, "Missing config argument");
        return nullptr;
    }
    
    /* Extract config values */
    EchoConfig config = {0};
    napi_value prop;
    
    #define GET_CONFIG_INT(name) \
        if (napi_get_named_property(env, args[0], #name, &prop) == napi_ok) { \
            int64_t val; \
            napi_get_value_int64(env, prop, &val); \
            config.name = (size_t)val; \
        }
    
    #define GET_CONFIG_FLOAT(name, default_val) \
        config.name = default_val; \
        if (napi_get_named_property(env, args[0], #name, &prop) == napi_ok) { \
            double val; \
            if (napi_get_value_double(env, prop, &val) == napi_ok) { \
                config.name = (echo_float)val; \
            } \
        }
    
    GET_CONFIG_INT(vocab_size);
    GET_CONFIG_INT(embedding_dim);
    GET_CONFIG_INT(reservoir_size);
    GET_CONFIG_INT(hidden_dim);
    GET_CONFIG_INT(output_dim);
    GET_CONFIG_FLOAT(spectral_radius, 0.9f);
    GET_CONFIG_FLOAT(reservoir_sparsity, 0.9f);
    
    #undef GET_CONFIG_INT
    #undef GET_CONFIG_FLOAT
    
    /* Validate config */
    if (config.vocab_size == 0 || config.embedding_dim == 0 ||
        config.reservoir_size == 0 || config.output_dim == 0) {
        throw_error(env, "Invalid config: missing required fields");
        return nullptr;
    }
    
    if (config.hidden_dim == 0) {
        config.hidden_dim = config.reservoir_size;
    }
    
    /* Free existing engine if any */
    if (g_initialized) {
        echo_engine_free(&g_engine);
        g_initialized = false;
    }
    
    /* Initialize engine */
    EchoError err = echo_engine_init(&g_engine, &config);
    if (err != ECHO_OK) {
        throw_error(env, "Failed to initialize engine");
        return nullptr;
    }
    
    g_initialized = true;
    
    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

/**
 * Load model weights from a file.
 * 
 * Arguments:
 *   path: string (path to model file)
 * 
 * Returns: boolean (success)
 */
static napi_value echo_load(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        throw_error(env, "Missing path argument");
        return nullptr;
    }
    
    /* Get path string */
    size_t path_len;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &path_len);
    char* path = new char[path_len + 1];
    napi_get_value_string_utf8(env, args[0], path, path_len + 1, &path_len);
    
    /* Free existing engine */
    if (g_initialized) {
        echo_engine_free(&g_engine);
        g_initialized = false;
    }
    
    /* Load model */
    EchoError err = echo_engine_load(&g_engine, path);
    delete[] path;
    
    if (err != ECHO_OK) {
        throw_error(env, "Failed to load model");
        return nullptr;
    }
    
    g_initialized = true;
    
    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

/**
 * Save model weights to a file.
 * 
 * Arguments:
 *   path: string (path to save model)
 * 
 * Returns: boolean (success)
 */
static napi_value echo_save(napi_env env, napi_callback_info info) {
    if (!g_initialized) {
        throw_error(env, "Engine not initialized");
        return nullptr;
    }
    
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        throw_error(env, "Missing path argument");
        return nullptr;
    }
    
    /* Get path string */
    size_t path_len;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &path_len);
    char* path = new char[path_len + 1];
    napi_get_value_string_utf8(env, args[0], path, path_len + 1, &path_len);
    
    /* Save model */
    EchoError err = echo_engine_save(&g_engine, path);
    delete[] path;
    
    if (err != ECHO_OK) {
        throw_error(env, "Failed to save model");
        return nullptr;
    }
    
    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

/* ============================================================================
 * INFERENCE
 * ============================================================================ */

/**
 * Run inference on a sequence of token IDs.
 * 
 * Arguments:
 *   tokens: number[] (array of token IDs)
 * 
 * Returns: number[] (output vector)
 */
static napi_value echo_forward(napi_env env, napi_callback_info info) {
    if (!g_initialized) {
        throw_error(env, "Engine not initialized");
        return nullptr;
    }
    
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        throw_error(env, "Missing tokens argument");
        return nullptr;
    }
    
    /* Check if argument is array */
    bool is_array;
    napi_is_array(env, args[0], &is_array);
    if (!is_array) {
        throw_error(env, "Tokens must be an array");
        return nullptr;
    }
    
    /* Get array length */
    uint32_t num_tokens;
    napi_get_array_length(env, args[0], &num_tokens);
    
    if (num_tokens == 0) {
        throw_error(env, "Empty token array");
        return nullptr;
    }
    
    /* Extract token IDs */
    echo_token* tokens = new echo_token[num_tokens];
    for (uint32_t i = 0; i < num_tokens; i++) {
        napi_value elem;
        napi_get_element(env, args[0], i, &elem);
        int64_t val;
        napi_get_value_int64(env, elem, &val);
        tokens[i] = (echo_token)val;
    }
    
    /* Allocate output buffer */
    size_t output_dim = g_engine.config.output_dim;
    echo_float* output = new echo_float[output_dim];
    
    /* Run inference */
    echo_engine_forward(output, &g_engine, tokens, num_tokens);
    
    delete[] tokens;
    
    /* Create result array */
    napi_value result;
    napi_create_array_with_length(env, output_dim, &result);
    
    for (size_t i = 0; i < output_dim; i++) {
        napi_value val;
        napi_create_double(env, output[i], &val);
        napi_set_element(env, result, i, val);
    }
    
    delete[] output;
    
    return result;
}

/**
 * Reset the engine's internal state.
 */
static napi_value echo_reset(napi_env env, napi_callback_info info) {
    if (!g_initialized) {
        throw_error(env, "Engine not initialized");
        return nullptr;
    }
    
    echo_engine_reset(&g_engine);
    
    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

/**
 * Free the engine and release resources.
 */
static napi_value echo_free(napi_env env, napi_callback_info info) {
    if (g_initialized) {
        echo_engine_free(&g_engine);
        g_initialized = false;
    }
    
    napi_value result;
    napi_get_boolean(env, true, &result);
    return result;
}

/* ============================================================================
 * DICTIONARY / TOKENIZATION
 * ============================================================================ */

/**
 * Load a dictionary file.
 * 
 * Arguments:
 *   path: string (path to dictionary file, one token per line)
 */
static napi_value echo_load_dict(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        throw_error(env, "Missing path argument");
        return nullptr;
    }
    
    /* Get path string */
    size_t path_len;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &path_len);
    char* path = new char[path_len + 1];
    napi_get_value_string_utf8(env, args[0], path, path_len + 1, &path_len);
    
    /* Free existing dict */
    echo_dict_free(&g_dict);
    
    /* Load dictionary */
    EchoError err = echo_dict_load(&g_dict, path);
    delete[] path;
    
    if (err != ECHO_OK) {
        throw_error(env, "Failed to load dictionary");
        return nullptr;
    }
    
    napi_value result;
    napi_create_int64(env, g_dict.size, &result);
    return result;
}

/**
 * Tokenize a string using the loaded dictionary.
 * 
 * Arguments:
 *   text: string
 * 
 * Returns: number[] (array of token IDs)
 */
static napi_value echo_tokenize(napi_env env, napi_callback_info info) {
    if (g_dict.size == 0) {
        throw_error(env, "Dictionary not loaded");
        return nullptr;
    }
    
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    
    if (argc < 1) {
        throw_error(env, "Missing text argument");
        return nullptr;
    }
    
    /* Get text string */
    size_t text_len;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &text_len);
    char* text = new char[text_len + 1];
    napi_get_value_string_utf8(env, args[0], text, text_len + 1, &text_len);
    
    /* Tokenize */
    const size_t max_tokens = 1024;
    echo_token* tokens = new echo_token[max_tokens];
    size_t num_tokens = ::echo_tokenize(tokens, max_tokens, &g_dict, text);
    
    delete[] text;
    
    /* Create result array */
    napi_value result;
    napi_create_array_with_length(env, num_tokens, &result);
    
    for (size_t i = 0; i < num_tokens; i++) {
        napi_value val;
        napi_create_int64(env, tokens[i], &val);
        napi_set_element(env, result, i, val);
    }
    
    delete[] tokens;
    
    return result;
}

/**
 * Get engine configuration info.
 */
static napi_value echo_get_config(napi_env env, napi_callback_info info) {
    if (!g_initialized) {
        throw_error(env, "Engine not initialized");
        return nullptr;
    }
    
    napi_value result;
    napi_create_object(env, &result);
    
    #define SET_PROP_INT(name) { \
        napi_value val; \
        napi_create_int64(env, g_engine.config.name, &val); \
        napi_set_named_property(env, result, #name, val); \
    }
    
    #define SET_PROP_FLOAT(name) { \
        napi_value val; \
        napi_create_double(env, g_engine.config.name, &val); \
        napi_set_named_property(env, result, #name, val); \
    }
    
    SET_PROP_INT(vocab_size);
    SET_PROP_INT(embedding_dim);
    SET_PROP_INT(reservoir_size);
    SET_PROP_INT(hidden_dim);
    SET_PROP_INT(output_dim);
    SET_PROP_FLOAT(spectral_radius);
    SET_PROP_FLOAT(reservoir_sparsity);
    
    #undef SET_PROP_INT
    #undef SET_PROP_FLOAT
    
    return result;
}

/* ============================================================================
 * MODULE INITIALIZATION
 * ============================================================================ */

static napi_value init(napi_env env, napi_value exports) {
    /* Define module methods */
    napi_property_descriptor props[] = {
        { "init", nullptr, echo_init, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "load", nullptr, echo_load, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "save", nullptr, echo_save, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "forward", nullptr, echo_forward, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "reset", nullptr, echo_reset, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "free", nullptr, echo_free, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "loadDict", nullptr, echo_load_dict, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "tokenize", nullptr, echo_tokenize, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getConfig", nullptr, echo_get_config, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    
    napi_define_properties(env, exports, sizeof(props) / sizeof(props[0]), props);
    
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
