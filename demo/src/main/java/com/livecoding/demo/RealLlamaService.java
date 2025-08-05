package com.livecoding.demo;

import org.springframework.stereotype.Service;
import org.springframework.web.client.RestTemplate;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.JsonNode;

import java.util.Map;
import java.util.HashMap;

@Service
public class RealLlamaService {

    private final RestTemplate restTemplate = new RestTemplate();
    private final ObjectMapper objectMapper = new ObjectMapper();
    private final String llamaServerUrl = "http://127.0.0.1:8081/completion";

    public String generateText(String prompt) throws Exception {
        // Prepare request body
        Map<String, Object> requestBody = new HashMap<>();
        requestBody.put("prompt", prompt);
        requestBody.put("n_predict", 256);
        requestBody.put("temperature", 0.7);
        requestBody.put("top_p", 0.9);
        requestBody.put("top_k", 40);
        requestBody.put("repeat_penalty", 1.1);
        requestBody.put("stream", false);

        // Prepare headers
        HttpHeaders headers = new HttpHeaders();
        headers.setContentType(MediaType.APPLICATION_JSON);

        // Create HTTP entity
        HttpEntity<Map<String, Object>> entity = new HttpEntity<>(requestBody, headers);

        try {
            // Make HTTP POST request to llama-server
            String response = restTemplate.postForObject(llamaServerUrl, entity, String.class);

            // Parse JSON response to extract content
            JsonNode jsonNode = objectMapper.readTree(response);
            JsonNode contentNode = jsonNode.get("content");

            if (contentNode != null) {
                return contentNode.asText().trim();
            } else {
                return "Error: No content in response";
            }

        } catch (Exception e) {
            throw new Exception("Failed to generate text from LLaMA server: " + e.getMessage());
        }
    }

    public boolean isServerRunning() {
        try {
            Map<String, Object> testRequest = new HashMap<>();
            testRequest.put("prompt", "test");
            testRequest.put("n_predict", 1);

            HttpHeaders headers = new HttpHeaders();
            headers.setContentType(MediaType.APPLICATION_JSON);
            HttpEntity<Map<String, Object>> entity = new HttpEntity<>(testRequest, headers);

            restTemplate.postForObject(llamaServerUrl, entity, String.class);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
