package com.livecoding.demo;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import jakarta.annotation.PreDestroy;
import jakarta.annotation.PostConstruct;

import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

@Service
public class LlamaService {
    private final LlamaJNIInterface llamaJNI;
    private volatile long modelHandle = 0;
    private final ReentrantReadWriteLock modelLock = new ReentrantReadWriteLock();
    private final Semaphore generateSemaphore = new Semaphore(5); // Limit concurrent generations

    @Value("${llama.model.path:Llama-3.2-3B-Instruct-Q3_K_L.gguf}")
    private String modelPath;

    @Value("${llama.max.prompt.length:4000}")
    private int maxPromptLength;

    @Value("${llama.generation.timeout.seconds:30}")
    private int generationTimeoutSeconds;

    // Pattern to remove potentially harmful content
    private static final Pattern SANITIZE_PATTERN = Pattern.compile("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]");

    // Constructor for dependency injection (allows for testing)
    public LlamaService(LlamaJNIInterface llamaJNI) {
        this.llamaJNI = llamaJNI;
    }

    // Default constructor for Spring
    public LlamaService() {
        this.llamaJNI = new LlamaJNI();
    }

    @PostConstruct
    public void initialize() {
        // Don't load model at startup - load on first request instead
        // This prevents startup failures if model file is not available
    }

    public String generateText(String prompt) throws LlamaException {
        // Input validation
        validatePrompt(prompt);

        // Sanitize input
        String sanitizedPrompt = sanitizePrompt(prompt);

        try {
            // Acquire generation permit (limit concurrent generations)
            if (!generateSemaphore.tryAcquire(generationTimeoutSeconds, TimeUnit.SECONDS)) {
                throw new LlamaException("Generation timeout: too many concurrent requests");
            }

            try {
                // Ensure model is loaded
                loadModelIfNeeded();

                // Use read lock for generation (allows multiple concurrent reads)
                modelLock.readLock().lock();
                try {
                    if (modelHandle == 0) {
                        throw new LlamaException("Model not loaded");
                    }
                    return llamaJNI.generateText(modelHandle, sanitizedPrompt);
                } finally {
                    modelLock.readLock().unlock();
                }
            } finally {
                generateSemaphore.release();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new LlamaException("Generation interrupted", e);
        } catch (Exception e) {
            if (e instanceof LlamaException) {
                throw e;
            }
            throw new LlamaException("Generation failed: " + e.getMessage(), e);
        }
    }

    private void loadModelIfNeeded() throws LlamaException {
        if (modelHandle == 0) {
            modelLock.writeLock().lock();
            try {
                // Double-check pattern
                if (modelHandle == 0) {
                    try {
                        modelHandle = llamaJNI.loadModel(modelPath);
                        if (modelHandle == 0) {
                            throw new LlamaException("Failed to load model from path: " + modelPath);
                        }
                    } catch (Exception e) {
                        throw new LlamaException("Failed to load model: " + e.getMessage(), e);
                    }
                }
            } finally {
                modelLock.writeLock().unlock();
            }
        }
    }

    private void validatePrompt(String prompt) throws LlamaException {
        if (prompt == null) {
            throw new LlamaException("Prompt cannot be null");
        }

        if (prompt.trim().isEmpty()) {
            throw new LlamaException("Prompt cannot be empty");
        }

        if (prompt.length() > maxPromptLength) {
            throw new LlamaException("Prompt too long. Maximum length: " + maxPromptLength + " characters");
        }

        // Check for potential security issues
        if (containsSuspiciousContent(prompt)) {
            throw new LlamaException("Prompt contains potentially harmful content");
        }
    }

    private String sanitizePrompt(String prompt) {
        if (prompt == null) {
            return "";
        }

        // Remove control characters
        String sanitized = SANITIZE_PATTERN.matcher(prompt).replaceAll("");

        // Trim whitespace
        sanitized = sanitized.trim();

        // Limit line breaks
        sanitized = sanitized.replaceAll("\\n{3,}", "\n\n");

        return sanitized;
    }

    private boolean containsSuspiciousContent(String prompt) {
        // Basic checks for potentially harmful content
        String lowerPrompt = prompt.toLowerCase();

        // Check for obvious injection attempts
        String[] suspiciousPatterns = {
                "system(",
                "exec(",
                "eval(",
                "\\x",
                "\\.dll",
                "\\.exe",
                "cmd.exe",
                "powershell"
        };

        for (String pattern : suspiciousPatterns) {
            if (lowerPrompt.contains(pattern)) {
                return true;
            }
        }

        return false;
    }

    public boolean isModelLoaded() {
        // First check without loading
        modelLock.readLock().lock();
        try {
            if (modelHandle != 0) {
                // Check with JNI layer for accurate status
                return llamaJNI.isModelLoaded(modelHandle);
            }
        } finally {
            modelLock.readLock().unlock();
        }

        // Try to load model if not already loaded
        try {
            loadModelIfNeeded();
            modelLock.readLock().lock();
            try {
                if (modelHandle != 0) {
                    return llamaJNI.isModelLoaded(modelHandle);
                }
                return false;
            } finally {
                modelLock.readLock().unlock();
            }
        } catch (LlamaException e) {
            // If loading fails, model is not loaded
            return false;
        }
    }

    public String getModelStatus() {
        // First check without loading
        modelLock.readLock().lock();
        try {
            if (modelHandle != 0) {
                // Get detailed status from JNI layer
                return llamaJNI.getModelInfo(modelHandle);
            }
        } finally {
            modelLock.readLock().unlock();
        }

        // Try to load model if not already loaded
        try {
            loadModelIfNeeded();
            modelLock.readLock().lock();
            try {
                if (modelHandle != 0) {
                    return llamaJNI.getModelInfo(modelHandle);
                }
                return "Model not loaded";
            } finally {
                modelLock.readLock().unlock();
            }
        } catch (LlamaException e) {
            // If loading fails, return error status
            return "Model not loaded: " + e.getMessage();
        }
    }

    @PreDestroy
    public void cleanup() {
        modelLock.writeLock().lock();
        try {
            if (modelHandle != 0) {
                try {
                    llamaJNI.unloadModel(modelHandle);
                } catch (Exception e) {
                    // Log error but don't throw during shutdown
                    System.err.println("Error unloading model: " + e.getMessage());
                } finally {
                    modelHandle = 0;
                }
            }
        } finally {
            modelLock.writeLock().unlock();
        }
    }
}
