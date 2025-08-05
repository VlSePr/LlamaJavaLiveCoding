#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include "com_livecoding_demo_LlamaJNI.h"

#pragma comment(lib, "ws2_32.lib")

// Use HTTP client to communicate with llama-server instead of complex JNI
#define USE_REAL_LLAMA 0
// We'll integrate via HTTP from Java instead

// Constants for input validation
#define MAX_PROMPT_LENGTH 4096
#define MAX_RESPONSE_LENGTH 8192
#define DEFAULT_MAX_TOKENS 512

// Enhanced structure to support both mock and real LLaMA
typedef struct {
    int model_id;
    char model_path[1024];
    
#if USE_REAL_LLAMA
    // For subprocess approach
    HANDLE server_process;
    HANDLE server_thread;
    int server_port;
    WSADATA wsa_data;
#endif
    
    int is_loaded;
} llama_model_context;

static llama_model_context* global_model = NULL;

// Helper function to check if file exists
int file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

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

    // Check if model file exists
    if (!file_exists(model_path)) {
        printf("Model file not found: %s (Enhanced Mock Mode - Simulating Load)\n", model_path);
        // In enhanced mock mode, simulate successful loading even if file doesn't exist
        // This allows testing the application without requiring actual model files
    }

    // Create model context
    llama_model_context *model_ctx = (llama_model_context*)malloc(sizeof(llama_model_context));
    if (model_ctx == NULL) {
        (*env)->ReleaseStringUTFChars(env, path, model_path);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to allocate memory for model context");
        return 0;
    }

    // Initialize the model context
    model_ctx->model_id = 12345;
    strncpy(model_ctx->model_path, model_path, sizeof(model_ctx->model_path) - 1);
    model_ctx->model_path[sizeof(model_ctx->model_path) - 1] = '\0';
    model_ctx->is_loaded = 1;  // Set to loaded for enhanced mock mode

#if USE_REAL_LLAMA
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2,2), &model_ctx->wsa_data) != 0) {
        (*env)->ReleaseStringUTFChars(env, path, model_path);
        free(model_ctx);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to initialize Winsock");
        return 0;
    }
    
    // Real LLaMA using llama-server.exe subprocess approach
    char server_command[2048];
    snprintf(server_command, sizeof(server_command), 
        "\"C:\\Users\\Volodymyr_Prudnikov\\source\\repos\\LLAama\\llama.cpp\\build\\bin\\Release\\llama-server.exe\" "
        "-m \"%s\" "
        "--port 8081 "
        "--ctx-size 2048 "
        "--n-predict 512 "
        "--temp 0.7 "
        "--repeat-penalty 1.1 "
        "--threads 4", 
        model_path);
    
    printf("Starting llama-server with command: %s\n", server_command);
    
    // Start the llama-server process in background
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    if (!CreateProcess(NULL, server_command, NULL, NULL, FALSE, 
                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WSACleanup();
        (*env)->ReleaseStringUTFChars(env, path, model_path);
        free(model_ctx);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to start llama-server process");
        return 0;
    }
    
    // Store process handles
    model_ctx->server_process = pi.hProcess;
    model_ctx->server_thread = pi.hThread;
    model_ctx->server_port = 8081;
    
    // Wait a bit for server to start
    Sleep(5000);
    
    // Test if server is responding
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        WSACleanup();
        (*env)->ReleaseStringUTFChars(env, path, model_path);
        free(model_ctx);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to create socket for server communication");
        return 0;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8081);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(sock);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        WSACleanup();
        (*env)->ReleaseStringUTFChars(env, path, model_path);
        free(model_ctx);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to connect to llama-server - server may not have started properly");
        return 0;
    }
    closesocket(sock);
    
    model_ctx->is_loaded = 1;
    printf("Successfully started llama-server for model: %s\n", model_path);
#else
    model_ctx->is_loaded = 1;
    printf("Mock model loaded from: %s\n", model_path);
