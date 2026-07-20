# IQ200 OS v9.8-alpha10 WEB MUSIC BROWSER

- Web Library tab with active playlist tracks
- Pagination: 50 tracks per page
- Search by filename/folder/path
- Direct Play for a selected playlist index
- Core0-safe decoder stop/select/start handoff
- REST: GET /api/library?offset=0&limit=50&q=...
- REST: POST /api/library/play index=N

Test: open Web UI, Library, search, page, press Play.
