// Key Compass cloud sync - a Cloudflare Worker in front of a D1 database (see ../schema.sql).
//
// Every device that knows the same sync code shares one set of songs (and starred progressions).
// The apps talk to it with POST requests carrying JSON, the code in an "Authorization: Bearer <code>"
// header or as "auth" in the JSON, and always get JSON back with "ok": true or false (and "error" saying why).
//
//   POST /v1/create   make the account for a new code (fine to repeat)
//   POST /v1/join     check a code typed on another device: { ok } or { ok: false, error: "unknown-code" }
//   POST /v1/changes  { since }  ->  { seq, items: [...], more }   everything changed after change number 'since'
//   POST /v1/put      { id, kind, baseRev, name, modified, device, deleted, data }  ->  { rev, seq }
//                     or { ok: false, error: "conflict", item } when another device saved first
//   POST /v1/forget   delete the account and everything in it
//   GET  /v1/health   { ok, version }

const VERSION = 1;
const CODE_ALPHABET = /^[23456789ABCDEFGHJKLMNPQRSTUVWXYZ]{20}$/;
const ID_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;
const KINDS = new Set(['song', 'favourites']);
const MAX_BODY = 1_600_000;          // bytes of JSON in one request (a song is usually 5-50 KB)
const MAX_DATA = 1_500_000;          // characters of song data (D1 rows are limited to 2 MB)
const MAX_ITEMS = 5000;              // songs per account
const PAGE_ITEMS = 100;              // items per /changes answer
const PAGE_BYTES = 3_000_000;        // ... or this much data, whichever comes first

export default {
    async fetch(request, env) {
        try {
            return await handle(request, env);
        } catch (e) {
            console.error('unhandled', e && e.stack || e);
            return json({ ok: false, error: 'server-error' }, 500);
        }
    },
};

