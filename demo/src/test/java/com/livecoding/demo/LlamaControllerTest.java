package com.livecoding.demo;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.InjectMocks;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.http.ResponseEntity;

import java.util.Map;

import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

@ExtendWith(MockitoExtension.class)
class LlamaControllerTest {

    @Mock
    private LlamaService llamaService;

    @InjectMocks
    private LlamaController llamaController;

    @Test
    void testGeneratePost_ValidRequest_ShouldReturnSuccess() throws Exception {
        when(llamaService.generateText(anyString())).thenReturn("Generated response");

        LlamaController.GenerateRequest request = new LlamaController.GenerateRequest();
        request.setPrompt("Hello, world!");

        ResponseEntity<Map<String, Object>> response = llamaController.generate(request);

        assertTrue(response.getStatusCode().is2xxSuccessful());
        Map<String, Object> body = response.getBody();
        assertNotNull(body);
        assertEquals("success", body.get("status"));
        assertEquals("Generated response", body.get("text"));
        assertEquals(13, body.get("prompt_length"));
    }

    @Test
    void testGenerateGet_ValidRequest_ShouldReturnSuccess() throws Exception {
        when(llamaService.generateText(anyString())).thenReturn("Generated response");

        ResponseEntity<Map<String, Object>> response = llamaController.generateGet("Hello, world!");

        assertTrue(response.getStatusCode().is2xxSuccessful());
        Map<String, Object> body = response.getBody();
        assertNotNull(body);
        assertEquals("success", body.get("status"));
        assertEquals("Generated response", body.get("text"));
    }

    @Test
    void testGenerateGet_EmptyPrompt_ShouldReturnError() {
        ResponseEntity<Map<String, Object>> response = llamaController.generateGet("");

        assertTrue(response.getStatusCode().is4xxClientError());
        Map<String, Object> body = response.getBody();
        assertNotNull(body);
        assertEquals("error", body.get("status"));
        assertEquals("Invalid input", body.get("error"));
    }

    @Test
    void testGeneratePost_LlamaException_ShouldReturnBadRequest() throws Exception {
        when(llamaService.generateText(anyString())).thenThrow(new LlamaException("Invalid prompt"));

        LlamaController.GenerateRequest request = new LlamaController.GenerateRequest();
        request.setPrompt("test");

        ResponseEntity<Map<String, Object>> response = llamaController.generate(request);

        assertTrue(response.getStatusCode().is4xxClientError());
        Map<String, Object> body = response.getBody();
        assertNotNull(body);
        assertEquals("error", body.get("status"));
        assertEquals("Generation failed", body.get("error"));
        assertEquals("Invalid prompt", body.get("message"));
    }

    @Test
    void testStatus_ShouldReturnModelStatus() {
        when(llamaService.isModelLoaded()).thenReturn(true);
        when(llamaService.getModelStatus()).thenReturn("Model loaded");

        ResponseEntity<Map<String, Object>> response = llamaController.getStatus();

        assertTrue(response.getStatusCode().is2xxSuccessful());
        Map<String, Object> body = response.getBody();
        assertNotNull(body);
        assertEquals("healthy", body.get("status"));
        assertEquals(true, body.get("model_loaded"));
        assertEquals("Model loaded", body.get("model_status"));
    }
}
