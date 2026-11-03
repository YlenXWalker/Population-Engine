# BanquetOfHeroes pre-renewal population preset

Optional complete replacements for the three corresponding files in `db/`.
The default Population Engine databases retain Renewal and third-job support.

| Job group | Ambient target |
| --- | ---: |
| Novice | 200 |
| First jobs | 800 |
| Second jobs | 1,400 |
| Rebirth / transcended jobs | 1,200 |
| Expanded pre-renewal jobs | 400 |
| **Total** | **4,000** |

The rebirth group contains 100 High Novices, 200 High First jobs and 900
transcendent Second jobs. Expanded jobs contain 80 Super Novices, 160
Taekwon/Star Gladiators/Soul Linkers, 80 Gunslingers and 80 Ninjas.
Placement targets: 1,040 town, 1,555 field and 1,405 dungeon shells.
Job choices inside a shared profile are randomized, not exact per-job quotas.

## Apply

1. Build the map-server in pre-renewal mode (`PRERE` / `--disable-renewal`).
2. Stop the map-server and back up `db/population_engine.yml`,
   `db/population_spawn.yml`, and `db/population_pvp.yml`.
3. Copy the three YAML files from this directory over those files in `db/`.
4. Review `db/import/population_*.yml` for overrides. These still load last.
5. Keep `population_engine_max_count` above 4,000 to allow vendors, companions,
   fixed posts and arena fighters. The default cap is 5,000.
6. Restart and let autosummon fill the targets in batches.

Do **not** merely import this preset after the default databases: an overlay
does not remove older job mappings or the default spawn profiles. The files
are intentional replacements and do not import the Renewal-only third jobs.
Gear sets and skills continue to use the standard shared databases.
Vendors, companions, arena teams and fixed posts are additional to the target.
Capacity and passable placement still constrain actual live population.

Restoring the backed-up files restores the standard mode-aware catalog.