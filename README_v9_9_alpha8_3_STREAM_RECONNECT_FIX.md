# IQ200 OS v9.9-alpha8.3 — WebRadio stream reconnect fix

- Transient HTTPS/DNS/ICY connection failures no longer leave the radio in a permanent ERROR state.
- Failed reconnects are retried indefinitely with bounded exponential backoff (1–15 seconds).
- Wi-Fi loss retries after at least 3 seconds and resumes the selected station automatically.
- EOF marks the old session inactive before reconnecting.
- Invalid URL and missing Audio allocation remain fatal errors.

Target stream used for the fix:
`https://cast.brg.ua/shanson_main_public_mp3_hq`
