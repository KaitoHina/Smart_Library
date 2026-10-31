# Smart Library — Task List (Compressed, W10–W14)

Division of labour omitted — fill **Owner** after re-assigning.

Status: procurement arrived late; only IR sensor reading verified.
Decision already made in practice: seat sensor = analog `GP2Y0A21YK0F` (D1 closed).

| # | Main task | Final goal | Wk | Owner | Done |
|---|---|---|---|---|---|
| 1 | Technical freeze | Seat sensor type, radar interface, web scope, pin map — all locked and written into the report | 10 | | [ ] |
| 2 | Physical model | One desk + 4 seats built, sensors mounted, cables routed, labels on, ready for the demo bench | 10 | | [ ] |
| 3 | Seat detection | All 4 seats reliably report occupied/free after debounce, live free count correct | 11 | | [ ] |
| 4 | OLED dashboard | One screen shows free count, 2×2 seat map, noise level, radar state, plus all warning pages | 11 | Joe| [ ] |
| 5 | Noise monitoring & alerts | Classifies silent / moderate / noisy, ignores brief sounds, sustained noise → red warning at 5 s | 12 | | [ ] |
| 6 | Radar & sensor fusion | Radar cross-checks seat map; disagreement detected and shown as "sensor check" | 13 | | [ ] |
| 7 | Web dashboard | Phone opens a page showing the same live state as the OLED | 12 | Joe| [ ] |
| 8 | Firmware structure | Modular, single config point, no blocking code, 10 min continuous run without hang | 13 | | [ ] |
| 9 | Testing | Quantified table + plots of accuracy, false positives, response time, alert latency | 14 | | [ ] |
| 10 | Deliverables | Mid-term demo, 4 logbook entries, final presentation, YouTube video, final report all in | 9–14 | | [ ] |

## Week-by-week

| Week | Main line | Parallel | Weekend target |
|---|---|---|---|
| W10 | 1, 2 | 3, 4 | Model holds people, 4 LEDs follow IR, OLED shows free count |
| W11 | 3, 4 | 5 (wiring + baseline) | Seat subsystem complete, test data capture can start |
| W12 | 5 | 6 (radar wiring + presence) | Noise alert works end to end, radar reports presence |
| W13 | 6, 8 | 7 (if time), 9 (start) | Full demo runs end to end |
| W14 | 9 | 10 | Report + video delivered |

## Immediate actions (today/tomorrow)

1. Cut the model out (task 2) — sensors can be tested on cardboard.
2. Wire the OLED and run the `Adafruit_SSD1306` example (task 4) — fastest visible result.
3. Group meeting to close radar interface and web scope (task 1) — undecided now means a second slip in W12.
