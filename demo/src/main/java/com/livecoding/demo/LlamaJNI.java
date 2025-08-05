package com.livecoding.demo;

public class LlamaJNI implements LlamaJNIInterface {
    static {
        try {
            // Try direct loading of main JNI library first
            NativeLibraryLoader.loadLibraryFromJar("llama_jni");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Direct loading failed, trying with dependencies: " + e.getMessage());
            try {
                // Load dependent libraries first
                NativeLibraryLoader.loadDependentLibrariesFromJar();
                // Load main JNI library
                NativeLibraryLoader.loadLibraryFromJar("llama_jni");
            } catch (Exception e2) {
                throw new RuntimeException("Failed to load native library even with dependencies: " + e2.getMessage(),
                        e2);
            }
        } catch (Exception e) {
            throw new RuntimeException("Failed to load native library: " + e.getMessage(), e);
        }
    }

    // Native method declarations
    public native long loadModel(String path);

    public native String generateText(long modelHandle, String prompt);

    public native void unloadModel(long modelHandle);

    // Additional methods for configuration (to be implemented later)
    public native String getModelInfo(long modelHandle);

    public native boolean isModelLoaded(long modelHandle);
}
