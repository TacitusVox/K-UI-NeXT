# SPDX-License-Identifier: GPL-3.0-only
"""Media companion checks with independent provider fixtures and loopback HTTP."""
import contextlib
import http.client
import http.server
import os
import io
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.parse
from pathlib import Path
from unittest.mock import patch

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import media_server as m

GAME = m.Game("MK-51035", "U", "Crazy Taxi")
Q = "/v1/dreamcast/cover?product=MK-51035&region=U&title=Crazy%20Taxi"
DAT = '''clrmamepro (
    name "Sega - Dreamcast"
)
game (
    name "Crazy Taxi (USA)"
    region "USA"
    serial "MK-51035"
    rom ( name "Crazy Taxi (USA) (Track 3).bin" size 800000000 )
)
game (
    name "Crazy Taxi (Japan)"
    region "Japan"
    serial "HDR-0026"
)
game (
    name "Crazy Taxi (USA) (Demo)"
    region "USA"
    serial "DEMO"
)
'''
ENTRY = '''<jeu id="27"><systeme id="23">Dreamcast</systeme>
<noms><nom region="us">Crazy Taxi</nom></noms><medias>
<media type="box-2D" region="us">https://www.screenscraper.fr/box</media>
<media type="box-2D" region="jp">https://www.screenscraper.fr/jp</media>
<media type="video" region="wor">https://media.screenscraper.fr/video</media>
<media type="manuel" region="us">https://media.screenscraper.fr/manual</media>
</medias></jeu>'''
XML = '<Data><jeux>' + ENTRY + '</jeux><ssuser><requeststoday>10</requeststoday>' \
      '<maxrequestsperday>20</maxrequestsperday><maxrequestspermin>10</maxrequestspermin></ssuser></Data>'


