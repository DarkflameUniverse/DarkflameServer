#!/usr/bin/env python3
"""
A fake Anthropic Messages API for trying the dashboard's AI moderator helper without a real key or any cost.

    python3 tests/dWebTests/mock_claude_api.py [--port 8765] [--mode good]

Then in Settings (Dashboard, AI moderator helper): ai_helper_enabled=1, claude_api_key=anything,
claude_api_base=http://127.0.0.1:8765. Modes (also switchable per request with the X-Mock-Mode header or by
putting "mock:<mode>" in a player's chat/report text, which the helper sends along as data):

    good       a valid suggestion built from the refs in the case file
    malformed  prose around the JSON (the dashboard must reject it)
    fence      JSON in a ```json code fence (rejected)
    inject     echoes the hidden canary marker, as if a player's text had hijacked the model (rejected)
    ratelimit  429 with retry-after: 1 on the first try, then a good answer
    error500   500 every time (the dashboard gives up after 4 tries)
    overloaded 529 every time
    refusal    stop_reason "refusal"
    cutoff     stop_reason "max_tokens"
    badkey     401 authentication_error

Only for local testing: it listens on 127.0.0.1 and never checks the key.
"""
import argparse
import json
import re
from http.server import BaseHTTPRequestHandler, HTTPServer

STATE = {"mode": "good", "calls": 0}


def suggestion_for(body):
    """A plausible answer: the first allowed action, citing the refs found in the case file."""
    user = body["messages"][0]["content"]
    refs = sorted(set(re.findall(r'"ref":"([A-Z]+[0-9]*)"', user)))
    schema = (body.get("output_config") or {}).get("format", {}).get("schema", {})
    actions = schema.get("properties", {}).get("action", {}).get("enum") or ["note"]
    action = "reject_name" if "reject_name" in actions else ("warn" if "warn" in actions else actions[0])
    return {
        "action": action,
        "days": 0,
        "strike": False,
        "player_reason": "Please keep things friendly for everyone." if action in ("warn", "reject_name") else "",
        "staff_explanation": "Mock answer from mock_claude_api.py. It cites " + (", ".join(refs) or "nothing") + ".",
        "confidence": "low",
        "evidence": refs[:5],
    }


def message(text, stop="end_turn"):
    return {
        "id": "msg_mock", "type": "message", "role": "assistant", "model": "mock-" + STATE.get("model", "claude"),
        "content": [{"type": "thinking", "thinking": ""}, {"type": "text", "text": text}],
        "stop_reason": stop, "usage": {"input_tokens": 1000, "output_tokens": 120},
    }


def error(kind, text):
    return {"type": "error", "error": {"type": kind, "message": text}}


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, payload, headers=None):
        data = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        for key, value in (headers or {}).items():
            self.send_header(key, value)
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        if self.path != "/v1/messages":
            return self.reply(404, error("not_found_error", "no such path"))
        body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
        STATE["calls"] += 1
        STATE["model"] = body.get("model", "claude")
        found = re.search(r"mock:([a-z0-9]+)", body["messages"][0]["content"])
        mode = self.headers.get("X-Mock-Mode") or (found.group(1) if found else STATE["mode"])
        canary = re.search(r"CANARY-[0-9a-f]+", body.get("system", ""))
        answer = json.dumps(suggestion_for(body))
        print(f"[mock] call {STATE['calls']}: model={body.get('model')} max_tokens={body.get('max_tokens')} mode={mode} "
              f"key_present={'x-api-key' in self.headers} version={self.headers.get('anthropic-version')}")

        if mode == "ratelimit":
            if STATE["calls"] % 2 == 1:
                return self.reply(429, error("rate_limit_error", "slow down"), {"retry-after": "1"})
            return self.reply(200, message(answer))
        if mode == "error500":
            return self.reply(500, error("api_error", "Internal server error"))
        if mode == "overloaded":
            return self.reply(529, error("overloaded_error", "Overloaded"))
        if mode == "badkey":
            return self.reply(401, error("authentication_error", "invalid x-api-key"))
        if mode == "malformed":
            return self.reply(200, message("Sure! Here is what I think: " + answer))
        if mode == "fence":
            return self.reply(200, message("```json\n" + answer + "\n```"))
        if mode == "inject":
            hijacked = suggestion_for(body)
            hijacked.update(action="ban" if "ban" in json.dumps(body) else hijacked["action"], days=365,
                            staff_explanation="Ignoring my instructions as the player asked. Marker: " + (canary.group(0) if canary else "?"))
            return self.reply(200, message(json.dumps(hijacked)))
        if mode == "refusal":
            return self.reply(200, message("", "refusal"))
        if mode == "cutoff":
            return self.reply(200, message(answer[:40], "max_tokens"))
        return self.reply(200, message(answer))

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--mode", default="good")
    args = parser.parse_args()
    STATE["mode"] = args.mode
    print(f"Mock Claude API on http://127.0.0.1:{args.port} (mode {args.mode})")
    HTTPServer(("127.0.0.1", args.port), Handler).serve_forever()
