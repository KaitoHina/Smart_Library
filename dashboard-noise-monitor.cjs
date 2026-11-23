const { readFileSync } = require('node:fs');
const { join } = require('node:path');

// Adjust these defaults for the actual room. Durations are milliseconds.
const CONFIG = {
  windowMs: 30000,
  noisyFraction: 0.7,
  quietToEndMs: 15000,
  staleMs: 5000,
  fallbackNoisyAmplitude: 100,
  levelsMs: [30000, 60000, 300000, 600000]
};
const samples = [];
const history = [];
let last = null;
let active = null;
let quietSince = null;

function finish(reason, now) {
  if (active) {
    history.unshift({ ...active, endedAt: now, reason });
    history.splice(30);
  }
  active = null;
  quietSince = null;
}

function refresh(now) {
  if (last && now - last.time >= CONFIG.staleMs) {
    finish('Monitoring interrupted', last.time);
    samples.length = 0;
    last = null;
  }
}

function receive(packet) {
  const now = packet.receivedAt;
  refresh(now);
  const noisy = packet.noiseLevel ? packet.noiseLevel === 'NOISY' :
    Number.isFinite(packet.noiseAmplitude) ? packet.noiseAmplitude >= CONFIG.fallbackNoisyAmplitude : null;
  if (noisy === null) {
    finish('Noise reading unavailable', now);
    samples.length = 0;
    last = null;
    return;
  }
  last = { time: now, noisy };
  samples.push(last);
  while (samples.length > 1 && samples[1].time <= now - CONFIG.windowMs) samples.shift();
  if (active) {
    if (noisy) quietSince = null;
    else quietSince ??= now;
    if (quietSince !== null && now - quietSince >= CONFIG.quietToEndMs) finish('Quiet period confirmed', now);
  }
  if (!active && samples.length > 1 && now - samples[0].time >= CONFIG.windowMs &&
      fraction(now) >= CONFIG.noisyFraction) {
    active = { startedAt: now - CONFIG.windowMs, detectedAt: now, acknowledgedAt: null, handledAt: null };
  }
}

function fraction(now) {
  let noisyMs = 0;
  for (let i = 0; i < samples.length; i++) {
    const start = Math.max(now - CONFIG.windowMs, samples[i].time);
    const end = Math.min(now, samples[i + 1]?.time ?? now);
    if (samples[i].noisy) noisyMs += Math.max(0, end - start);
  }
  return noisyMs / CONFIG.windowMs;
}

function snapshot() {
  const now = Date.now();
  refresh(now);
  const durationMs = active ? now - active.startedAt : 0;
  return {
    table: 'Table 05', now, lastReceivedAt: last?.time ?? null,
    status: !last ? 'Monitoring interrupted' : active ? 'Sustained noise' : 'Observing',
    noisyFraction: fraction(now), windowCoverageMs: samples.length ? Math.min(CONFIG.windowMs, now - samples[0].time) : 0,
    durationMs, level: active ? CONFIG.levelsMs.filter(threshold => durationMs >= threshold).length : 0,
    active, history, config: CONFIG
  };
}

function respond(res, code, data) {
  res.writeHead(code, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(data));
}

function handleAdmin(req, res, port) {
  const adminPort = port;
  const hosts = [`localhost:${adminPort}`, `127.0.0.1:${adminPort}`];
  // This listener is separate from the public dashboard. Reject forwarded requests too.
  if (!['127.0.0.1', '::1', '::ffff:127.0.0.1'].includes(req.socket.remoteAddress) || !hosts.includes(req.headers.host) || Object.keys(req.headers).some(name =>
    name === 'forwarded' || name.startsWith('x-forwarded-') || name.startsWith('cf-'))) {
    return respond(res, 403, { error: 'Local computer access only' });
  }
  if (req.method === 'GET' && req.url === '/admin' || req.method === 'GET' && req.url === '/admin/') {
    try {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
      return res.end(readFileSync(join(__dirname, 'dashboard-admin.html')));
    } catch { return respond(res, 500, { error: 'Admin page unavailable' }); }
  }
  if (req.method === 'GET' && req.url === '/admin/status') return respond(res, 200, snapshot());
  if (req.method === 'POST' && ['/admin/acknowledge', '/admin/handled'].includes(req.url)) {
    if (!hosts.some(host => req.headers.origin === `http://${host}`)) {
      return respond(res, 403, { error: 'Local origin required' });
    }
    refresh(Date.now());
    if (!active) return respond(res, 409, { error: 'No active event' });
    active[req.url === '/admin/handled' ? 'handledAt' : 'acknowledgedAt'] = Date.now();
    return respond(res, 200, snapshot());
  }
  respond(res, 404, { error: 'Not found' });
}

module.exports = { receive, handleAdmin };
