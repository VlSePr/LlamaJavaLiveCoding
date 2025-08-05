#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "com_livecoding_demo_LlamaJNI.h"
#include "llama.h"

// Constants for input validation
#define MAX_PROMPT_LENGTH 4096
#define MAX_RESPONSE_LENGTH 8192
#define DEFAULT_MAX_TOKENS 512

// Structure to hold model, context, and sampler together for thread safety
typedef struct {
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;
} llama_model_context;

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

    // Set context parameters
    struct llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_threads = 4;
    ctx_params.seed = 42;
    
    // Create context
    struct llama_context *ctx = llama_new_context_with_model(model, ctx_params);
    if (ctx == NULL) {
        llama_free_model(model);
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
        llama_free_model(model);
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
        llama_free_model(model);
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

JNIEXPORT jstring JNICALL Java_com_livecoding_demo_LlamaJNI_generateText(JNIEnv *env, jobject obj, jlong modelHandle, jstring prompt, jint maxTokens) {
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

    // Sanitize maxTokens
    int max_gen_tokens = (maxTokens > 0 && maxTokens <= DEFAULT_MAX_TOKENS) ? maxTokens : DEFAULT_MAX_TOKENS;

    // Clear memory using new API
    llama_memory_clear(llama_get_memory(model_ctx->ctx));

    // Tokenize the prompt
    const int n_ctx = llama_n_ctx(model_ctx->ctx);
    int tokens[n_ctx];
    int n_tokens = llama_tokenize(model_ctx->model, prompt_text, strlen(prompt_text), tokens, n_ctx, true, false);
    
    (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);

    if (n_tokens < 0) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to tokenize prompt");
        return NULL;
    }

    if (n_tokens >= n_ctx) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Prompt too long for context");
        return NULL;
    }

    // Create batch for prompt evaluation
    struct llama_batch batch = llama_batch_get_one(tokens, n_tokens);

    // Evaluate the prompt
    if (llama_decode(model_ctx->ctx, batch) != 0) {
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to evaluate prompt");
        return NULL;
    }

    // Generate response
    char *response = (char*)malloc(MAX_RESPONSE_LENGTH);
    if (response == NULL) {
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

        // Convert token to text
        char token_str[256];
        int token_len = llama_token_to_piece(model_ctx->model, next_token, token_str, sizeof(token_str), false, false);
        
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
        llama_free_model(model_ctx->model);
    }
    
    free(model_ctx);
    llama_backend_free();
}
