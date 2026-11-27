# Review of the two launch checks

One independent reviewer (a separate agent of this session) read
`git diff f41f9744 6bed5c43 -- src/` and the code around it on 27 November
2026 (UTC): both `.inc` files, `prepare_launch`, `games_card_route`, the
record headers and codec, and the combined stage's discovery. It was given
the intended behaviour and a list of places to attack, not the delivery
record or the test results. It ran `make test-games-native-cache` and
`make test-games-native-cache-hint`, and compiled `games_retail.c` on the
host with and without `KUI_VMU_GAME_CARDS`. Its findings are from reading;
none was reproduced by execution in the review. It was stopped before it
reached everything it planned (listed at the end).

The K-UI of `6bed5c43` had been built and packaged; it was held back for
this review and not sent.

## Findings and what was done

| # | Finding | Disposition |
|---|---|---|
| 1 | With Virtual VMU On a hint that is READY skips the preflight. The hint is the image's alias, or else the single record in the cache directory that fits, which can be another game's. Neither new refusal is then produced; the combined module is prepared, and the foreign record is published under this image's alias, so later launches take the same path. On the console the stage rejects the hint, rediscovers and takes the lease or stops. The same state starts the combined module for a game that should take the reader-only route; that part is older than the change. | Real. Corrected in `d41a04f4`: a combined module takes only the image's own indexed alias; the alias names of a combined module come from a domain of their own, so an alias written before is not read. Reproduced first: the route test with one saved record of another game on the card fails on the uncorrected launcher (it takes the record and goes on to publish) and passes on the corrected one. |
| 2 | SDK not recognized, Virtual VMU On, SCI family, reader-only module absent: the refusal offered "set Background loading Off", which with Virtual VMU still On ends in the Standard reader's refusal. | Real. Corrected in `71efce2d`: with Virtual VMU On the refusal names Background loading Off and Virtual VMU Off. |
| 3 | Pool too small, Virtual VMU On, SCI family, reader-only module absent: "Set Virtual VMU Off", then a second refusal for the missing module. | Real. Corrected in `71efce2d`: the launcher looks for the module first and asks for Virtual VMU Off only where the module then runs the game. |
| n1 | `native_cache_pool_small` scanned the whole module for the lease on every call (about 290 KB, several times a launch). | Corrected in `71efce2d`: the preflight reads the lease once from the descriptor it already found. The hint path reads it once for a combined module's alias. |
| n2 | The comment on the reader-only route said the launcher always says what to install or set; the arena-lease mismatch is still left to the stage. | Corrected in `71efce2d`. The arena case itself is unchanged. |
| n3 | A small-pool game that runs with the reader-only module repeats the full read and SDK scan on every launch. | As the unrecognized route already did. Not changed. |
| n4 | An unproved extent never tries the reader-only module. | Known and recorded (`background-launch-checks.md`); before the change it was a stage stop. Not changed. |
| n5 | The three new diagnostic lines were 104 to 115 characters and split mid-word at column 76. | Corrected in `71efce2d`: each fits one line. |
| n6 | A hash-valid record with a pool of 0 bytes answers "pool too small" and logs "0 KiB". | Fails safe. Not changed. |

## Checked by the reviewer and found clean

- No fall-through to the combined module for the two new answers once the
  preflight has run.
- The existing answers behave as before, including the Smash route, the
  Standard virtual card, the alias publish and the repeat record.
- The two call sites of `native_cache_refusal` for a module that is present
  but unreadable or a directory, an I/O error, Legacy (Y), and Virtual VMU On
  with a small pool and the module installed.
- Integers in the pool rule; saved records reach the same check as a fresh
  discovery; no write on a preflight refusal.
- `native_cache_hint_admitted` for a module that is not combined.
- Memory and resources on the new exits; `problem` is never NULL.
- The jump over `const char *refused` is legal C11.
- The strings: both first sentences are 63 characters, the longest whole
  message 151.
- The launcher's check against the stage's: no case found where a stage of
  the same source accepts what the launcher refuses.

## Not reached by the reviewer

- The scenario of finding 1 was not executed in the review (it was
  afterwards, see the table).
- No SH-4 compile; `test-games-native-route` and `tests/test_games_retail.py`
  were not run.
- The 426-program counts and the installed module builds were not checked.
- The test changes were only skimmed.
- The multi-disc session path was not checked.

## After the review

The two correction commits were tested (`host-checks.txt`) and the K-UI was
rebuilt from them. They were not reviewed a second time.
