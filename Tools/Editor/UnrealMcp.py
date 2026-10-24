"""Minimal localhost MCP client for UE's native server; no third-party packages.
python Tools/Editor/UnrealMcp.py list
python Tools/Editor/UnrealMcp.py call <tool-name> <arguments-json-file>
"""
import json
from pathlib import Path
import sys
import urllib.request


def request(method, params, session=None, request_id=1):
    payload = dict(jsonrpc="2.0", method=method, params=params)
    if request_id is not None: payload["id"] = request_id
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if session:
        headers["Mcp-Session-Id"] = session
        headers["MCP-Protocol-Version"] = "2025-11-25"
    req = urllib.request.Request("http://127.0.0.1:8000/mcp", json.dumps(payload).encode(), headers)
    with urllib.request.urlopen(req, timeout=180) as response:
        body = response.read().decode("utf-8")
        if body.startswith("event:") or body.startswith("data:"):
            body = next(line[5:].strip() for line in body.splitlines() if line.startswith("data:"))
        return (json.loads(body) if body.strip() else {}, response.headers.get("Mcp-Session-Id", session))


def call(method, params):
    init, session = request("initialize", dict(protocolVersion="2025-11-25", capabilities={},
                                               clientInfo=dict(name="RoadToWisdomEditorTools", version="1.0")))
    request("notifications/initialized", {}, session, request_id=None)
    result, _ = request(method, params, session, request_id=2)
    return result


if __name__ == "__main__":
    if sys.argv[1] == "list":
        print(json.dumps(call("tools/list", {}), ensure_ascii=False, indent=2))
    elif sys.argv[1] == "call":
        args = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8-sig"))
        print(json.dumps(call("tools/call", dict(name=sys.argv[2], arguments=args)), ensure_ascii=False, indent=2))