class TestCompanion(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.gamelist = self.root / "gamelist.xml"
        self.pic = self.root / "art.png"
        Image.new("RGB", (1000, 550), "orange").save(self.pic)

    def test_request(self):
        self.assertEqual(m.parse_request(Q), ("cover", GAME))
        for bad in [Q + "&title=x", "/v1/dreamcast/rom?title=Crazy",
                    Q.replace("Crazy%20Taxi", "%00"), "/v1/dreamcast/cover",
                    Q.replace("MK-51035", "x"*30), Q.replace("title=", "file=")]:
            with self.assertRaises(m.MediaError):
                m.parse_request(bad)

    def test_libretro_exact_release_and_url(self):
        releases = m.parse_dat(DAT)
        self.assertEqual(m.libretro_release(releases, GAME), "Crazy Taxi (USA)")
        self.assertEqual(m.libretro_release(releases, m.Game("MK51035", "U", "alt name")), "Crazy Taxi (USA)")
        self.assertEqual(m.libretro_release(releases, m.Game("", "J", "crazy taxi")), "Crazy Taxi (Japan)")
        self.assertIsNone(m.libretro_release(releases, m.Game("", "JU", "Crazy Taxi")))
        self.assertIsNone(m.libretro_release(releases, m.Game("", "E", "Crazy Taxi")))
        self.assertIsNone(m.libretro_release(releases, m.Game("", "U", "Crazy Taxi 2")))
        calls = []
        def upstream(url, domains, limit, fp):
            calls.append(url)
            fp.write(DAT.encode() if url == m.DAT_URL else self.pic.read_bytes())
        libretro = m.Libretro()
        with patch.object(m, "fetch", upstream):
            self.assertTrue(libretro.download(GAME, self.root / "cover"))
            self.assertTrue(libretro.download(GAME, self.root / "cover"))
        self.assertEqual(calls, [m.DAT_URL, m.COVER_BASE + "Crazy%20Taxi%20%28USA%29.png",
                                 m.COVER_BASE + "Crazy%20Taxi%20%28USA%29.png"])

    def test_libretro_shared_retail_box(self):
        libretro = m.Libretro()
        libretro.releases = [m.Release("Skies of Arcadia (USA) (Disc 1)", "USA", "51052"),
                             m.Release("Skies of Arcadia (USA) (Disc 2)", "USA", "51052")]
        game = m.Game("MK-51052", "U", "SKIES OF ARCADIA DISC1")
        self.assertEqual(m.libretro_release(libretro.releases, game), "Skies of Arcadia (USA)")
        libretro.releases.append(m.Release("Skies of Arcadia (USA) (Demo)", "USA", "51052"))
        self.assertIsNone(m.libretro_release(libretro.releases, game))
        libretro.releases = [m.Release("Crazy Taxi (USA) (En,Ja)", "USA", "51035")]
        game = m.Game("MK-51035", "U", "CRAZY TAXI")
        calls = []
        def upstream(url, domains, limit, file):
            calls.append(url)
            if len(calls) == 1:
                raise urllib.error.HTTPError(url, 404, "missing", {}, None)
            file.write(b"alternate box")
        with patch.object(m, "fetch", upstream):
            self.assertTrue(libretro.download(game, self.root / "result"))
        self.assertEqual(calls[-1], m.COVER_BASE+"Crazy%20Taxi%20%28USA%29.png")

    def test_no_untrusted_or_cleartext_urls(self):
        for url in ("http://api.screenscraper.fr/data", "https://screenscraper.fr.example.com/x",
                    "https://user:secret@api.screenscraper.fr/x", "https://127.0.0.1/",
                    "https://api.screenscraper.fr:8888/"):
            with self.assertRaises(m.MediaError):
                m.checked_url(url, ["screenscraper.fr"])
        self.assertEqual(m.checked_url("https://media.screenscraper.fr/a", ["screenscraper.fr"]),
                         "https://media.screenscraper.fr/a")

    def test_screenscraper_identity_regions_and_cache(self):
        calls = []
        def upstream(url, domains, limit, fp):
            calls.append(url)
            if url.startswith(m.SS_URL + "?"):
                params = urllib.parse.parse_qs(urllib.parse.urlsplit(url).query)
                self.assertEqual(params["systemeid"], ["23"])
                self.assertEqual(params["devid"], ["test"])
                self.assertEqual(params["recherche"], ["Crazy Taxi"])
                fp.write(XML.encode())
            else:
                fp.write(b"media")
        provider = m.ScreenScraper({"devid": "test", "devpassword": "fake"})
        with patch.object(m, "fetch", upstream):
            for kind in ("cover", "manual", "preview"):
                self.assertTrue(provider.download(GAME, kind, self.root / "asset"))
        self.assertEqual(len(calls), 4)
        self.assertEqual(provider.daily_remaining, 7)
        self.assertEqual(provider.minute_limit, 10)
        self.assertEqual(calls[1:], ["https://www.screenscraper.fr/box",
                                     "https://media.screenscraper.fr/manual",
                                     "https://media.screenscraper.fr/video"])
        for invalid in (XML.replace('id="23"', 'id="22"'),
                        XML.replace(ENTRY, ENTRY + ENTRY.replace('id="27"', 'id="34"')),
                        XML.replace("Crazy Taxi", "Crazy Taxi 2")):
            self.assertIsNone(m.exact_ss_game(m.screenscraper_xml(invalid.encode()), GAME.title))
        item = m.exact_ss_game(m.screenscraper_xml(XML.encode()), GAME.title)
        self.assertIsNone(m.select_media(item, m.Game("", "E", GAME.title), "manual"))
        for old_url, expected in (("#screenscraperserveur#api2/movie 2", "https://api.screenscraper.fr/api2/movie%202"),
                                  ("http://media.screenscraper.fr/x", "https://media.screenscraper.fr/x")):
            item = m.exact_ss_game(m.screenscraper_xml(XML.replace("https://media.screenscraper.fr/video", old_url).encode()), GAME.title)
            self.assertEqual(m.select_media(item, GAME, "preview"), expected)

    def test_quota_stops_requests(self):
        for code in (429, 430, 431, 403):
            provider = m.ScreenScraper({})
            error = urllib.error.HTTPError("https://example.invalid", code, "", {}, io.BytesIO())
            with patch.object(m, "fetch", side_effect=error) as upstream:
                with self.assertRaises(m.MediaError) as raised:
                    provider.lookup("Foo")
                self.assertEqual(raised.exception.status, 503 if code == 403 else 429)
                if code != 403:
                    with self.assertRaises(m.MediaError):
                        provider.lookup("Bar")
                    self.assertEqual(upstream.call_count, 1)

    def test_local_matches_and_paths(self):
        entries = '<game><name>Crazy Taxi</name><image>art.png</image></game>'
        def resolve(xml):
            self.gamelist.write_text("<gameList>" + xml + "</gameList>")
            return m.LocalLibrary(self.gamelist).find(GAME, "cover")
        self.assertEqual(resolve(entries), self.pic)
        self.assertEqual(resolve(entries.replace("art.png", "/storage/roms/dreamcast/art.png")), self.pic)
        self.assertIsNone(resolve(entries.replace("art.png", "/storage/roms/dreamcast/../../etc/passwd")))
        self.assertEqual(m.normalized("House of the Dead 2, The"), m.normalized("THE HOUSE OF THE DEAD 2"))
        self.assertIsNone(resolve(entries + entries))
        self.assertIsNone(resolve(entries.replace("<image>", "<region>jp</region><image>")))
        self.assertEqual(resolve(entries.replace("<name>Crazy Taxi</name>", "<path>Crazy Taxi (USA).gdi</path>")), self.pic)
        self.assertIsNone(resolve(entries.replace("<name>Crazy Taxi</name>", "<path>Crazy Taxi (Japan).gdi</path>")))
        self.assertIsNone(resolve(entries.replace("<name>Crazy Taxi</name>", "<path>Crazy Taxi (USA) (Demo).gdi</path>")))
        self.assertEqual(resolve(entries.replace("<image>art.png</image>", "<thumbnail>art.png</thumbnail><image>/missing</image>")), self.pic)
        self.assertIsNone(resolve(entries.replace("<image>", "<serial>other</serial><image>")))
        self.assertIsNone(resolve(entries.replace("art.png", "/etc/passwd")))
        (self.root / "escape").symlink_to("/etc/passwd")
        self.assertIsNone(resolve(entries.replace("art.png", "escape")))
        self.assertEqual(resolve(entries + entries.replace("<image>", "<serial>MK51035</serial><image>")), self.pic)

    def test_bounded_png(self):
        with Image.frombytes("RGB", (800, 800), os.urandom(800*800*3)) as noise:
            noise.save(self.pic)
        target = self.root / "result"
        m.convert_cover(self.pic, target)
        self.assertLessEqual(target.stat().st_size, 256*1024)
        with Image.open(target) as image:
            self.assertEqual(image.format, "PNG")
            self.assertLessEqual(max(image.size), 512)

    def test_loopback(self):
        self.gamelist.write_text('<gameList><game><name>Crazy Taxi</name><image>./art.png</image></game></gameList>')
        with http.server.HTTPServer(("127.0.0.1", 0), m.MediaHandler) as server:
            server.catalogue = m.Catalogue(m.LocalLibrary(self.gamelist))
            thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": .02})
            thread.start()
            try:
                for path, status in ((Q, 200), (Q.replace("cover?", "manual?"), 404),
                                     (Q.replace("Crazy%20Taxi", "Crazy%20Taxi%202"), 404)):
                    client = http.client.HTTPConnection(*server.server_address, timeout=3)
                    with contextlib.closing(client):
                        client.request("GET", path)
                        response = client.getresponse()
                        body = response.read()
                        self.assertEqual(response.status, status)
                        self.assertEqual(len(body), int(response.getheader("Content-Length")))
                        if status == 200:
                            self.assertEqual(response.getheader("Content-Type"), "image/png")
                            with Image.open(io.BytesIO(body)) as image:
                                self.assertEqual(image.size, (512, 282))
            finally:
                server.shutdown()
                thread.join()


if __name__ == "__main__":
    unittest.main()
