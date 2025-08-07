# LLaMA Java Wrapper

A Spring Boot application that provides a Java wrapper around the LLaMA C++ library, exposing AI text generation capabilities through REST endpoints.

## Recent Improvements

✅ **Complete C++ Implementation**: Implemented actual LLaMA text generation with proper tokenization, sampling, and generation  
✅ **Thread Safety**: Added concurrent model handling with read-write locks and request semaphores  
✅ **Error Handling**: Comprehensive null checks, input validation, and proper error responses  
✅ **Input Validation**: Sanitization and length limits for prompts with security checks  

## Architecture

### Components
- **LlamaJNI**: JNI interface for native method calls
- **LlamaService**: Thread-safe service layer with model management
- **LlamaController**: REST API endpoints with proper error handling
- **Native Layer**: C implementation with actual LLaMA integration

## Building the Project

### Prerequisites
- Java 17
- Maven 3.6+
- Visual Studio 2022 (with C++ tools)
- LLaMA.cpp library compiled and available

### Compilation Steps

1. **Compile Native Library**:
   ```bash
   # Adjust paths in compile_jni.bat for your system
   ./compile_jni.bat
   ```

2. **Build Java Application**:
   ```bash
   mvn clean compile
   ```

3. **Run Tests**:
   ```bash
   mvn test
   ```

4. **Package Application**:
   ```bash
   mvn package
   ```

## Running the Application

### Quick Start Guide

The application supports two modes: **Real LLM Integration** (with llama-server.exe) and **Enhanced Mock Mode** (fallback).

#### Option 1: Real LLM Integration (Recommended)

**Step 1: Start LLaMA Server**
```cmd
# Navigate to llama-server directory
cd "...\source\repos\LLAama\llama.cpp\build\bin\Release"

# Start server with your model
.\llama-server.exe --model "...\source\repos\Cortana\AIModels\Llama-3.2-3B-Instruct-Q3_K_L.gguf" --port 8081 --host 127.0.0.1 --ctx-size 2048
```

**Wait for confirmation:**
```
main: server is listening on http://127.0.0.1:8081 - starting the main loop
```

**Step 2: Verify LLaMA Server**
```powershell
# Test health endpoint
Invoke-RestMethod -Uri "http://127.0.0.1:8081/health" -Method GET
```

**Step 3: Start Spring Boot Application**
```cmd
# Navigate to project directory
cd "...\source\repos\LlamaJavaLiveCoding\demo"

# Start the application
.\mvnw.cmd spring-boot:run
```

**Wait for confirmation:**
```
Started DemoApplication in X.X seconds
```

**Step 4: Test Real LLM Integration**
```powershell
# Check status - should show real_llama_available: true
Invoke-RestMethod -Uri "http://localhost:8080/llama/status" -Method GET

# Test real LLM generation
Invoke-RestMethod -Uri "http://localhost:8080/llama/generate?prompt=What is quantum computing?" -Method GET
```

#### Option 2: Enhanced Mock Mode (Fallback)

If you skip the LLaMA server setup, the application automatically runs in enhanced mock mode:

```cmd
# Just start the Spring Boot application
cd "...\source\repos\LlamaJavaLiveCoding\demo"
.\mvnw.cmd spring-boot:run
```

### Expected Response Formats

#### Real LLM Mode:
```json
{
  "text": "Quantum computing is a revolutionary computational paradigm...",
  "mode": "real_llama",
  "prompt_length": 25
}
```

#### Enhanced Mock Mode:
```json
{
  "text": "That's a fascinating topic! Quantum computing leverages...",
  "mode": "enhanced_mock",
  "prompt_length": 25
}
```

### Process Management

```powershell
# Check if llama-server is running
Get-Process | Where-Object { $_.Name -like "*llama*" }

# Stop llama-server if needed
Stop-Process -Name "llama-server" -Force
```

### Troubleshooting Startup

- **LLaMA server fails to start**: Check model path and file existence
- **Connection refused**: Verify port 8081 not in use, check firewall
- **real_llama_available: false**: Ensure llama-server started first
- **Spring Boot startup issues**: Check port 8080 availability

## Configuration

Edit `src/main/resources/application.properties`:

```properties
# LLaMA Configuration
llama.model.path=Llama-3.2-3B-Instruct-Q3_K_L.gguf
llama.max.prompt.length=4000
llama.generation.timeout.seconds=30

# Server Configuration
server.port=8080
```

## API Endpoints

### Generate Text (POST)
```bash
curl -X POST http://localhost:8080/llama/generate \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Hello, how are you?"}'
```

**Response**:
```json
{
  "text": "Generated response...",
  "status": "success",
  "prompt_length": 18
}
```

### Generate Text (GET)
```bash
curl "http://localhost:8080/llama/generate?prompt=Hello,%20world!"
```

### Check Status
```bash
curl http://localhost:8080/llama/status
```

**Response**:
```json
{
  "model_loaded": true,
  "model_status": "Model loaded from: Llama-3.2-3B-Instruct-Q3_K_L.gguf",
  "status": "healthy"
}
```

### Reload Model
```bash
curl -X POST http://localhost:8080/llama/reload
```

## Security Features

- **Input Validation**: Prompt length limits and content sanitization
- **Suspicious Content Detection**: Blocks potentially harmful prompts
- **Thread Safety**: Concurrent request handling with proper synchronization
- **Resource Limits**: Semaphore-based generation limiting
- **Error Isolation**: Proper exception handling without exposing internals

## Threading Model

- **Read-Write Locks**: Multiple concurrent generations, exclusive model loading
- **Semaphore**: Limits concurrent generation requests (default: 5)
- **Timeout Protection**: Generation operations have configurable timeouts

## Error Handling

### Error Response Format
```json
{
  "status": "error",
  "error": "Error category",
  "message": "Detailed error message",
  "timestamp": 1640995200000
}
```

### Common Error Cases
- Invalid/null prompts → 400 Bad Request
- Prompts too long → 400 Bad Request
- Model loading failures → 500 Internal Server Error
- Generation timeouts → 400 Bad Request
- Suspicious content → 400 Bad Request

## Performance Considerations

- **Model Persistence**: Model loaded once and reused
- **Memory Management**: Proper cleanup in C layer
- **Request Limiting**: Configurable concurrent generation limits
- **Input Sanitization**: Minimal overhead validation

## Development

### Running Tests
```bash
mvn test
```

### Running Application
```bash
mvn spring-boot:run
```

### Building for Production
```bash
mvn clean package
java -jar target/demo-0.0.1-SNAPSHOT.jar
```

## Troubleshooting

### Common Issues

1. **"Failed to load native library"**
   - Ensure `llama_jni.dll` is in resources folder
   - Check that all dependent DLLs are available
   - Verify path configuration in `compile_jni.bat`

2. **"Model not loaded"**
   - Check model file path in `application.properties`
   - Ensure model file exists and is readable
   - Check application logs for loading errors

3. **Generation timeouts**
   - Increase `llama.generation.timeout.seconds`
   - Reduce concurrent request limits
   - Check model size and hardware capabilities

### Logging
```properties
logging.level.com.livecoding.demo=DEBUG
```

## Future Enhancements

- [ ] Multiple model support
- [ ] Async generation endpoints
- [ ] Streaming responses
- [ ] GPU acceleration
- [ ] Model hot-swapping
- [ ] Metrics and monitoring
- [ ] Configuration validation
- [ ] Batch processing
