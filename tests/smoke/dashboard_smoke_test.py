#!/usr/bin/env python3
"""
Smoke test for a DarkflameServer build with the web dashboard.

Starts the whole server (master, auth, chat, worlds, dashboard) on a throwaway copy of the SQLite database, checks that
every server comes up and connects, then goes through the dashboard: every page and API route an operator can reach,
the same routes as a player and a moderator (they must be refused), live updates over the WebSocket, and a few security
rules. Stops the server and reports. Nothing touches your real database.

    python3 tests/smoke/dashboard_smoke_test.py                  # uses ./build and the database it is configured with
    python3 tests/smoke/dashboard_smoke_test.py --build out --fresh
    python3 tests/smoke/dashboard_smoke_test.py --attach http://127.0.0.1:2006 --user admin --password ...

--attach checks a server that is already running (for example one on MySQL) without starting or stopping anything.
It signs in with the given GM 9 account and creates two short-lived test accounts, which it deletes at the end.

Linux and macOS only (it drives MasterServer's password prompt through a pseudo-terminal). Exit code 0 means passed.
"""
import argparse
import base64
import http.cookiejar
import json
import os
import pty
import re
import secrets
import select
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

RESULTS = []


def check(name, ok, detail=''):
    RESULTS.append((name, bool(ok), detail))
    print(('  PASS  ' if ok else '  FAIL  ') + name + ('' if ok or not detail else '  (' + str(detail)[:300] + ')'))
    return ok


def section(title):
    print('\n== ' + title)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


class Client:
    """A browser-like session: cookie jar, and the X-Requested-With header the dashboard's scripts send."""

    def __init__(self, base):
        self.base = base
        self.jar = http.cookiejar.CookieJar()
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(self.jar), NoRedirect())

    def request(self, method, path, body=None, headers=None, csrf=True, timeout=30):
        data = json.dumps(body).encode() if body is not None else None
        all_headers = {'Content-Type': 'application/json'}
        if csrf:
            all_headers['X-Requested-With'] = 'smoke-test'
        all_headers.update(headers or {})
        req = urllib.request.Request(self.base + path, data=data, method=method, headers=all_headers)
        try:
            response = self.opener.open(req, timeout=timeout)
            return response.status, response.read(), response.headers
        except urllib.error.HTTPError as e:
            return e.code, e.read(), e.headers
        except (urllib.error.URLError, ConnectionError, socket.timeout) as e:
            return 0, str(e).encode(), {}

    def json(self, method, path, body=None, **kwargs):
        code, raw, headers = self.request(method, path, body, **kwargs)
        try:
            return code, json.loads(raw or b'{}')
        except ValueError:
            return code, {'_raw': raw[:200].decode(errors='replace')}

    def login(self, username, password):
        code, data = self.json('POST', '/api/auth/login', {'username': username, 'password': password})
        return code == 200 and data.get('success'), data

    def cookie(self):
        return '; '.join('%s=%s' % (c.name, c.value) for c in self.jar)


# ---- starting and stopping the server ----

def read_config(path):
    values = {}
    if os.path.exists(path):
        for line in open(path, encoding='utf-8', errors='replace'):
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, value = line.split('=', 1)
                values[key.strip()] = value.strip()
    return values


def port_open(host, port):
    with socket.socket() as s:
        s.settimeout(0.5)
        return s.connect_ex((host, port)) == 0


def create_admin(build, env, username, password):
    """Run 'MasterServer -a' in a pseudo-terminal and answer its prompts (getpass reads the terminal, not stdin)."""
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(build)
        os.execve(os.path.join(build, 'MasterServer'), ['MasterServer', '-a'], env)
    output, answered = b'', set()
    answers = [
        (b'Enter a username', username),
        (b'change the password', 'y'),
        (b'Enter a password', password),
        (b'Update admin privileges', 'y'),
        (b'level of privilege', '9'),
    ]
    deadline = time.time() + 120
    while time.time() < deadline:
        ready, _, _ = select.select([fd], [], [], 1)
        if ready:
            try:
                chunk = os.read(fd, 4096)
            except OSError:
                break
            if not chunk:
                break
            output += chunk
            for prompt, answer in answers:
                if prompt in output and prompt not in answered:
                    answered.add(prompt)
                    time.sleep(0.2)
                    os.write(fd, (answer + '\n').encode())
        finished, status = os.waitpid(pid, os.WNOHANG)
        if finished:
            break
    else:
        os.kill(pid, signal.SIGKILL)
    try:
        os.waitpid(pid, 0)
    except ChildProcessError:
        pass
    return b'Enter a password' in answered or b'Enter a password' in output, output.decode(errors='replace')


