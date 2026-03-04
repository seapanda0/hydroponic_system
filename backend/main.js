'use strict';

require('dotenv').config();
const mqtt = require('mqtt');
const express = require('express');

// ─── Config ────────────────────────────────────────────────────────────────
const BROKER_URL = process.env.BROKER_URL || 'mqtts://8892a280da7b489e930600e068ffd8a3.s1.eu.hivemq.cloud:8883';
const MQTT_USERNAME = process.env.MQTT_USERNAME || '';
const MQTT_PASSWORD = process.env.MQTT_PASSWORD || '';
const HTTP_PORT = parseInt(process.env.HTTP_PORT || '3000', 10);

// Topics to subscribe to (adjust to match your ESP32 publish topics)
const TOPICS = [
  'sensors/+/data',   // e.g. sensors/esp32-001/data
  'sensors/+/status', // e.g. sensors/esp32-001/status
];

// ─── State ─────────────────────────────────────────────────────────────────
/** @type {Record<string, { topic: string, payload: unknown, ts: string }>} */
const latestMessages = {};

// ─── MQTT Client ───────────────────────────────────────────────────────────
const clientOptions = {
  clientId: `backend_${Math.random().toString(16).slice(2, 8)}`,
  clean: true,
  reconnectPeriod: 5000,
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
};

const client = mqtt.connect(BROKER_URL, clientOptions);

client.on('connect', () => {
  console.log(`[MQTT] Connected to broker: ${BROKER_URL}`);
  client.subscribe(TOPICS, (err) => {
    if (err) {
      console.error('[MQTT] Subscribe error:', err.message);
    } else {
      console.log('[MQTT] Subscribed to:', TOPICS.join(', '));
    }
  });
});

client.on('message', (topic, payload) => {
  let parsed;
  try {
    parsed = JSON.parse(payload.toString());
  } catch {
    parsed = payload.toString();
  }

  const ts = new Date().toISOString();
  latestMessages[topic] = { topic, payload: parsed, ts };
  console.log(`[MQTT] [${ts}] ${topic} →`, parsed);
});

client.on('reconnect', () => console.log('[MQTT] Reconnecting…'));
client.on('offline',   () => console.log('[MQTT] Client offline'));
client.on('error',     (err) => console.error('[MQTT] Error:', err.message));

// ─── Express HTTP API ───────────────────────────────────────────────────────
const app = express();
app.use(express.json());

app.get('/health', (_req, res) => {
  res.json({ status: 'ok', broker: BROKER_URL, connected: client.connected });
});

app.get('/messages', (_req, res) => {
  res.json(Object.values(latestMessages));
});

app.get('/messages/latest', (req, res) => {
  const { topic } = req.query;
  if (!topic) return res.status(400).json({ error: 'topic query param required' });
  const msg = latestMessages[topic];
  if (!msg) return res.status(404).json({ error: 'No message for that topic yet' });
  return res.json(msg);
});

app.post('/publish', (req, res) => {
  const { topic, payload } = req.body;
  if (!topic || payload === undefined) {
    return res.status(400).json({ error: 'topic and payload are required' });
  }
  const message = typeof payload === 'string' ? payload : JSON.stringify(payload);
  client.publish(topic, message, { qos: 1 }, (err) => {
    if (err) return res.status(500).json({ error: err.message });
    return res.json({ success: true, topic, payload });
  });
});

app.listen(HTTP_PORT, () => {
  console.log(`[HTTP] API server listening on http://localhost:${HTTP_PORT}`);
}).on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(`[HTTP] ❌ Port ${HTTP_PORT} is already in use. Kill the other process and retry.`);
  } else {
    console.error('[HTTP] ❌ Server error:', err.message);
  }
  process.exit(1);
});
