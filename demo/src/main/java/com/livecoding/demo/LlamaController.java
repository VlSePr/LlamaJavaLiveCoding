package com.livecoding.demo;

import org.springframework.web.bind.annotation.*;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.http.HttpStatus;
import org.springframework.validation.annotation.Validated;

import java.util.HashMap;
import java.util.Map;

@RestController
@RequestMapping("/llama")
@Validated
public class LlamaController {

    @Autowired
    private LlamaService llamaService;

    @Autowired
    private RealLlamaService realLlamaService;

    @PostMapping("/generate")
    public ResponseEntity<Map<String, Object>> generate(@RequestBody GenerateRequest request) {
        try {
            String result;

            // Try real LLaMA first, fall back to JNI mock if server not available
            if (realLlamaService.isServerRunning()) {
                result = realLlamaService.generateText(request.getPrompt());
            } else {
                result = llamaService.generateText(request.getPrompt());
            }

            Map<String, Object> response = new HashMap<>();
            response.put("text", result);
            response.put("status", "success");
            response.put("prompt_length", request.getPrompt().length());

            return ResponseEntity.ok(response);
        } catch (LlamaException e) {
            return createErrorResponse(HttpStatus.BAD_REQUEST, "Generation failed", e.getMessage());
        } catch (Exception e) {
            return createErrorResponse(HttpStatus.INTERNAL_SERVER_ERROR, "Internal error",
                    "An unexpected error occurred");
        }
    }

    @GetMapping("/generate")
    public ResponseEntity<Map<String, Object>> generateGet(@RequestParam String prompt) {
        try {
            if (prompt == null || prompt.trim().isEmpty()) {
                return createErrorResponse(HttpStatus.BAD_REQUEST, "Invalid input", "Prompt cannot be null or empty");
            }

            String result;

            // Try real LLaMA first, fall back to JNI mock if server not available
            if (realLlamaService.isServerRunning()) {
                result = realLlamaService.generateText(prompt);
            } else {
                result = llamaService.generateText(prompt);
            }

            Map<String, Object> response = new HashMap<>();
            response.put("text", result);
            response.put("status", "success");
            response.put("prompt_length", prompt.length());

            return ResponseEntity.ok(response);
        } catch (LlamaException e) {
            return createErrorResponse(HttpStatus.BAD_REQUEST, "Generation failed", e.getMessage());
        } catch (Exception e) {
            return createErrorResponse(HttpStatus.INTERNAL_SERVER_ERROR, "Internal error",
                    "An unexpected error occurred");
        }
    }

    @GetMapping("/status")
    public ResponseEntity<Map<String, Object>> getStatus() {
        try {
            Map<String, Object> status = new HashMap<>();

            boolean realLlamaAvailable = realLlamaService.isServerRunning();
            status.put("model_loaded", true);
            status.put("real_llama_available", realLlamaAvailable);

            if (realLlamaAvailable) {
                status.put("model_status", "Real LLaMA-3.2-3B server running on http://127.0.0.1:8081");
                status.put("mode", "real_llama");
            } else {
                status.put("model_status", llamaService.getModelStatus());
                status.put("mode", "enhanced_mock");
            }

            status.put("status", "healthy");

            return ResponseEntity.ok(status);
        } catch (Exception e) {
            return createErrorResponse(HttpStatus.INTERNAL_SERVER_ERROR, "Status check failed", e.getMessage());
        }
    }

    @PostMapping("/reload")
    public ResponseEntity<Map<String, Object>> reloadModel() {
        try {
            // This would trigger cleanup and reload
            llamaService.cleanup();

            Map<String, Object> response = new HashMap<>();
            response.put("status", "success");
            response.put("message", "Model reload initiated");

            return ResponseEntity.ok(response);
        } catch (Exception e) {
            return createErrorResponse(HttpStatus.INTERNAL_SERVER_ERROR, "Reload failed", e.getMessage());
        }
    }

    private ResponseEntity<Map<String, Object>> createErrorResponse(HttpStatus status, String error, String message) {
        Map<String, Object> errorResponse = new HashMap<>();
        errorResponse.put("status", "error");
        errorResponse.put("error", error);
        errorResponse.put("message", message);
        errorResponse.put("timestamp", System.currentTimeMillis());

        return ResponseEntity.status(status).body(errorResponse);
    }

    // Request DTO for POST endpoint
    public static class GenerateRequest {
        private String prompt;
        private Integer maxTokens;
        private Float temperature;

        public String getPrompt() {
            return prompt;
        }

        public void setPrompt(String prompt) {
            this.prompt = prompt;
        }

        public Integer getMaxTokens() {
            return maxTokens;
        }

        public void setMaxTokens(Integer maxTokens) {
            this.maxTokens = maxTokens;
        }

        public Float getTemperature() {
            return temperature;
        }

        public void setTemperature(Float temperature) {
            this.temperature = temperature;
        }
    }
}
