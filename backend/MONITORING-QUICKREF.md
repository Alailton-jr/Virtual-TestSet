# 🔍 Backend Monitoring - Quick Reference

## 🚀 Start Backend

```bash
cd backend
./scripts/monitor.sh
```

**What it does:**
- ✅ Builds backend if needed
- ✅ Starts server with color-coded logs
- ✅ Saves logs to `.logs/` directory
- ✅ Shows errors in **red**, warnings in **yellow**, info in **green**

---

## 👀 Watch Logs (in separate terminal)

```bash
cd backend
./scripts/watch-logs.sh
```

**When to use:** Monitor logs while backend runs in another terminal

---

## 📊 Check Backend Status

```bash
cd backend
./scripts/status.sh
```

**What it shows:**
- ✅ If backend is running (port, PID, process name)
- ✅ HTTP API health check
- ✅ Last 10 log entries
- ✅ Available commands

---

## 📝 View Logs Manually

```bash
# List all logs
ls -lh backend/.logs/

# View latest log
tail -f backend/.logs/backend-*.log | tail -1

# Search for errors
grep -i error backend/.logs/backend-*.log

# Count error occurrences
grep -ic error backend/.logs/backend-$(ls -t backend/.logs/ | head -n 1)
```

---

## 🛑 Stop Backend

**Method 1: Graceful stop (if running in terminal)**
```bash
Press Ctrl+C
```

**Method 2: Force stop**
```bash
# Find the process
lsof -i :8080

# Kill it
kill -9 <PID>
```

**Method 3: Using status script**
```bash
cd backend
./scripts/status.sh
# Note the PID, then:
kill <PID>
```

---

## 🐛 Common Issues

### Backend won't start
```bash
cd backend
./scripts/monitor.sh
# Check red ERROR messages
```

**Common causes:**
- Port 8080 already in use
- Missing dependencies
- Build failed

### Can't connect from frontend
```bash
# Check if backend is running
cd backend
./scripts/status.sh

# Should show:
# ✓ Process running on port 8080
# ✓ HTTP API is responding
```

### See errors in logs
```bash
# Watch logs in real-time
cd backend
./scripts/watch-logs.sh

# Errors appear in RED
# Warnings appear in YELLOW
```

---

## 🎨 Log Color Guide

| Color | Meaning | Examples |
|-------|---------|----------|
| 🔴 Red | **ERROR/FATAL** | Crashes, failed operations |
| 🟡 Yellow | **WARN** | Warnings, potential issues |
| 🟢 Green | **INFO/Success** | Server started, operations OK |
| 🔵 Blue | **DEBUG** | Detailed traces |
| 🟣 Magenta | **WebSocket** | WS connections/messages |
| 🔷 Cyan | **HTTP** | HTTP requests |

---

## 🏃 Full Workflow

### Terminal 1: Backend
```bash
cd backend
./scripts/monitor.sh
```

### Terminal 2: Frontend
```bash
cd frontend
npm run dev
```

### Terminal 3: Watch Logs (optional)
```bash
cd backend
./scripts/watch-logs.sh
```

---

## 🔧 Advanced

### Filter logs by type
```bash
# Only errors
./scripts/watch-logs.sh | grep -i error

# Only HTTP requests
./scripts/watch-logs.sh | grep -iE "GET|POST"
```

### Run in background
```bash
cd backend
nohup ./build/Main > .logs/backend-bg.log 2>&1 &
echo $! > .logs/backend.pid

# Stop later
kill $(cat .logs/backend.pid)
```

### Archive old logs
```bash
cd backend/.logs
tar -czf archive-$(date +%Y%m%d).tar.gz backend-*.log
rm backend-*.log
```

---

## 📚 More Info

See [MONITORING.md](./MONITORING.md) for detailed documentation.

---

**Tip:** Keep this file open in a terminal while developing! 🚀
