const http = require('node:http');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

let latest = null;
const ids = ['A', 'B', 'C', 'D'];
const levels = new Set(['QUIET', 'MODERATE', 'NOISY']);
const modes = new Set(['SILENT', 'QUIET', 'COLLAB']);
const port = Number(process.env.PORT || 8080);
const host = process.env.DASHBOARD_HOST || '127.0.0.1';
const page = join(__dirname, 'smart-library-weekly-dashboard.html');

function json(res, status, data) {
  res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(data));
}

const isObject = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const boolValue = value => value === true || value === 1 ? true : value === false || value === 0 ? false : null;

http.createServer(async (req, res) => {
  const path = req.url.split('?')[0];
  if (req.method === 'GET' && (path === '/' || path === '/smart-library-weekly-dashboard.html')) {
    try {
      const html = readFileSync(page);
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
      return res.end(html);
    } catch {
      return json(res, 500, { error: 'Dashboard unavailable' });
    }
  }
  if (path !== '/api/telemetry') return json(res, 404, { error: 'Not found' });
  if (req.method === 'GET') return json(res, 200, latest);
  if (req.method !== 'POST') return json(res, 405, { error: 'Use GET or POST' });
  if (!/^application\/json(?:\s*;|$)/i.test(req.headers['content-type'] || '')) {
    return json(res, 415, { error: 'Send application/json' });
  }
  try {
    let body = '';
    for await (const chunk of req.iterator({ destroyOnReturn: false })) {
      body += chunk.toString();
      if (body.length > 4096) {
        res.setHeader('Connection', 'close');
        return json(res, 413, { error: 'Packet too large' });
      }
    }
    const data = JSON.parse(body);
    const valid = value => value === null || (Number.isFinite(value) && value >= 0);
    if (!isObject(data) || !isObject(data.distanceCm) ||
        !ids.every(id => valid(data.distanceCm[id])) ||
        !valid(data.noiseAmplitude) || !valid(data.radar)) {
      return json(res, 400, { error: 'Send distanceCm A–D, noiseAmplitude and radar (numbers or null)' });
    }

    const optional = {};
    const has = key => Object.prototype.hasOwnProperty.call(data, key);
    const badOptional = field => json(res, 400, { error: `Invalid optional field: ${field}` });

    if (has('seatOccupied')) {
      if (!isObject(data.seatOccupied)) return badOptional('seatOccupied');
      const values = ids.map(id => boolValue(data.seatOccupied[id]));
      if (values.some(value => value === null)) return badOptional('seatOccupied');
      optional.seatOccupied = Object.fromEntries(ids.map((id, index) => [id, values[index]]));
    }
    if (has('freeCount')) {
      if (!Number.isInteger(data.freeCount) || data.freeCount < 0 || data.freeCount > ids.length) {
        return badOptional('freeCount');
      }
      optional.freeCount = data.freeCount;
    }
    if (has('noiseLevel')) {
      if (typeof data.noiseLevel !== 'string' || !levels.has(data.noiseLevel)) return badOptional('noiseLevel');
      optional.noiseLevel = data.noiseLevel;
    }
    if (has('noiseStage')) {
      if (!Number.isInteger(data.noiseStage) || data.noiseStage < 0 || data.noiseStage > 2) {
        return badOptional('noiseStage');
      }
      optional.noiseStage = data.noiseStage;
    }
    if (has('noiseWarning')) {
      const value = boolValue(data.noiseWarning);
      if (value === null) return badOptional('noiseWarning');
      optional.noiseWarning = value;
    }
    if (has('mode')) {
      if (typeof data.mode !== 'string' || !modes.has(data.mode)) return badOptional('mode');
      optional.mode = data.mode;
    }
    if (has('disagree')) {
      const value = boolValue(data.disagree);
      if (value === null) return badOptional('disagree');
      optional.disagree = value;
    }

    latest = {
      distanceCm: Object.fromEntries(ids.map(id => [id, data.distanceCm[id]])),
      noiseAmplitude: data.noiseAmplitude,
      radar: data.radar,
      ...optional,
      receivedAt: Date.now()
    };
    return json(res, 202, { accepted: true });
  } catch {
    return json(res, 400, { error: 'Invalid JSON packet' });
  }
}).listen(port, host, () => console.log(`Smart Library: http://${host}:${port}`));
