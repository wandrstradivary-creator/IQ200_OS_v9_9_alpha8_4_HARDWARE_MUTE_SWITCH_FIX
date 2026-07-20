# IQ200 OS v8.1.5 FAVORITES UI X10THINK

Added SD-backed Favorites foundation:

- `src/services/FavoriteManager.h`
- `/iq200/db/favorites.db`
- commands: `favorites/fav`, `favadd`, `favload`, `favsave`, `favclear`
- Favorites screen shows count, last path, and DB status
- favorites are loaded/saved without SD rescan
- RT Audio untouched

Test flow:

```text
fav
favadd
fav
favsave
favload
favclear
```
