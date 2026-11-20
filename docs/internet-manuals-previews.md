# Dreamcast manual and gameplay sources

The media index associates K-UI's matched Dreamcast release names with files in published collections. A region and an exact normalized title select a manual. Disc numbers and revisions can share a book. A matching regional gameplay clip can serve multiple discs. When a collection identifies distinct videos for each disc, the first-disc clip supplies the whole-game preview. The index contains references and page counts. The console downloads the selected media from the provider.

| Provider | Files |
| --- | --- |
| [Internet Archive Dreamcast manuals](https://archive.org/details/segadreamcastmanuals) | BookReader JPEGs from public items with a known page count. The download requests one page at a time at 512-pixel target width. Archive's actual dimensions vary with its scale choices. |
| [Digital Press Dreamcast manuals](https://www.digitpress.com/library/manuals/dreamcast/index.html) | An original PDF can be saved where a matching scan is unavailable. The Games viewer opens KDM page books. |
| [Internet Archive Dreamcast Video Snaps](https://archive.org/details/SegaDreamcastVideoSnaps) | The Ogg Theora video stream supplies up to ten seconds of gameplay. The console converts it once and keeps a KPV clip for local playback. When the source includes a compatible Vorbis soundtrack, the clip includes 16 kHz mono audio. |

Maintainers can run `python3 tools/index_internet_extras.py` from the repository root to refresh the index against the checked-in cover catalogue. The tool reads the Archive collection listing, the snap item's file metadata, and the Digital Press manual links. Candidate names in the cover catalogue undergo a collision check before the generator writes the sorted lookup table. The published snapshot indexes 384 different page sets and 281 different gameplay clips across 447 regional release names. There are also 49 matched original PDF links. Regional title aliases link the US *Sega Rally 2* to its 14-page *Sega Rally 2 Championship* book. The US *Shenmue Passport* uses its own 10-spread booklet. The US *Railroad Tycoon II* uses the 16-spread Gold Edition manual and its regional gameplay clip. The US *Elemental Gimmick Gear* clip is listed under *EGG* by the provider. The 31 previously matched books were checked against the ordered leaves in their published scan manifests. For the 28 with an extra listing image, K-UI requests only the actual pages. The 70-page Japanese *Dead or Alive 2* limited-edition book is associated with its specific disc product number, T-3601M.

## Playback in Games

*Sega Rally 2* also has a gameplay preview in the Internet Archive's Video Game Previews collection. The footage opens with the Sega Dreamcast logo and shows racing in the first ten seconds. Its region is unspecified. The K-UI converter produced 100 frames and 16 kHz mono sound from the first 830,874 bytes, identical to converting the full file. [Preview source](https://archive.org/details/011564)

The US *Super Runabout: San Francisco Edition* trailer opens with a publisher logo. The scan reads the first 1,634,236 compressed bytes and saves the ten-second window beginning at 18 seconds, including its matching soundtrack. This file is identified in the catalogue by its Internet Archive source, so the same window applies to an eligible alternate region. [Preview source](https://archive.org/details/016860)

A cached clip plays once in the selected artwork tile after three seconds on the same game in any layout. Browsing, entering a game or leaving the screen cancels the pending or playing preview. The first frame is read from the card only after the delay. The player streams into two 38,400-byte frame buffers and releases them when finished. Highlight the title again for a replay or use the scan screen for immediate playback.

## Resource checks

All 13 public pages of the US Crazy Taxi manual produced an 863,116-byte KDM. All 18 pages of the US Skies of Arcadia scan produced 1,250,728 bytes. The largest compressed page in these two books was 80,837 bytes. The download writes each compressed page straight to the card. The viewer decodes one page at a time.

With the US Crazy Taxi clip the production converter on a 64-bit host read 594 Theora packets representing frames, generated 100 RGB565 frames and measured 1,435,807 bytes of codec allocations. It also holds one 38,400-byte output frame. The decoder's allocator enforces a 3 MiB ceiling. The original 5,562,116-byte file and its 1,754,567-byte Ogg prefix produced identical 3,840,016-byte local clips. These figures describe that particular source. Network throughput and conversion time await console measurement.

`make test-internet-preview test-internet-extra` exercises file selection, page boundaries, preview duration, frame sampling, mixed audio/video input, cancellation, truncation and the resolution limit. The decoder is compiled from pinned Xiph **libogg 1.3.6** and **libtheora 1.2.0** portable C sources. The release archives and SHA-256 checks are in `tools/fetch_media_xiph.py`. Binary packages include both upstream copyright notices.
