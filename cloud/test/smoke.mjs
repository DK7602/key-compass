// Checks a running sync server (the live one after a deploy, or local-server.mjs) does what the apps expect.
// Uses a throw-away sync code and deletes it at the end.
//   node cloud/test/smoke.mjs https://keycompass-sync.<you>.workers.dev
const base = (process.argv[2] || 'http://127.0.0.1:8787').replace(/\/$/, '');
const alphabet = '23456789ABCDEFGHJKLMNPQRSTUVWXYZ';
const randomCode = () => Array.from(crypto.getRandomValues(new Uint8Array(20)), (b) => alphabet[b % 32]).join('');

let failures = 0;
function check(what, ok, detail) {
    console.log(`${ok ? '  ok  ' : 'FAILED'} ${what}${ok || detail === undefined ? '' : '  ->  ' + JSON.stringify(detail)}`);
    if (!ok) failures++;
}
async function call(op, code, body = {}) {
    const r = await fetch(`${base}/v1/${op}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${code}` },
        body: JSON.stringify(body),
    });
    return r.json();
}

const code = randomCode();
const pretty = code.match(/.{4}/g).join('-').toLowerCase();       // how a person might type it
const other = randomCode();

const health = await (await fetch(`${base}/v1/health`)).json();
check('health', health.ok === true, health);
check('a made-up code is refused', (await call('join', other)).error === 'unknown-code');
check('a malformed code is refused', (await call('join', 'hello')).error === 'bad-code');
{
    const viaBody = await fetch(`${base}/v1/create`, { method: 'POST', headers: { 'Content-Type': 'application/json' },
                                                       body: JSON.stringify({ auth: other }) });
    check('the code can come in the body instead of the header', (await viaBody.json()).ok === true);
    await call('forget', other);
}
check('create a new code', (await call('create', code)).created === true);
check('create again is fine', (await call('create', code)).ok === true);
check('join with it, typed with dashes', (await call('join', pretty)).ok === true);

const song = { id: 'test-song-1', kind: 'song', name: 'Smoke Test', modified: Date.now(), device: 'test', data: '<SongMap name="Smoke Test"/>' };
const put1 = await call('put', code, { ...song, baseRev: 0 });
check('save a new song', put1.ok && put1.rev === 1, put1);
const dup = await call('put', code, { ...song, baseRev: 0 });
check('saving it as new again is a conflict', dup.error === 'conflict' && dup.item && dup.item.rev === 1, dup);
const put2 = await call('put', code, { ...song, baseRev: 1, data: '<SongMap name="Smoke Test" v="2"/>' });
check('save a change', put2.ok && put2.rev === 2 && put2.seq > put1.seq, put2);
const stale = await call('put', code, { ...song, baseRev: 1, data: '<SongMap name="other device"/>' });
check('a change to an old version is a conflict', stale.error === 'conflict' && stale.item.rev === 2 && stale.item.data.includes('v="2"'), stale);

const all = await call('changes', code, { since: 0 });
check('changes since 0 has the song', all.ok && all.items.length === 1 && all.items[0].rev === 2 && all.seq >= put2.seq, all);
const none = await call('changes', code, { since: all.seq });
check('nothing after the latest change', none.ok && none.items.length === 0 && none.seq === all.seq, none);
check('another code sees nothing', (await call('create', other)).ok && (await call('changes', other, { since: 0 })).items.length === 0);

const del = await call('put', code, { ...song, baseRev: 2, deleted: true });
check('delete it', del.ok && del.rev === 3, del);
const after = await call('changes', code, { since: all.seq });
check('the delete reaches other devices', after.items.length === 1 && after.items[0].deleted === true && after.items[0].data === '', after);

const fav = await call('put', code, { id: 'favourites', kind: 'favourites', baseRev: 0, data: '[]', device: 'test' });
check('save the starred progressions', fav.ok, fav);
check('a bad id is refused', (await call('put', code, { ...song, id: 'no spaces!', baseRev: 0 })).error === 'bad-item');
check('a big song still fits', (await call('put', code, { ...song, id: 'big', baseRev: 0, data: 'x'.repeat(400_000) })).ok === true);

check('forget the code', (await call('forget', code)).ok === true);
check('it is gone', (await call('join', code)).error === 'unknown-code');
await call('forget', other);

console.log(failures ? `${failures} check(s) failed` : 'all server checks passed');
process.exit(failures ? 1 : 0);