def server_processes(build):
    """PIDs of server processes started from this build directory."""
    pids = []
    for name in ('MasterServer', 'AuthServer', 'ChatServer', 'WorldServer', 'DashboardServer'):
        try:
            out = subprocess.run(['pgrep', '-f', os.path.join(build, name)], capture_output=True, text=True).stdout
            pids += [int(p) for p in out.split()]
        except FileNotFoundError:
            pass
    return pids


def stop_server(master, build):
    if master.poll() is None:
        master.send_signal(signal.SIGINT)
        try:
            master.wait(timeout=60)
        except subprocess.TimeoutExpired:
            master.kill()
    deadline = time.time() + 30
    while server_processes(build) and time.time() < deadline:
        time.sleep(1)
    leftovers = server_processes(build)
    for pid in leftovers:
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    return leftovers


# ---- WebSocket (just enough for one subscription) ----

def ws_frame(text):
    payload = text.encode()
    mask = secrets.token_bytes(4)
    header = bytes([0x81])
    if len(payload) < 126:
        header += bytes([0x80 | len(payload)])
    else:
        header += bytes([0x80 | 126]) + len(payload).to_bytes(2, 'big')
    return header + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload))


def ws_read(sock, deadline):
    def exact(n):
        data = b''
        while len(data) < n:
            sock.settimeout(max(0.1, deadline - time.time()))
            chunk = sock.recv(n - len(data))
            if not chunk:
                raise ConnectionError('closed')
            data += chunk
        return data
    head = exact(2)
    length = head[1] & 0x7F
    if length == 126:
        length = int.from_bytes(exact(2), 'big')
    elif length == 127:
        length = int.from_bytes(exact(8), 'big')
    return head[0] & 0x0F, exact(length)


