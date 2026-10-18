// Runs the sync server (../src/index.js) on this computer, with Node's built-in SQLite standing in for D1.
// For testing the apps and the server without Cloudflare:
//   node --no-warnings cloud/test/local-server.mjs [port] [database file]      (default 8787, in memory)
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DatabaseSync } from 'node:sqlite';
import worker from '../src/index.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const port = Number(process.argv[2] || 8787);
const sqlite = new DatabaseSync(process.argv[3] || ':memory:');
sqlite.exec(fs.readFileSync(path.join(here, '..', 'schema.sql'), 'utf8'));

// just enough of the D1 API for the server: prepare / bind / first / all / run / batch
class Statement {
    constructor(sql, params = []) { this.sql = sql; this.params = params; }
    bind(...params) { return new Statement(this.sql, params); }
    exec() {
        const s = sqlite.prepare(this.sql);
        if (/^\s*SELECT|\bRETURNING\b/i.test(this.sql)) {
            const results = s.all(...this.params);
            return { success: true, results, meta: { changes: /^\s*SELECT/i.test(this.sql) ? 0 : results.length } };
        }
        const r = s.run(...this.params);
        return { success: true, results: [], meta: { changes: Number(r.changes) } };
    }
    async first() { const r = this.exec().results; return r.length ? r[0] : null; }
    async all() { return this.exec(); }
    async run() { return this.exec(); }
}
const env = {
    DB: {
        prepare: (sql) => new Statement(sql),
        async batch(statements) {
            sqlite.exec('BEGIN');
            try {
                const out = statements.map((s) => s.exec());
                sqlite.exec('COMMIT');
                return out;
            } catch (e) {
                sqlite.exec('ROLLBACK');
                throw e;
            }
        },
    },
};

http.createServer(async (req, res) => {
    const chunks = [];
    for await (const c of req) chunks.push(c);
    const body = Buffer.concat(chunks);
    const request = new Request(`http://localhost:${port}${req.url}`, {
        method: req.method,
        headers: req.headers,
        body: req.method === 'GET' || req.method === 'HEAD' ? undefined : body,
    });
    const response = await worker.fetch(request, env);
    const out = Buffer.from(await response.arrayBuffer());
    res.writeHead(response.status, { ...Object.fromEntries(response.headers), 'Content-Length': out.length, Connection: 'close' });
    res.end(out);
}).listen(port, '127.0.0.1', () => console.log(`keycompass-sync (local) on http://127.0.0.1:${port}`));
