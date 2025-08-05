package com.livecoding.demo;

/**
 * Interface for the JNI implementation to allow for testing
 */
public interface LlamaJNIInterface {
    long loadModel(String path);

    String generateText(long modelHandle, String prompt);

    void unloadModel(long modelHandle);

    String getModelInfo(long modelHandle);

    boolean isModelLoaded(long modelHandle);
}