#endif
    
    (*env)->ReleaseStringUTFChars(env, path, model_path);
    
    // Store globally for easy access
    global_model = model_ctx;
    
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
    if (model_ctx == NULL || model_ctx->model_id != 12345 || !model_ctx->is_loaded) {
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

    char response[MAX_RESPONSE_LENGTH];

#if USE_REAL_LLAMA
    // Real LLaMA text generation via HTTP API to llama-server
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to create socket for server communication");
        return NULL;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(model_ctx->server_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        closesocket(sock);
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to connect to llama-server");
        return NULL;
    }
    
    // Simple HTTP request for completion
    char http_request[8192];
    snprintf(http_request, sizeof(http_request),
        "POST /completion HTTP/1.1\r\n"
        "Host: 127.0.0.1:8081\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"prompt\":\"%s\",\"n_predict\":256,\"temperature\":0.7}",
        (int)(strlen(prompt_text) + 50), prompt_text);
    
    // Send request and read response
    if (send(sock, http_request, strlen(http_request), 0) == SOCKET_ERROR) {
        closesocket(sock);
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to send request to llama-server");
        return NULL;
    }
    
    // Read response (simplified)
    char http_response[8192];
    int bytes_received = recv(sock, http_response, sizeof(http_response) - 1, 0);
    if (bytes_received > 0) {
        http_response[bytes_received] = '\0';
        // Extract content from JSON response (simplified)
        char* content_start = strstr(http_response, "\"content\":\"");
        if (content_start) {
            content_start += 11;
            char* content_end = strstr(content_start, "\"");
            if (content_end) {
                int len = content_end - content_start;
                if (len < sizeof(response)) {
                    strncpy(response, content_start, len);
                    response[len] = '\0';
                } else {
                    strcpy(response, "Response too long");
                }
            } else {
                strcpy(response, "Could not parse response");
            }
        } else {
            strcpy(response, "No content in response");
        }
    } else {
        strcpy(response, "Failed to receive response");
    }
    
    closesocket(sock);
    printf("Generated real LLaMA response for prompt: %.50s...\n", prompt_text);
    
    // KV cache should be cleared at model level, not separately
    // llama_kv_cache_clear is not available in this API version

    // Tokenize the prompt
    const int n_ctx = llama_n_ctx(model_ctx->ctx);
    tokens = (int*)malloc(n_ctx * sizeof(int));
    if (tokens == NULL) {
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/OutOfMemoryError"), 
                        "Failed to allocate token buffer");
        return NULL;
    }

    // Use the correct tokenize API - it wants vocab instead of model for some versions
    struct llama_vocab * vocab = (struct llama_vocab *)model_ctx->model; // Cast as needed
    int n_tokens = llama_tokenize(vocab, prompt_text, strlen(prompt_text), tokens, n_ctx, 1, 0);
    
    if (n_tokens < 0) {
        free(tokens);
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to tokenize prompt");
        return NULL;
    }

    if (n_tokens >= n_ctx) {
        free(tokens);
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/IllegalArgumentException"), 
                        "Prompt too long for context");
        return NULL;
    }

    // Create batch for prompt evaluation
    struct llama_batch batch = llama_batch_get_one(tokens, n_tokens);

    // Evaluate the prompt
    if (llama_decode(model_ctx->ctx, batch) != 0) {
        free(tokens);
        (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/RuntimeException"), 
                        "Failed to evaluate prompt");
        return NULL;
    }

    // Generate response
    int response_pos = 0;
    int generated_tokens = 0;
    int max_gen_tokens = DEFAULT_MAX_TOKENS;

    for (int i = 0; i < max_gen_tokens && response_pos < MAX_RESPONSE_LENGTH - 256; i++) {
        // Sample next token using the sampler chain
        llama_token next_token = llama_sampler_sample(model_ctx->sampler, model_ctx->ctx, -1);

        // Check for end token
        if (llama_token_is_eog(model_ctx->model, next_token)) {
            break;
        }

        // Accept the token (updates sampler state)
        llama_sampler_accept(model_ctx->sampler, next_token);

        // Convert token to text - use vocab instead of model
        char token_str[256];
        int token_len = llama_token_to_piece(vocab, next_token, token_str, sizeof(token_str), 0, 1);
        
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

    free(tokens);
    response[response_pos] = '\0';
    
    printf("Generated %d tokens for prompt: %.50s...\n", generated_tokens, prompt_text);
    success = 1;
#else
    // Enhanced mock response with realistic variability
    char* selected_response = NULL;
    
    // Add randomization based on prompt content and length
    srand((unsigned int)(prompt_len + time(NULL) + strlen(prompt_text)));
    int variation = rand() % 5;
    
    // Math questions with detailed responses
    if (strstr(prompt_text, "2+2") || strstr(prompt_text, "2 + 2")) {
        const char* math_responses[] = {
            "The answer to 2 + 2 is 4. This is a basic arithmetic operation where we add two numbers together.",
            "2 + 2 equals 4. This is one of the fundamental addition problems in mathematics.",
            "When you add 2 and 2, you get 4. Addition is the process of combining quantities.",
            "The sum of 2 plus 2 is 4. This demonstrates the commutative property of addition.",
            "2 + 2 = 4. This simple addition shows how numbers combine to create larger values."
        };
        selected_response = (char*)math_responses[variation];
    }
    // General math
    else if (strstr(prompt_text, "+") || strstr(prompt_text, "-") || strstr(prompt_text, "*") || strstr(prompt_text, "/") || 
             strstr(prompt_text, "math") || strstr(prompt_text, "calculate")) {
        const char* math_responses[] = {
            "I can help with mathematical calculations. Could you please specify the exact problem you'd like me to solve?",
            "Mathematics is fascinating! What specific calculation or concept would you like me to help you with?",
            "I'm ready to assist with math problems. Please provide the specific equation or question you need help with.",
            "Mathematical problem-solving is one of my strengths. What calculation would you like me to perform?",
            "I can work through various mathematical problems. What specific math question do you have?"
        };
        selected_response = (char*)math_responses[variation];
    }
    // Greetings with personality
    else if (strstr(prompt_text, "Hello") || strstr(prompt_text, "hello") || strstr(prompt_text, "Hi") || strstr(prompt_text, "hi") || 
             strstr(prompt_text, "how are you")) {
        const char* greetings[] = {
            "Hello! I'm doing well, thank you for asking. How can I assist you today?",
            "Hi there! I'm functioning optimally and ready to help. What would you like to know?",
            "Hello! It's great to meet you. I'm here and ready to help with whatever you need.",
            "Hi! I'm operating smoothly and excited to assist you. What's on your mind?",
            "Hello there! I'm in good form today. How may I be of service to you?"
        };
        selected_response = (char*)greetings[variation];
    }
    // Personal introductions
    else if (strstr(prompt_text, "I am") || strstr(prompt_text, "My name is") || strstr(prompt_text, "I'm")) {
        const char* intro_responses[] = {
            "It's nice to meet you! I'm an AI assistant here to help you with various tasks and questions.",
            "Hello! Thanks for introducing yourself. I'm here to assist you with information and problem-solving.",
            "Great to meet you! I'm an AI language model ready to help you with whatever you need.",
            "Nice to make your acquaintance! I'm designed to be helpful, informative, and engaging.",
            "Pleased to meet you! I'm an AI assistant created to help answer questions and provide assistance."
        };
        selected_response = (char*)intro_responses[variation];
    }
    // Questions with depth
    else if (strstr(prompt_text, "what") || strstr(prompt_text, "What") || strstr(prompt_text, "how") || strstr(prompt_text, "How") || 
             strstr(prompt_text, "why") || strstr(prompt_text, "Why") || strstr(prompt_text, "?")) {
        const char* questions[] = {
            "That's an interesting question! Based on my knowledge, this topic has several important aspects to consider.",
            "I understand you're asking about that topic. Let me provide you with a thoughtful response based on available information.",
            "That's a thoughtful inquiry. From what I understand, this is a complex subject with multiple perspectives worth exploring.",
            "Great question! This is something that involves several interconnected concepts that I'd be happy to explain.",
            "You've raised an important point. This topic encompasses various factors that contribute to a comprehensive understanding."
        };
        selected_response = (char*)questions[variation];
    }
    // Default responses with variety
    else {
        const char* defaults[] = {
            "I understand your request. Let me provide you with a helpful response based on the context you've provided.",
            "Thank you for your input. I'll do my best to address what you're asking about in a comprehensive way.",
            "I see what you're asking about. Let me share some insights that might be useful for your inquiry.",
            "I appreciate your message. Based on what you've shared, I can offer some relevant information and perspectives.",
            "That's an interesting point you've raised. I'd be happy to explore this topic with you in more detail."
        };
        selected_response = (char*)defaults[variation];
    }
    
    // Create response with some randomized additional context
    if (rand() % 3 == 0) {
        snprintf(response, sizeof(response), "%s\n\nIs there anything specific about this topic you'd like me to elaborate on?", selected_response);
    } else {
        snprintf(response, sizeof(response), "%s", selected_response);
    }
#endif
    
    (*env)->ReleaseStringUTFChars(env, prompt, prompt_text);
    
    // Create Java string from response
    jstring result = (*env)->NewStringUTF(env, response);
    return result;
}

