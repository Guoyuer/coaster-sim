# Reference Anchors

These links are development-only visual anchors for the Yarlung / Nyingchi
photo-real pass. Do not copy these images into product assets. Only download or
commit reference files when the license explicitly allows redistribution in this
public repository.

| ID | Anchor | Source | License note | Use |
|---|---|---|---|---|
| R1 | Yarlung Tsangpo river valley, Tibet | https://commons.wikimedia.org/wiki/File:Yarlung_Tsangpo_-_Tibet_-_01.jpg | Wikimedia Commons page; verify license before downloading | River corridor color, broad valley scale, distant haze |
| R2 | Yarlung Tsangpo Grand Canyon / Namcha Barwa context | https://commons.wikimedia.org/wiki/Category:Yarlung_Tsangpo_Grand_Canyon | Wikimedia Commons category; pick per-file licenses before downloading | Canyon wall silhouettes, snow mountain / cloud-band context |
| R3 | Namcha Barwa mountain views | https://commons.wikimedia.org/wiki/Category:Namcha_Barwa | Wikimedia Commons category; pick per-file licenses before downloading | Distant snow mountain shape and atmospheric layering |
| R4 | Nyingchi / Lulang forest valley references | https://commons.wikimedia.org/wiki/Category:Nyingchi | Wikimedia Commons category; pick per-file licenses before downloading | Humid green slopes, forest density, non-desert palette |
| R5 | Parlung Tsangpo / southeast Tibet river valley context | https://commons.wikimedia.org/wiki/Category:Parlung_Tsangpo | Wikimedia Commons category; pick per-file licenses before downloading | Turquoise / milky river and vegetated Himalayan valley cues |

## Local-only reference images (gitignored — NOT committed to this public repo)

Stored under `docs/refs/local/` (in `.gitignore`). Agents can `Read` these locally
to score screenshots, but they are NOT redistributed because their license is
unverified. Do not `git add` these files unless their license clearly permits
redistribution in a public repo.

| ID | File | Source / License | Use |
|---|---|---|---|
| L1 | `docs/refs/local/01_yarlung_valley_river_blossom.jpeg` | User-provided aerial photo; license UNVERIFIED (do not redistribute) | **Primary palette/atmosphere anchor.** Turquoise braided river + rapids/foam (D3), cloud bands + distant snow peaks + humid haze (D2), wet gray rock cliffs + river terraces (D1, D5), spring forest + farmland + peach blossom palette (D4). NOTE: this is a **broad open valley** composition — use for color/water/sky/material tone, but for the narrow deepest-gorge hero shot take the canyon **width/silhouette** (D1) from R2/R3, not from this wide-valley framing. |

Scoring use:

- D1 terrain: compare canyon wall and distant mountain silhouette against R2-R3 (narrow gorge) + L1 (rock/terrace detail).
- D2 sky / atmosphere: compare haze, cloud bands, and snow-mountain separation against L1 (best), R2-R3.
- D3 river: compare milky blue-green water and braided/rapid structure against L1 (best), R1, R5.
- D4 vegetation: compare slope density and cool humid greens against L1, R4-R5.
- D5 material / light: compare wet gray-green rock and clear-day contrast against L1 and all anchors.
