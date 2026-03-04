# Backend - MQTT to HTTP Bridge

This is the backend service for the TechForGood sensor project. It acts as a bridge between MQTT (receives sensor data from ESP32) and HTTP (exposes data via a REST API).

## Setup & Installation

1.  **Install Dependencies**:
    ```bash
    npm install
    ```

2.  **Environment Configuration**:
    Create a `.env` file in this directory based on `.env.example`.
    ```bash
    cp .env.example .env
    ```

3.  **Run the Server**:
    - **Production**: `npm start`
    - **Development (Auto-reload)**: `npm run dev`

## Configuration (.env)

| Variable | Description | Default |
| :--- | :--- | :--- |
| `BROKER_URL` | The URL of the MQTT broker. | `mqtts://...s1.eu.hivemq.cloud:8883` |
| `MQTT_USERNAME` | MQTT broker username. | (Required) |
| `MQTT_PASSWORD` | MQTT broker password. | (Required) |
| `HTTP_PORT` | The port the Express server will listen on. | `3000` |

## API Endpoints

### 1. Health Check
`GET /health`
Returns the status of the backend and its connection to the MQTT broker.

**Response (JSON)**:
```json
{
  "status": "ok",
  "broker": "mqtts://...",
  "connected": true
}
```

### 2. Get All Messages
`GET /messages`
Returns an array of the latest messages received from all topics.

**Response (JSON)**:
```json
[
  {
    "topic": "sensors/esp32-001/data",
    "payload": { "temp": 25.5 },
    "ts": "2024-03-03T12:00:00.000Z"
  }
]
```

### 3. Get Latest Message by Topic
`GET /messages/latest?topic=<topic_name>`
Returns the latest message received for a specific MQTT topic.

**Query Parameters**:
- `topic` (Required): The full topic name (e.g., `sensors/esp32-001/data`).

**Response (JSON)**:
- `200 OK`: Returns the message object.
- `400 Bad Request`: If `topic` is missing.
- `404 Not Found`: If no message has been received for that topic yet.

### 4. Publish Message
`POST /publish`
Publishes a message to a specific MQTT topic via the bridge.

**Usage Note**: It's best practice to use a separate topic for commands (e.g., `sensors/<device-id>/command`) rather than reusing the data topic. This allows the ESP32 to subscribe only to its control messages.

**Request Body (JSON)**:
```json
{
  "topic": "sensors/esp32-001/command",
  "payload": { "led": "ON" }
}
```

**Response (JSON)**:
- `200 OK`: `{ "success": true, "topic": "...", "payload": "..." }`
- `400 Bad Request`: If `topic` or `payload` is missing.
- `500 Internal Server Error`: If publishing fails.