async function handle(request, env) {
    const url = new URL(request.url);

    if (request.method === 'GET' && (url.pathname === '/' || url.pathname === '/v1/health'))
        return json({ ok: true, service: 'keycompass-sync', version: VERSION });

    if (request.method !== 'POST' || !url.pathname.startsWith('/v1/'))
        return json({ ok: false, error: 'not-found' }, 404);

    const length = Number(request.headers.get('Content-Length') || 0);
    if (length > MAX_BODY)
        return json({ ok: false, error: 'too-big' });

    let body = {};
    const text = await request.text();
    if (text.length > MAX_BODY)
        return json({ ok: false, error: 'too-big' });
    if (text.trim()) {
        try { body = JSON.parse(text); } catch { return json({ ok: false, error: 'bad-json' }); }
    }
    if (typeof body !== 'object' || body === null || Array.isArray(body))
        return json({ ok: false, error: 'bad-json' });

    // the code comes in the Authorization header, or as "auth" in the body (some platforms mangle headers)
    const code = normaliseCode((request.headers.get('Authorization') || '').replace(/^\s*Bearer\s+/i, ''))
              || normaliseCode(body.auth);
    if (!code)
        return json({ ok: false, error: 'bad-code' });

    const account = await accountId(code);
    const db = env.DB;
    const now = Date.now();

    switch (url.pathname) {
        case '/v1/create': {
            const res = await db.prepare('INSERT OR IGNORE INTO accounts (id, seq, created, last_seen) VALUES (?1, 0, ?2, ?2)')
                                .bind(account, now).run();
            return json({ ok: true, created: (res.meta && res.meta.changes) > 0 });
        }

        case '/v1/join': {
            const row = await touch(db, account, now);
            return row ? json({ ok: true, seq: row.seq }) : json({ ok: false, error: 'unknown-code' });
        }

        case '/v1/changes': {
            const row = await touch(db, account, now);
            if (!row) return json({ ok: false, error: 'unknown-code' });
            const since = toInt(body.since, 0);
            const { results } = await db.prepare(
                'SELECT id, kind, name, rev, seq, modified, device, deleted, data FROM items ' +
                'WHERE account = ?1 AND seq > ?2 ORDER BY seq LIMIT ?3').bind(account, since, PAGE_ITEMS + 1).all();
            const items = [];
            let bytes = 0;
            for (const r of results.slice(0, PAGE_ITEMS)) {
                if (items.length > 0 && bytes + r.data.length > PAGE_BYTES) break;
                bytes += r.data.length;
                items.push(itemOut(r));
            }
            const more = items.length < results.length;
            return json({ ok: true, seq: more ? items[items.length - 1].seq : row.seq, items, more });
        }

        case '/v1/put': {
            const row = await db.prepare('SELECT seq FROM accounts WHERE id = ?1').bind(account).first();
            if (!row) return json({ ok: false, error: 'unknown-code' });

            const id = String(body.id || '');
            const kind = String(body.kind || 'song');
            const baseRev = toInt(body.baseRev, -1);
            const deleted = body.deleted ? 1 : 0;
            const name = String(body.name || '').slice(0, 200);
            const device = String(body.device || '').slice(0, 40);
            const modified = toInt(body.modified, now);
            const data = deleted ? '' : String(body.data || '');
            if (!ID_PATTERN.test(id) || !KINDS.has(kind) || baseRev < 0)
                return json({ ok: false, error: 'bad-item' });
            if (data.length > MAX_DATA)
                return json({ ok: false, error: 'too-big' });

            if (baseRev === 0) {
                const count = await db.prepare('SELECT COUNT(*) AS n FROM items WHERE account = ?1').bind(account).first();
                if (count && count.n >= MAX_ITEMS)
                    return json({ ok: false, error: 'too-many' });
            }

            // one transaction: take the next change number, then save only if nobody else saved first
            const save = baseRev === 0
                ? db.prepare('INSERT INTO items (account, id, kind, name, rev, seq, modified, device, deleted, data) ' +
                             'SELECT ?1, ?2, ?3, ?4, 1, (SELECT seq FROM accounts WHERE id = ?1), ?5, ?6, ?7, ?8 ' +
                             'WHERE NOT EXISTS (SELECT 1 FROM items WHERE account = ?1 AND id = ?2) RETURNING rev, seq')
                     .bind(account, id, kind, name, modified, device, deleted, data)
                : db.prepare('UPDATE items SET kind = ?3, name = ?4, rev = rev + 1, seq = (SELECT seq FROM accounts WHERE id = ?1), ' +
                             'modified = ?5, device = ?6, deleted = ?7, data = ?8 ' +
                             'WHERE account = ?1 AND id = ?2 AND rev = ?9 RETURNING rev, seq')
                     .bind(account, id, kind, name, modified, device, deleted, data, baseRev);
            const results = await db.batch([
                db.prepare('UPDATE accounts SET seq = seq + 1, last_seen = ?2 WHERE id = ?1').bind(account, now),
                save,
            ]);
            const saved = results[1].results && results[1].results[0];
            if (saved)
                return json({ ok: true, rev: saved.rev, seq: saved.seq });

            const current = await db.prepare(
                'SELECT id, kind, name, rev, seq, modified, device, deleted, data FROM items WHERE account = ?1 AND id = ?2')
                .bind(account, id).first();
            return json({ ok: false, error: 'conflict', item: current ? itemOut(current) : null });
        }

        case '/v1/forget': {
            await db.batch([
                db.prepare('DELETE FROM items WHERE account = ?1').bind(account),
                db.prepare('DELETE FROM accounts WHERE id = ?1').bind(account),
            ]);
            return json({ ok: true });
        }
    }
    return json({ ok: false, error: 'not-found' }, 404);
}

// the account row, noting (at most hourly, to save database writes) that it is still in use
async function touch(db, account, now) {
    const row = await db.prepare('SELECT seq, last_seen FROM accounts WHERE id = ?1').bind(account).first();
    if (row && now - row.last_seen > 3_600_000)
        await db.prepare('UPDATE accounts SET last_seen = ?2 WHERE id = ?1').bind(account, now).run();
    return row;
}

// "kcs7-h3qm 9xd2..." -> "KCS7H3QM9XD2...", or '' if it can't be a code
export function normaliseCode(raw) {
    const code = String(raw || '').toUpperCase().replace(/[^0-9A-Z]/g, '');
    return CODE_ALPHABET.test(code) ? code : '';
}

async function accountId(code) {
    const bytes = new TextEncoder().encode('keycompass-sync:' + code);
    const hash = await crypto.subtle.digest('SHA-256', bytes);
    return [...new Uint8Array(hash)].map(b => b.toString(16).padStart(2, '0')).join('');
}

function itemOut(r) {
    return { id: r.id, kind: r.kind, name: r.name, rev: r.rev, seq: r.seq, modified: r.modified,
             device: r.device, deleted: !!r.deleted, data: r.data };
}

function toInt(v, fallback) {
    const n = Number(v);
    return Number.isFinite(n) ? Math.trunc(n) : fallback;
}

function json(obj, status = 200) {
    return new Response(JSON.stringify(obj), {
        status,
        headers: { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' },
    });
}
