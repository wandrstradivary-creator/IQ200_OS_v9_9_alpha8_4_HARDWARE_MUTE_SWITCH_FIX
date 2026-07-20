IQ200 OS v9.6-alpha2 WEB COMMAND CENTER

Web UI additions:
- Full CommandManager registry shown by category.
- Search across command, alias, category and description.
- Generic command console: POST /api/command with cmd=<command>.
- Cross-core queue: 8 commands, maximum 191 characters each.
- Commands are executed by the same Core1 parser used by Serial.
- GET /api/commands returns all registered command metadata as JSON.
- GET /api/command/status returns queue and last-command status.
- Existing /api/play, /api/stop, /api/next, /api/prev, /api/scan remain compatible.
- Web volume slider uses the new command: volume 0..100.

Start:
  net ap
  web on
Then open:
  http://192.168.4.1/
