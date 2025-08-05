#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "com_livecoding_demo_LlamaJNI.h"

// Constants for input validation
#define MAX_PROMPT_LENGTH 4096
#define MAX_RESPONSE_LENGTH 8192
#define DEFAULT_MAX_TOKENS 512

// Simple structure to simulate model handle
typedef struct {
    int model_id;
    char model_path[1024];
} simple_model_context;

static simple_model_context* global_model = NULL;

JNIEXPORT jlong JNICALL Java_com_livecoding_demo_LlamaJNI_loadModel(JNIEnv *env, jobject obj, jstring path) {
    if (path == NULL) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Model path cannot be null");
        return 0;
    }

    const char *model_path = (*env)->GetStringUTFChars(env, path, 0);
    if (model_path == NULL) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to get string from JNI");
        return 0;
    }

    // Initialize llama backend
    llama_backend_init();
    
    // Set model parameters
    struct llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0; // CPU only for now
    
    // Load model using new API
    struct llama_model *model = llama_model_load_from_file(model_path, model_params);
    (*env)->ReleaseStringUTFChars(env, path, model_path);
    
    if (model == NULL) {
        llama_backend_free();
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to load model");
        return 0;
    }

    // Set context parameters (no seed parameter in new API)
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_threads = 4;
    
    // Create context using new API
    struct llama_context *ctx = llama_init_from_model(model, ctx_params);
    if (ctx == NULL) {
        llama_model_free(model);
        llama_backend_free();
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to create context");
        return 0;
    }

    // Create sampler chain
    struct llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    struct llama_sampler *sampler = llama_sampler_chain_init(sparams);
    
    if (sampler == NULL) {
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to create sampler");
        return 0;
    }
    
    // Add sampling layers to the chain
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(42));

    // Allocate and populate the model context structure
    llama_model_context *model_ctx = (llama_model_context*)malloc(sizeof(llama_model_context));
    if (model_ctx == NULL) {
        llama_sampler_free(sampler);
        llama_free(ctx);
        llama_model_free(model);
        llama_backend_free();
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to allocate memory for model context");
        return 0;
    }
    
    model_ctx->model = model;
    model_ctx->ctx = ctx;
    model_ctx->sampler = sampler;
    
    return (jlong)model_ctx;
}

JNIEXPORT jstring JNICALL Java_com_livecoding_demo_LlamaJNI_generateText(JNIEnv *env, jobject obj, jlong modelHandle, jstring prompt) {
    if (modelHandle == 0) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Model not loaded");
        return NULL;
    }

    if (prompt == NULL) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Prompt cannot be null");
        return NULL;
    }

    llama_model_context *model_ctx = (llama_model_context*)modelHandle;
    if (model_ctx->model == NULL || model_ctx->ctx == NULL || model_ctx->sampler == NULL) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalStateException"), 
                        "Invalid model state");
        return NULL;
    }

    const char *prompt_text = (*env)->GetStringUTFChars(env, prompt, 0);
    if (prompt_text == NULL) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to get prompt string");
        return NULL;
    }

    // Validate prompt length
    size_t prompt_len = strlen(prompt_text);
    if (prompt_len == 0 || prompt_len > MAX_PROMPT_LENGTH) {
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Invalid prompt length");
        return NULL;
    }

    // Use default max tokens
    int max_gen_tokens = DEFAULT_MAX_TOKENS;

    // Clear memory using new API
    llama_memory_clear(llama_get_memory(model_ctx->ctx), true);

    // Tokenize the prompt
    const int n_ctx = llama_n_ctx(model_ctx->ctx);
    llama_token *tokens = (llama_token*)malloc(n_ctx * sizeof(llama_token));
    if (tokens == NULL) {
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to allocate token buffer");
        return NULL;
    }
    
    const struct llama_vocab * vocab = llama_model_get_vocab(model_ctx->model);
    int n_tokens = llama_tokenize(vocab, prompt_text, prompt_len, tokens, n_ctx, true, true);
    
    (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);

    if (n_tokens < 0) {
        free(tokens);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to tokenize prompt");
        return NULL;
    }

    if (n_tokens >= n_ctx) {
        free(tokens);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Prompt too long for context");
        return NULL;
    }

    // Create batch for prompt evaluation
    struct llama_batch batch = llama_batch_get_one(tokens, n_tokens);

    // Evaluate the prompt
    if (llama_decode(model_ctx->ctx, batch) != 0) {
        free(tokens);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to evaluate prompt");
        return NULL;
    }

    // Generate response
    char *response = (char*)malloc(MAX_RESPONSE_LENGTH);
    if (response == NULL) {
        free(tokens);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to allocate response buffer");
        return NULL;
    }

    int response_pos = 0;
    int generated_tokens = 0;

    for (int i = 0; i < max_gen_tokens && response_pos < MAX_RESPONSE_LENGTH - 256; i++) {
        // Sample next token using the sampler chain
        llama_token next_token = llama_sampler_sample(model_ctx->sampler, model_ctx->ctx, -1);

        // Check for end token
        if (next_token == llama_token_eos(model_ctx->model)) {
            break;
        }

        // Accept the token (updates sampler state)
        llama_sampler_accept(model_ctx->sampler, next_token);

        // Convert token to text using correct API
        char token_str[256];
        int token_len = llama_token_to_piece(vocab, next_token, token_str, sizeof(token_str), 0, false);
        
        if (token_len > 0 && response_pos + token_len < MAX_RESPONSE_LENGTH - 1) {
            memcpy(response + response_pos, token_str, token_len);
            response_pos += token_len;
        }

        // Create batch for single token
        struct llama_batch next_batch = llama_batch_get_one(&next_token, 1);

        // Evaluate the new token
        if (llama_decode(model_ctx->ctx, next_batch) != 0) {
            break;
        }

        generated_tokens++;
    }

    // Null-terminate the response
    response[response_pos] = '\0';

    // Create Java string from response
    jstring result = (*env)->NewStringUTF(env, response);
    free(response);
    free(tokens);

    return result;
}

JNIEXPORT void JNICALL Java_com_livecoding_demo_LlamaJNI_unloadModel(JNIEnv *env, jobject obj, jlong modelHandle) {
    if (modelHandle == 0) {
        return; // Already unloaded or never loaded
    }

    llama_model_context *model_ctx = (llama_model_context*)modelHandle;
    
    if (model_ctx->sampler != NULL) {
        llama_sampler_free(model_ctx->sampler);
    }
    
    if (model_ctx->ctx != NULL) {
        llama_free(model_ctx->ctx);
    }
    
    if (model_ctx->model != NULL) {
        llama_model_free(model_ctx->model);
    }
    
    free(model_ctx);
    llama_backend_free();
}
