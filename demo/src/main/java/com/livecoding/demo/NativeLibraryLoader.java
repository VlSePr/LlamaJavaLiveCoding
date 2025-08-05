package com.livecoding.demo;

import java.io.*;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

public class NativeLibraryLoader {

    private static Path sharedTempDir = null;

    private static Path getSharedTempDir() throws IOException {
        if (sharedTempDir == null) {
            sharedTempDir = Files.createTempDirectory("native-libs");
            sharedTempDir.toFile().deleteOnExit();
        }
        return sharedTempDir;
    }

    public static void loadLibraryFromJar(String libraryName) throws IOException {
        String osName = System.getProperty("os.name").toLowerCase();

        String libraryFile;
        if (osName.contains("windows")) {
            libraryFile = libraryName + ".dll";
        } else if (osName.contains("linux")) {
            libraryFile = "lib" + libraryName + ".so";
        } else if (osName.contains("mac")) {
            libraryFile = "lib" + libraryName + ".dylib";
        } else {
            throw new UnsupportedOperationException("Unsupported operating system: " + osName);
        }

        // Use shared temp directory
        Path tempDir = getSharedTempDir();
        Path tempFile = tempDir.resolve(libraryFile);
        tempFile.toFile().deleteOnExit();

        // Extract library from resources
        try (InputStream inputStream = NativeLibraryLoader.class.getResourceAsStream("/" + libraryFile)) {
            if (inputStream == null) {
                throw new FileNotFoundException("Library file not found in resources: " + libraryFile);
            }
            Files.copy(inputStream, tempFile, StandardCopyOption.REPLACE_EXISTING);
        }

        // Load the library
        System.load(tempFile.toAbsolutePath().toString());
        System.out.println("Loaded main library: " + libraryFile);
    }

    public static void loadDependentLibrariesFromJar() throws IOException {
        // Core dependencies needed for basic functionality
        String[] coreLibs = {
                "ggml-base.dll",
                "ggml-cpu.dll",
                "ggml.dll"
        };

        // Optional dependencies that may have complex dependency chains
        String[] optionalLibs = {
                "mtmd.dll",
                "llama.dll"
        };

        Path tempDir = getSharedTempDir();

        // Extract all libraries to the same directory
        String[] allLibs = new String[coreLibs.length + optionalLibs.length];
        System.arraycopy(coreLibs, 0, allLibs, 0, coreLibs.length);
        System.arraycopy(optionalLibs, 0, allLibs, coreLibs.length, optionalLibs.length);

        for (String libName : allLibs) {
            Path tempFile = tempDir.resolve(libName);
            tempFile.toFile().deleteOnExit();

            try (InputStream inputStream = NativeLibraryLoader.class.getResourceAsStream("/" + libName)) {
                if (inputStream != null) {
                    Files.copy(inputStream, tempFile, StandardCopyOption.REPLACE_EXISTING);
                    System.out.println("Extracted dependency: " + tempFile.toAbsolutePath());
                } else {
                    System.err.println("Warning: Dependency not found in resources: " + libName);
                }
            } catch (Exception e) {
                System.err.println("Warning: Could not extract dependency " + libName + ": " + e.getMessage());
            }
        }

        // Load core libraries first (these are critical)
        for (String libName : coreLibs) {
            Path libFile = tempDir.resolve(libName);
            if (libFile.toFile().exists()) {
                try {
                    System.load(libFile.toAbsolutePath().toString());
                    System.out.println("Loaded core dependency: " + libName);
                } catch (Exception e) {
                    System.err.println("ERROR: Failed to load critical dependency " + libName + ": " + e.getMessage());
                    throw new RuntimeException("Failed to load critical dependency: " + libName, e);
                }
            }
        }

        // Try to load optional libraries (failures are non-fatal)
        for (String libName : optionalLibs) {
            Path libFile = tempDir.resolve(libName);
            if (libFile.toFile().exists()) {
                try {
                    System.load(libFile.toAbsolutePath().toString());
                    System.out.println("Loaded optional dependency: " + libName);
                } catch (Exception e) {
                    System.err
                            .println("Warning: Could not load optional dependency " + libName + ": " + e.getMessage());
                    System.err.println("         This may be resolved when the main JNI library loads.");
                    // Continue without throwing - these dependencies might be loaded automatically
                    // by the main JNI library or may not be needed for basic functionality
                }
            }
        }
    }
}
