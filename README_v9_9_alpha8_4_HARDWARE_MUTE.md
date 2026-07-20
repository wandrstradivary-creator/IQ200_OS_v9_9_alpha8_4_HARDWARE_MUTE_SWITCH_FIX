# IQ200 OS v9.9-alpha8.4 — Hardware MUTE during station switching

- GPIO: `MUTE_PIN = 4`
- Active level: `MUTE_VAL = LOW`
- MUTE asserted before station stop/switch, DNS/TLS connect, buffering, EOF and reconnect.
- MUTE released non-blockingly 120 ms after the new decoder reports stable playback.
- Stop and fatal stream errors leave the output muted.
- Diagnostic log: `[WEBRADIO][MUTE] ON/OFF`.
