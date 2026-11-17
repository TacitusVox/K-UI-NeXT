# Broadband Adapter readiness

Reviewed against main `c6a07a130791` and the KallistiOS revision in `dependencies.json`, `fcfa7d869471`. The Dreamcast expansion-port adapters and the serial-port adapters take different paths through K-UI.

| K-UI operation | Broadband Adapter / LAN Adapter | W5500 on SCI | ESP32-C5 on SCI |
| --- | --- | --- | --- |
| Inspect adapter | Driver identity and saved/current addressing. An initialized BBA also supplies carrier status. | Chip, SPI speed, MAC and cable link | Board, firmware and Wi-Fi status |
| Test network | Temporary DHCP, address-conflict check, gateway ARP and ping when a gateway is advertised | Same tests over raw Ethernet | Wi-Fi association, DNS lookup and board-reported network time |
| FTP and browser sharing | No socket backend | Implemented | Implemented |
| Disc Ripper → PC volume | No socket backend | Implemented | Implemented |
| Download missing cover art | No client | No client | No client |

On the BBA, Inspect can identify a disconnected adapter. The Test action starts the driver on the I/O worker and releases the network session afterward. A DHCP ACK proves the console reached its DHCP server. A gateway that answers ARP can still filter ping. An advertised DNS address alone gives no evidence that a name resolved. SCI storage can remain selected for the BBA test. The serial adapters need the SCI port for themselves.

## What the older branches contribute

The review compared the networking and cover sources across the 114 pre-existing remote branch tips, grouping identical files, then traced the distinct implementations and their callers. The scope was the K-UI networking and cover paths, including the PC receiver and the C5 bridge.

| Historical reference | Finding on main |
| --- | --- |
| `milestone/m15-shell` | The BBA/LAN diagnostic grew into the current raw-frame test. K-UI restores the previous receive callback and discards the acquired address when the test ends. |
| `codex/ftp-close-storm` at `191e715c` | The W5500 close handling is already in main's ancestry. Preserve its bounded socket cleanup and the hardware-tested transfer path. |
| `claude/wifi-esp32c5-firmware` and `milestone/wifi-performance-test` at `f430f267` | Main carries the later Wi-Fi pacing and listener-readiness work. These board-specific mechanisms do not provide BBA sockets. |
| `codex/storage-transports` and `codex/3.0-medium-foundations-5c411e32` | PC capture uses outbound TCP to one configured IP. Browser sharing serves local exports to an incoming client. Neither supplies an HTTP response reader or TLS client. |

## Changes on the audit branch

- **BBA and LAN diagnostics.** Inspection now fits the identity, link and address fields on the eight-row results panel. The connection test retains the same packet across a temporary BBA transmit stall, with a bounded wait and a working cancel path. LAN short-frame padding is zeroed through the length the pinned driver actually copies. The diagnostic owns its input callback throughout driver startup and teardown.
- **W5500 lease maintenance.** FTP, browser sharing and PC capture use the session's renewal timing. The console unicasts a renewal to the issuing server, broadcasts when rebinding, accepts a replacement server for the same address and stops using an expired lease. This matters on sessions long enough to outlive the router's first renewal interval. [DHCP renewal and rebinding, RFC 2131 §4.4.5](https://datatracker.ietf.org/doc/html/rfc2131#section-4.4.5)
- **File transfers.** FTP protects in-progress destinations and staging files across concurrent clients, including FAT32 short-name aliases. Browser sharing enforces the request deadline even when additional bytes are waiting. The Wi-Fi test reports a canceled lookup/time sequence as stopped.
- **Cover replacement.** A replacement keeps a separate copy of the prior validated record, writes and syncs the new pixels, then publishes the new header. The reader can recover the old pixels after an interrupted update. Ordinary misses still take one record lookup. An explicit rescan repairs an interrupted replacement.

## Verification

| Check | Coverage |
| --- | --- |
| Dreamcast GUI | Cross-link the SD runtime with the pinned KOS and SH-4 GCC 15.2.0. |
| Ethernet tests | Simulated ownership, re-detection, link startup, cancellation, busy transmit queue and LAN padding. |
| W5500 and C5 | DHCP renewal/rebinding/expiry, Wi-Fi status, listener lifecycle and firmware bridge pacing. |
| FTP and browser | Real loopback TCP with simulated adapters and FatFs media, overlapping FTP writes, partial browser requests, transfer cleanup and clean service stop. |
| PC capture | 16 receiver tests and four integration tests, including capture, stop, resume, verify and independent readback on FAT32 and exFAT. |
| Games | 32 cover cases and 104 catalogue cases across FAT32 and exFAT. The replacement tests inject write and sync failures and compare the saved pixels. |

The review has no physical BBA run or throughput measurement. File-level failure injection also leaves physical SD power-loss behavior unmeasured.

## First run with the BBA

1. Boot with your usual storage and an Ethernet cable connected to your DHCP router. Open **Network → Inspect adapter**. Record the adapter identity. A cold inspection can show **Link: not checked**.
2. Select **Test network** and save the assigned IPv4 address, gateway ARP result, ping result and receive-queue drops. If the gateway filters ICMP, keep the DHCP and ARP results as separate observations.
3. In the same boot, cancel a second run and start it again. Repeat once with the cable unplugged, reconnect it and run the test again. The upstream link wait can take up to ten seconds before cancellation takes effect. Check that Home and the Games list open afterward.

One issue remains in the pinned KOS BBA driver. It enables receive interrupts before its receive worker exists. On teardown it destroys that worker before disabling reception, and the driver exposes no public operation to drain an outstanding BBA receive DMA completion. The host lifecycle tests cannot exercise that ordering on a live G2 bus. A repeatable freeze at start, cancel or exit is grounds to stop the rollout and fix the driver. [Pinned BBA implementation](https://github.com/KallistiOS/KallistiOS/blob/fcfa7d869471591ca1c777543261a7bfea7cb726/kernel/arch/dreamcast/hardware/network/broadband_adapter.c)

## Download missing artwork

1. **Prove one outbound BBA connection first.** On explicit user action, open a cancellable session on the existing worker, obtain an address, resolve a hostname and stream a known file from a LAN test server. Compare its length and digest, then retry after a disconnect. This tests the missing transport independently of artwork matching. Keep networking off the boot and Games-list paths.
2. **Select a provider and prove one HTTPS image request.** Confirm the provider's documented lookup keys, thumbnail sizes, request limits and use terms. Budget TLS, validate the certificate and hostname with a usable clock and trust store, and measure peak memory on the console. Give connect, read and cancel explicit deadlines. The pinned KOS TCP implementation does not supply ordinary desktop socket timeout behavior. [Pinned TCP socket implementation](https://github.com/KallistiOS/KallistiOS/blob/fcfa7d869471591ca1c777543261a7bfea7cb726/kernel/net/net_tcp.c)
3. **Decide what is missing from the record.** A valid 512-byte `.kcv` with source `NONE` contains title metadata only. Respect an explicit cover assignment, the user's loose PNG/JPEG and existing disc art. Match the validated product number and region to a provider entry, using revision or disc information for ambiguous releases. Skip unresolved matches. Keep downloaded art identifiable separately from user art and from a failed or deferred lookup.
4. **Use the current decoder and cache.** Stream to a bounded temporary file and enforce the existing 3 MiB encoded and 1.2-megapixel decoded limits. The cache already produces 160, 104 and 56 pixel RGB565 thumbnails. Publish the result only after validation, invalidate cached misses and leave ordinary browsing offline. A second missing-art pass should make zero requests for already covered games.
