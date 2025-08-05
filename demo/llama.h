// Minimal llama.h header for real LLaMA integration
#ifndef LLAMA_H
#define LLAMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct llama_model llama_model;
typedef struct llama_context llama_context;
typedef struct llama_sampler llama_sampler;
typedef int32_t llama_token;
typedef uint32_t llama_seq_id;

// Model and context parameters
struct llama_model_params {
    int32_t n_gpu_layers;
    bool    use_mmap;
    bool    use_mlock;
    void *  kv_overrides;
};

struct llama_context_params {
    uint32_t seed;
    uint32_t n_ctx;
    uint32_t n_batch;
    uint32_t n_ubatch;
    uint32_t n_seq_max;
    uint32_t n_threads;
    uint32_t n_threads_batch;
    bool     embeddings;
    bool     offload_kqv;
    bool     flash_attn;
    bool     no_perf;
};

// Batch structure
struct llama_batch {
    int32_t n_tokens;
    llama_token * token;
    float * embd;
    llama_seq_id * seq_id;
    int32_t * pos;
    int8_t * logits;
    bool all_pos_0;
    bool all_pos_1;
    bool all_seq_id;
};

// Function declarations
llama_model * llama_load_model_from_file(const char * path_model, struct llama_model_params params);
void llama_free_model(llama_model * model);

llama_context * llama_new_context_with_model(llama_model * model, struct llama_context_params params);
void llama_free(llama_context * ctx);

struct llama_model_params llama_model_default_params(void);
struct llama_context_params llama_context_default_params(void);

int llama_n_ctx(const llama_context * ctx);
int llama_n_vocab(const llama_model * model);

// Tokenization
int llama_tokenize(const llama_model * model, const char * text, int text_len, llama_token * tokens, int n_tokens_max, bool add_special, bool parse_special);
int llama_token_to_piece(const llama_model * model, llama_token token, char * buf, int length, int lstrip, bool special);

// Token checking
bool llama_token_is_eog(const llama_model * model, llama_token token);

// Batch operations
struct llama_batch llama_batch_get_one(llama_token * tokens, int32_t n_tokens);
void llama_batch_free(struct llama_batch batch);

// Inference
int llama_decode(llama_context * ctx, struct llama_batch batch);

// Sampling
llama_sampler * llama_sampler_chain_init(struct llama_sampler_chain_params params);
void llama_sampler_chain_add(llama_sampler * chain, llama_sampler * smpl);
llama_sampler * llama_sampler_init_temp(float temp);
llama_sampler * llama_sampler_init_top_p(float p, size_t min_keep);
llama_sampler * llama_sampler_init_top_k(int32_t k);

llama_token llama_sampler_sample(llama_sampler * smpl, llama_context * ctx, int32_t idx);
void llama_sampler_accept(llama_sampler * smpl, llama_token token);
void llama_sampler_free(llama_sampler * smpl);

struct llama_sampler_chain_params {
    bool no_perf;
};

struct llama_sampler_chain_params llama_sampler_chain_default_params(void);

// Utility functions
void llama_backend_init(void);
void llama_backend_free(void);

#ifdef __cplusplus
}
#endif

#endif // LLAMA_H