def websocket_check(base, cookie, topic, wait_seconds, trigger=None):
    """Subscribe to a topic, run trigger() (something that should make the server send it), and wait for it."""
    url = urllib.parse.urlparse(base)
    sock = socket.create_connection((url.hostname, url.port or 80), timeout=10)
    try:
        key = base64.b64encode(secrets.token_bytes(16)).decode()
        sock.sendall(('GET /ws HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                      'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nCookie: %s\r\n\r\n' % (url.netloc, key, cookie)).encode())
        response = b''
        while b'\r\n\r\n' not in response:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
        if b' 101 ' not in response.split(b'\r\n')[0]:
            return False, 'no upgrade: ' + response.split(b'\r\n')[0].decode(errors='replace')
        sock.sendall(ws_frame(json.dumps({'event': 'subscribe', 'subscription': topic})))
        deadline = time.time() + wait_seconds
        triggered = trigger is None
        seen = []
        while time.time() < deadline:
            try:
                opcode, payload = ws_read(sock, deadline)
            except (socket.timeout, ConnectionError):
                break
            if opcode != 1:
                continue
            text = payload.decode(errors='replace')
            seen.append(text[:80])
            if not triggered and 'subscribed' in text:
                triggered = True
                trigger()
                continue
            try:
                message = json.loads(text)
            except ValueError:
                continue
            if message.get('event') == topic or message.get('subscription') == topic or topic in text[:200]:
                if 'status' not in message:
                    return True, text[:120]
        return False, 'messages: %s' % seen[:5]
    finally:
        sock.close()


# ---- the checks ----

def run_checks(base, admin_user, admin_password, started):
    admin = Client(base)

    section('Signing in')
    ok, data = admin.login(admin_user, admin_password)
    if not check('operator can sign in', ok, data):
        return None
    if data.get('twoFactorRequired'):
        check('operator account has no two-factor login (turn it off for the smoke test account)', False)
        return None

    section('Servers')
    deadline = time.time() + (90 if started else 5)
    status = {}
    while time.time() < deadline:
        code, status = admin.json('GET', '/api/status')
        worlds = status.get('worlds') or status.get('instances') or []
        if code == 200 and status.get('auth', {}).get('online') and status.get('chat', {}).get('online') and worlds:
            break
        time.sleep(2)
    check('auth server online', status.get('auth', {}).get('online'), status.get('auth'))
    check('chat server online', status.get('chat', {}).get('online'), status.get('chat'))
    check('at least one world server', len(status.get('worlds') or status.get('instances') or []) > 0, status.get('worlds'))

    # Test accounts at other levels, made through the API like a GM would
    suffix = secrets.token_hex(3)
    others = {}
    for level, name in ((0, 'smoke_player_' + suffix), (3, 'smoke_mod_' + suffix)):
        password = 'Smoke-' + secrets.token_hex(6)
        code, data = admin.json('POST', '/api/accounts/create', {'username': name, 'password': password, 'gm_level': level})
        if check('create GM %d test account' % level, code == 200 and data.get('success'), data):
            client = Client(base)
            if check('GM %d test account can sign in' % level, client.login(name, password)[0]):
                others[level] = client
    code, accounts = admin.json('POST', '/api/tables/accounts', {'draw': 1, 'start': 0, 'length': 100, 'search': 'smoke_'})
    test_ids = [row['id'] for row in accounts.get('data', []) if str(row.get('name', '')).endswith(suffix)]

    section('Pages')
    code, raw, _ = admin.request('GET', '/')
    html = raw.decode(errors='replace')
    check('home page', code == 200, code)
    # The page scripts read the viewer's permissions from here, one key per word
    can = re.search(r'data-can="([^"]*)"', html)
    check('page lists the permissions for scripts', can and {'accounts_view', 'settings'} <= set(can.group(1).split()), can and can.group(1)[:80])
    pages =sorted(set(re.findall(r'href="(/[a-z_]*)"', html)) - {'/', '/logout'})
    for page in pages:
        code, _, headers = admin.request('GET', page)
        if page == '/account':
            check('page /account goes to your own account', code == 302 and '/accounts/' in headers.get('Location', ''), code)
        else:
            check('page ' + page, code == 200, code)
    player = others.get(0)
    if player:
        staff_pages = [p for p in pages if p not in ('/account', '/api_docs', '/about', '/leaderboards', '/showcase', '/challenges')]
        refused = [p for p in staff_pages if player.request('GET', p)[0] not in (403, 302)]
        check('a player is refused every staff page', not refused, refused)

    section('API')
    code, docs = admin.json('GET', '/api/docs')
    routes = docs.get('routes', [])
    check('API docs list routes', code == 200 and len(routes) > 50, len(routes))
    server_errors, failures = [], []
    for route in routes:
        method, path = route['method'], route['path']
        if method != 'GET' or ':' in path:
            continue  # routes with an ID are covered by the table and page checks
        code, raw, _ = admin.request('GET', path)
        if code >= 500 or code == 0:
            server_errors.append('%s %d' % (path, code))
        elif code not in (200, 400, 404):
            failures.append('%s %d' % (path, code))
    check('every GET route without parameters answers without a server error', not server_errors, server_errors)
    check('every GET route is allowed for an operator', not failures, failures)
    tables = [r['path'] for r in routes if r['method'] == 'POST' and r['path'].startswith('/api/tables/')]
    bad = []
    for path in tables:
        code, data = admin.json('POST', path, {'draw': 3, 'start': 0, 'length': 5})
        if code != 200 or data.get('draw') != 3 or 'data' not in data:
            bad.append('%s %d' % (path, code))
    check('every table (%d) returns rows' % len(tables), not bad, bad)

    section('Permissions')
    for level, client in sorted(others.items()):
        code, mine = client.json('GET', '/api/account/permissions')
        allowed = mine.get('permissions', {}) if code == 200 else {}
        code, their_docs = client.json('GET', '/api/docs')
        visible = {(r['method'], r['path']) for r in their_docs.get('routes', [])}
        leaks = []
        for route in routes:
            method, path = route['method'], route['path']
            if method != 'GET' or ':' in path or (method, path) in visible or route.get('minGmLevel', 0) <= level:
                continue
            code, _, _ = client.request('GET', path)
            if code not in (401, 403):
                leaks.append('%s %d' % (path, code))
        check('GM %d is refused every route above its permissions' % level, not leaks, leaks)
        # A state-changing route as well
        code, _ = client.json('POST', '/api/permissions', {'key': 'accounts_ban', 'level': 1})
        check('GM %d cannot change permissions' % level, code == 403, code)

    section('Live updates')
    # Updates are only pushed when something changes, so make a change once subscribed: a new play key
    make_key = lambda: admin.json('POST', '/api/play_keys/create', {'count': 1, 'uses': 1, 'notes': 'smoke test'})
    ok, detail = websocket_check(base, admin.cookie(), 'table_changed', 20, make_key)
    check('WebSocket delivers table_changed after a change', ok, detail)
    ok, detail = websocket_check(base, admin.cookie(), 'dashboard_update', 20, make_key)
    check('WebSocket delivers dashboard_update after a change', ok, detail)

    section('Security rules')
    code, _, headers = admin.request('POST', '/api/auth/login', {'username': 'x', 'password': 'y'}, csrf=False)
    check('POST without X-Requested-With is refused', code == 403, code)
    code, _, headers = admin.request('GET', '/login')
    check('Content-Security-Policy header', 'frame-ancestors' in headers.get('Content-Security-Policy', ''), headers.get('Content-Security-Policy'))
    check('X-Frame-Options header', headers.get('X-Frame-Options') == 'DENY', headers.get('X-Frame-Options'))
    anonymous = Client(base)
    code, _, _ = anonymous.request('GET', '/api/permissions')
    check('signed-out API calls get 401', code == 401, code)
    code, _, headers = anonymous.request('GET', '/accounts')
    check('signed-out page visits go to the login page', code == 302 and '/login' in headers.get('Location', ''), code)

    section('Scheduled tasks')
    code, tasks = admin.json('GET', '/api/tasks')
    check('scheduler lists tasks', code == 200 and len(tasks.get('tasks', [])) >= 4, code)
    invalid = [t['name'] for t in tasks.get('tasks', []) if not t.get('valid', True)]
    check('every task schedule is valid', not invalid, invalid)
    return admin, test_ids


def main():
    parser = argparse.ArgumentParser(description='Start the server on a copy of the database and check the dashboard.')
    parser.add_argument('--build', default='build', help='build directory with the server binaries (default: build)')
    parser.add_argument('--fresh', action='store_true', help='start from an empty database instead of a copy of yours')
    parser.add_argument('--keep', action='store_true', help='keep the temporary database and logs')
    parser.add_argument('--attach', metavar='URL', help='check an already running dashboard instead of starting one')
    parser.add_argument('--user', help='GM 9 account for --attach')
    parser.add_argument('--password', help='its password (or set SMOKE_PASSWORD)')
    args = parser.parse_args()

    if args.attach:
        password = args.password or os.environ.get('SMOKE_PASSWORD')
        if not args.user or not password:
            parser.error('--attach needs --user and --password')
        result = run_checks(args.attach.rstrip('/'), args.user, password, started=False)
        if result:
            admin, test_ids = result
            for account_id in test_ids:
                admin.json('POST', '/api/accounts/%s/delete' % account_id, {})
        return summary()

    build = os.path.abspath(args.build)
    if not os.path.exists(os.path.join(build, 'MasterServer')):
        parser.error('no MasterServer in %s (use --build)' % build)
    shared = read_config(os.path.join(build, 'sharedconfig.ini'))
    dashboard = read_config(os.path.join(build, 'dashboardconfig.ini'))
    if shared.get('database_type', 'sqlite') != 'sqlite' and not args.fresh:
        parser.error('this build uses MySQL; start the server yourself and use --attach, or use --fresh for a throwaway SQLite database')
    port = int(os.environ.get('PORT', dashboard.get('port', '2006')) or 2006)
    host = dashboard.get('listen_ip', '127.0.0.1') or '127.0.0.1'
    if host in ('0.0.0.0', ''):
        host = '127.0.0.1'
    base = 'http://%s:%d' % (host, port)
    if port_open(host, port) or server_processes(build):
        parser.error('a server from this build (or something on port %d) is already running; stop it first' % port)

    work = tempfile.mkdtemp(prefix='dlu-smoke-')
    database = os.path.join(work, 'smoke.sqlite')
    source = os.path.join(build, shared.get('sqlite_database_path', 'resServer/dlu.sqlite'))
    if not args.fresh and os.path.exists(source):
        # A consistent copy even if something has the database open
        subprocess.run(['sqlite3', source, '.backup ' + database], check=False, capture_output=True)
        if not os.path.exists(database):
            shutil.copy(source, database)
    env = dict(os.environ, SQLITE_DATABASE_PATH=database, DATABASE_TYPE='sqlite', SKIP_ACCOUNT_CREATION='1', ENABLE_DASHBOARD='1')
    print('Build:    %s\nDatabase: %s (%s)\nWork dir: %s' % (build, database, 'empty' if args.fresh or not os.path.exists(source) else 'copy of ' + source, work))

    admin_user, admin_password = 'smoke_admin', 'Smoke-' + secrets.token_hex(8)
    section('Setup')
    ok, output = create_admin(build, env, admin_user, admin_password)
    if not check('create the operator account with MasterServer -a', ok, output[-400:]):
        return summary()

    log = open(os.path.join(work, 'server.log'), 'wb')
    master = subprocess.Popen([os.path.join(build, 'MasterServer')], cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        deadline = time.time() + 90
        while time.time() < deadline and not port_open(host, port) and master.poll() is None:
            time.sleep(1)
        if not check('dashboard is listening on %s' % base, port_open(host, port), 'master exited' if master.poll() is not None else 'timed out'):
            return summary()
        run_checks(base, admin_user, admin_password, started=True)
        section('Server stability')
        check('master server is still running', master.poll() is None, master.returncode)
    finally:
        section('Shutdown')
        leftovers = stop_server(master, build)
        check('every server stopped when master was stopped', not leftovers, leftovers)
        log.close()
        text = open(os.path.join(work, 'server.log'), errors='replace').read()
        route_errors = [line for line in text.splitlines() if 'Error handling' in line]
        check('no route threw an exception', not route_errors, route_errors[:5])
        if args.keep:
            print('\nKept %s' % work)
        else:
            shutil.rmtree(work, ignore_errors=True)
    return summary()


def summary():
    failed = [name for name, ok, _ in RESULTS if not ok]
    print('\n%d checks, %d failed' % (len(RESULTS), len(failed)))
    for name in failed:
        print('  - ' + name)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
