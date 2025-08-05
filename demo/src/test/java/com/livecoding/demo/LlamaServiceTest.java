package com.livecoding.demo;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.test.util.ReflectionTestUtils;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.*;
import static org.mockito.Mockito.*;

@ExtendWith(MockitoExtension.class)
class LlamaServiceTest {

    @Mock
    private LlamaJNIInterface llamaJNI;

    private LlamaService llamaService;

    @BeforeEach
    void setUp() {
        llamaService = new LlamaService(llamaJNI); // Use constructor injection
        ReflectionTestUtils.setField(llamaService, "modelPath", "test-model.gguf");
        ReflectionTestUtils.setField(llamaService, "maxPromptLength", 1000);
        ReflectionTestUtils.setField(llamaService, "generationTimeoutSeconds", 10);
    }

    @Test
    void testValidatePrompt_ValidPrompt_ShouldPass() {
        when(llamaJNI.loadModel(anyString())).thenReturn(1L);
        when(llamaJNI.generateText(eq(1L), eq("Valid prompt"))).thenReturn("Generated text");

        assertDoesNotThrow(() -> {
            String result = llamaService.generateText("Valid prompt");
            assertNotNull(result);
            assertEquals("Generated text", result);
        });
    }

    @Test
    void testValidatePrompt_NullPrompt_ShouldThrow() {
        assertThrows(LlamaException.class, () -> {
            llamaService.generateText(null);
        });
    }

    @Test
    void testValidatePrompt_EmptyPrompt_ShouldThrow() {
        assertThrows(LlamaException.class, () -> {
            llamaService.generateText("");
        });

        assertThrows(LlamaException.class, () -> {
            llamaService.generateText("   ");
        });
    }

    @Test
    void testValidatePrompt_TooLongPrompt_ShouldThrow() {
        String longPrompt = "a".repeat(1001); // Exceeds maxPromptLength of 1000

        assertThrows(LlamaException.class, () -> {
            llamaService.generateText(longPrompt);
        });
    }

    @Test
    void testSanitizePrompt_ControlCharacters_ShouldBeRemoved() {
        when(llamaJNI.loadModel(anyString())).thenReturn(1L);
        when(llamaJNI.generateText(eq(1L), eq("HelloWorld"))).thenReturn("Response");

        assertDoesNotThrow(() -> {
            String result = llamaService.generateText("Hello\u0000\u0001World");
            assertNotNull(result);
            assertEquals("Response", result);
        });

        verify(llamaJNI).generateText(eq(1L), eq("HelloWorld"));
    }

    @Test
    void testSuspiciousContent_ShouldBeRejected() {
        String[] suspiciousPrompts = {
                "system(\"rm -rf /\")",
                "exec(malicious_code)",
                "Load malicious.dll",
                "Run cmd.exe"
        };

        for (String prompt : suspiciousPrompts) {
            assertThrows(LlamaException.class, () -> {
                llamaService.generateText(prompt);
            }, "Should reject suspicious prompt: " + prompt);
        }
    }

    @Test
    void testModelLoading_FailedLoad_ShouldThrow() {
        when(llamaJNI.loadModel(anyString())).thenReturn(0L); // Indicates failure

        assertThrows(LlamaException.class, () -> {
            llamaService.generateText("Valid prompt");
        });
    }

    @Test
    void testModelLoading_ExceptionDuringLoad_ShouldThrow() {
        when(llamaJNI.loadModel(anyString())).thenThrow(new RuntimeException("Native error"));

        assertThrows(LlamaException.class, () -> {
            llamaService.generateText("Valid prompt");
        });
    }
}