JNIEXPORT void JNICALL Java_com_livecoding_demo_LlamaJNI_unloadModel(JNIEnv *env, jobject obj, jlong modelHandle) {
    if (modelHandle == 0) {
        return; // Already unloaded or never loaded
    }

    llama_model_context *model_ctx = (llama_model_context*)modelHandle;
    if (model_ctx != NULL) {
        if (global_model == model_ctx) {
            global_model = NULL;
        }
        
#if USE_REAL_LLAMA
        if (model_ctx->is_loaded) {
            if (model_ctx->sampler != NULL) {
                llama_sampler_free(model_ctx->sampler);
            }
            
            if (model_ctx->ctx != NULL) {
                llama_free(model_ctx->ctx);
            }
            
            if (model_ctx->model != NULL) {
                llama_free_model(model_ctx->model);
            }
            
            llama_backend_free();
            printf("LLaMA model unloaded successfully\n");
        }
#endif
        
        free(model_ctx);
    }
}

JNIEXPORT jstring JNICALL Java_com_livecoding_demo_LlamaJNI_getModelInfo(JNIEnv *env, jobject obj, jlong modelHandle) {
    if (modelHandle == 0) {
        return (*env)->NewStringUTF(env, "No model loaded");
    }

    llama_model_context *model_ctx = (llama_model_context*)modelHandle;
    if (model_ctx == NULL || model_ctx->model_id != 12345) {
        return (*env)->NewStringUTF(env, "Invalid model state");
    }

    char info[1024];
    
#if USE_REAL_LLAMA
    if (model_ctx->is_loaded) {
        // Get model metadata
        int n_vocab = llama_n_vocab(model_ctx->model);
        int n_ctx_current = llama_n_ctx(model_ctx->ctx);
        int n_embd = llama_n_embd(model_ctx->model);
        
        snprintf(info, sizeof(info), 
            "Real LLaMA Model - Path: %s, Status: Loaded, "
            "Vocab Size: %d, Context: %d, Embedding Dim: %d", 
            model_ctx->model_path, n_vocab, n_ctx_current, n_embd);
    } else {
        snprintf(info, sizeof(info), 
            "LLaMA Model - Path: %s, Status: Loading Failed", 
            model_ctx->model_path);
    }
#else
    // Enhanced mock mode with realistic response
    const char* filename = strrchr(model_ctx->model_path, '\\');
    if (!filename) filename = strrchr(model_ctx->model_path, '/');
    if (!filename) filename = model_ctx->model_path;
    else filename++; // Skip the slash
    
    snprintf(info, sizeof(info), 
        "Model loaded from: %s", filename);
#endif
    
    return (*env)->NewStringUTF(env, info);
}

JNIEXPORT jboolean JNICALL Java_com_livecoding_demo_LlamaJNI_isModelLoaded(JNIEnv *env, jobject obj, jlong modelHandle) {
    if (modelHandle == 0) {
        return JNI_FALSE;
    }

    llama_model_context *model_ctx = (llama_model_context*)modelHandle;
    return (model_ctx != NULL && model_ctx->model_id == 12345 && model_ctx->is_loaded) ? JNI_TRUE : JNI_FALSE;
}
