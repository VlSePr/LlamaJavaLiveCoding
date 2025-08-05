package com.livecoding.demo;

/**
 * Custom exception for LLaMA-related operations
 */
public class LlamaException extends Exception {

    public LlamaException(String message) {
        super(message);
    }

    public LlamaException(String message, Throwable cause) {
        super(message, cause);
    }

    public LlamaException(Throwable cause) {
        super(cause);
    }
}
