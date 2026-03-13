# AGENTS.md - Hydroponic System

This file provides guidelines for agentic coding agents working in this repository.

## Project Overview

This is a hydroponic monitoring system with two main components:
- **Backend** (`backend/`): Node.js MQTT-to-HTTP bridge
- **ESP32** (`esp32/`): Embedded C code for sensor data collection

---

## Build, Lint & Test Commands

### Backend (Node.js)

```bash
# Install dependencies
cd backend && npm install

# Run the server
npm start                    # Production
npm run dev                  # Development (auto-reload with --watch)

# No formal linting or test framework configured
```

### ESP32 (ESP-IDF)

```bash
cd esp32/test_esp_idf/test_main

# Configure target chip
idf.py set-target <esp32_chip>  # e.g., esp32c3, esp32s3

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Run tests (pytest)
pytest_blink.py
idf.py pytest

# Clean build
idf.py fullclean
```

---

## Code Style Guidelines

### JavaScript (Backend)

**File Organization**
- One major file: `backend/main.js`
- Use CommonJS (`require`, not ES modules)
- Include `'use strict'` at top of every file
- Group code with comment headers: `// ─── Section ────`

**Imports**
- Use `require()` for Node built-ins and npm packages
- Order: built-ins → external packages → project modules
- Example:
  ```javascript
  'use strict';
  require('dotenv').config();
  const mqtt = require('mqtt');
  const express = require('express');
  ```

**Types & JSDoc**
- Use JSDoc for type annotations in global/state variables
- Example: `@type {Record<string, { topic: string, payload: unknown, ts: string }>}`

**Naming Conventions**
- Constants: `UPPER_SNAKE_CASE`
- Variables/functions: `camelCase`
- Unused parameters: prefix with `_` (e.g., `_req`, `_err`)

**Formatting**
- Use 2-space indentation
- Template literals for string interpolation
- Prefer const, avoid var
- Use early returns in conditionals

**Error Handling**
- Use try/catch for JSON parsing and async operations
- Return appropriate HTTP status codes (400, 404, 500)
- Log errors with context: `console.error('[MQTT] Error:', err.message)`

**Logging**
- Prefix logs with subsystem: `[MQTT]`, `[HTTP]`, `[API]`
- Use ISO timestamps for message events
- Example: `console.log('[MQTT] [${ts}] ${topic} →', parsed);`

### C (ESP-IDF)

**File Organization**
- Main app code in `main/src/`
- Headers in `main/include/`
- Component-based structure recommended (see README project tree)

**Naming Conventions**
- Functions: `snake_case`
- Macros: `UPPER_SNAKE_CASE`
- Variables: `snake_case` or `camelCase` per ESP-IDF defaults

**Headers**
- Include guard or `#pragma once`
- Group includes: standard → ESP-IDF → external → local

**Error Handling**
- Use ESP-IDF error codes (`esp_err_t`)
- Check return values with `ESP_ERR_xxx` macros
- Use `ESP_LOGx` macros for logging (`ESP_LOGI`, `ESP_LOGE`, etc.)

---

## API Endpoints (Backend)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Health check |
| GET | `/messages` | All latest messages |
| GET | `/messages/latest?topic=<topic>` | Latest message by topic |
| POST | `/publish` | Publish to MQTT |

---

## Environment Variables

Create `backend/.env` from `backend/.env.example`:

| Variable | Description |
|----------|-------------|
| `BROKER_URL` | MQTT broker URL |
| `MQTT_USERNAME` | MQTT username |
| `MQTT_PASSWORD` | MQTT password |
| `HTTP_PORT` | HTTP server port (default: 3000) |

---

## Important Notes

1. **No tests configured** for the backend - consider adding Jest or Mocha
2. **No formal linter** - consider adding ESLint
3. The ESP-IDF project follows standard Espressif conventions
4. Never commit `.env` files - they contain secrets
