# Backend Monitoring Tools

This document describes the monitoring and debugging tools available for the Virtual TestSet backend.

## Quick Start

### Option 1: Monitor in One Terminal (Recommended)
```bash
cd backend
./scripts/monitor.sh
```

This will:
- Build the backend if needed
- Start the backend server
- Display color-coded logs in real-time
- Save logs to `.logs/` directory
- Show ERROR (red), WARN (yellow), INFO (green), DEBUG (blue)

### Option 2: Run Backend + Watch Logs in Separate Terminals

**Terminal 1 - Run Backend:**
```bash
cd backend
./scripts/monitor.sh
```

**Terminal 2 - Watch Logs:**
```bash
cd backend
./scripts/watch-logs.sh
```

This setup allows you to:
- Keep backend running in one terminal
- Monitor logs in another terminal
- Easy to inspect errors without stopping the server

## Log Files

Logs are automatically saved to:
```
backend/.logs/backend-YYYYMMDD-HHMMSS.log
```

Example:
```
backend/.logs/backend-20241111-143025.log
```

### View Historical Logs
```bash
# List all log files
ls -lh backend/.logs/

# View a specific log
cat backend/.logs/backend-20241111-143025.log

# Search for errors
grep -i error backend/.logs/backend-20241111-143025.log

# Follow the latest log
tail -f backend/.logs/backend-$(ls -t backend/.logs/ | head -n 1)
```

## Color Coding

The monitoring scripts use color coding for easy identification:

| Color | Log Level | Examples |
|-------|-----------|----------|
| 🔴 **Red** | ERROR/FATAL | Critical errors, crashes, failed operations |
| 🟡 **Yellow** | WARN | Warnings, potential issues, deprecations |
| 🟢 **Green** | INFO/Success | Server started, successful operations, status |
| 🔵 **Blue** | DEBUG | Debug information, detailed traces |
| 🟣 **Magenta** | WebSocket | WebSocket connections, messages, events |
| 🔷 **Cyan** | HTTP | HTTP requests (GET, POST, PUT, DELETE, PATCH) |

## Debugging Scenarios

### Scenario 1: Backend Won't Start
```bash
cd backend
./scripts/monitor.sh
# Watch for red ERROR messages
# Common issues: port already in use, missing dependencies
```

### Scenario 2: Frontend Can't Connect
```bash
# Terminal 1: Run backend with monitoring
cd backend
./scripts/monitor.sh

# Look for:
# ✓ Server listening on port XXXX
# ✓ WebSocket endpoint ready
```

Check that the backend is listening on the expected port (usually 8080 or 3001).

### Scenario 3: API Errors
```bash
# Terminal 1: Backend
cd backend
./scripts/monitor.sh

# Terminal 2: Test API
curl -X GET http://localhost:8080/api/streams

# Watch Terminal 1 for:
# - Cyan HTTP request logs
# - Red error messages if something fails
```

### Scenario 4: WebSocket Connection Issues
```bash
# Terminal 1: Backend with logs
cd backend
./scripts/monitor.sh

# Terminal 2: Test WebSocket (if you have wscat)
wscat -c ws://localhost:8080/ws

# Watch Terminal 1 for:
# - Magenta WebSocket connection logs
# - Green success messages
# - Red connection errors
```

### Scenario 5: Memory or Performance Issues
```bash
# Run backend with additional monitoring
cd backend

# Build in debug mode if needed
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run with monitoring
./scripts/monitor.sh

# Look for:
# - Yellow warnings about memory
# - Blue debug messages about performance
# - Any repeated error patterns
```

## Advanced Usage

### Filter Logs by Type
```bash
# Show only errors
./scripts/monitor.sh 2>&1 | grep -i error

# Show only warnings and errors
./scripts/monitor.sh 2>&1 | grep -iE "error|warn"

# Show HTTP requests only
./scripts/watch-logs.sh | grep -iE "GET|POST|PUT|DELETE|PATCH"
```

### Save Filtered Logs
```bash
# Save only errors to a separate file
./scripts/monitor.sh 2>&1 | grep -i error > errors-only.log
```

### Run Backend in Background with Logging
```bash
# Start backend in background
cd backend
nohup ./build/Main > .logs/backend-bg.log 2>&1 &

# Get the process ID
echo $! > .logs/backend.pid

# Watch logs
tail -f .logs/backend-bg.log

# Stop backend later
kill $(cat .logs/backend.pid)
```

## Troubleshooting

### Issue: "Permission denied" when running scripts
```bash
chmod +x backend/scripts/monitor.sh
chmod +x backend/scripts/watch-logs.sh
```

### Issue: "Backend executable not found"
```bash
cd backend
make clean
make build
# Or on macOS:
./scripts/build_macos.sh
```

### Issue: "Port already in use"
```bash
# Find process using the port (e.g., 8080)
lsof -i :8080

# Kill the process
kill -9 <PID>

# Or change the port in backend configuration
```

### Issue: Logs not appearing
```bash
# Check if logs directory exists
ls -la backend/.logs/

# Create it manually if needed
mkdir -p backend/.logs

# Verify log file is being written
ls -lh backend/.logs/
```

## Integration with Frontend

When running both frontend and backend:

**Terminal 1 - Backend:**
```bash
cd backend
./scripts/monitor.sh
```

**Terminal 2 - Frontend:**
```bash
cd frontend
npm run dev
```

**Terminal 3 - Watch Backend Logs (optional):**
```bash
cd backend
./scripts/watch-logs.sh
```

## VS Code Integration

Add these tasks to `.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Start Backend with Monitoring",
      "type": "shell",
      "command": "./scripts/monitor.sh",
      "options": {
        "cwd": "${workspaceFolder}/backend"
      },
      "isBackground": true,
      "problemMatcher": []
    },
    {
      "label": "Watch Backend Logs",
      "type": "shell",
      "command": "./scripts/watch-logs.sh",
      "options": {
        "cwd": "${workspaceFolder}/backend"
      },
      "isBackground": true,
      "problemMatcher": []
    }
  ]
}
```

Then use `Cmd+Shift+P` → "Tasks: Run Task" → select the desired task.

## Tips

1. **Always start backend first** before starting the frontend
2. **Keep logs terminal visible** when testing new features
3. **Check for red ERROR messages** immediately when something fails
4. **Use Ctrl+C** to gracefully stop the backend
5. **Archive old logs** periodically to save space:
   ```bash
   cd backend/.logs
   tar -czf logs-archive-$(date +%Y%m%d).tar.gz backend-*.log
   rm backend-*.log
   ```

## Getting Help

If you encounter issues:

1. Check the color-coded logs for errors (red)
2. Look for warnings (yellow) that might indicate the problem
3. Review the log file in `.logs/` for full context
4. Check if the port is available: `lsof -i :8080`
5. Verify the backend built correctly: `ls -lh backend/build/Main`

For more information, see:
- [Backend README](../README.md)
- [macOS Build Instructions](../README-macos.md)
- [Docker Instructions](../README_DOCKER.md)
