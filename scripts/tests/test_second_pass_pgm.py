#!/usr/bin/env python3
"""Characterize the validator's bounded PGM and consumed-byte hash contract."""
import hashlib
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import test_second_pass_dataset as fixture

v = fixture.validator
SPACE = b" \t\r\n\v\f"


def sha(data):
    return hashlib.sha256(data).hexdigest()


class PgmTests(unittest.TestCase):
    def parse(self, data):
        with io.BufferedReader(io.BytesIO(data)) as stream:
            reader = v.HashedReader(stream)
            result = v.pgm(reader)
            self.assertEqual(stream.tell(), len(data))
            self.assertEqual(reader.digest.hexdigest(), sha(data))
            return result

    def reject(self, data, message, consumed=None, exception=None):
        with io.BufferedReader(io.BytesIO(data)) as stream:
            reader = v.HashedReader(stream)
            with self.assertRaisesRegex(exception or v.Invalid, message):
                v.pgm(reader)
            if consumed is not None:
                self.assertEqual(stream.tell(), consumed)
                self.assertEqual(reader.digest.hexdigest(), sha(data[:consumed]))

    def test_header_whitespace_and_comments_at_token_boundaries(self):
        for separator in SPACE:
            sep = bytes([separator])
            for comment in (b"", b"# comment\n", b"# one\r\n# two\n"):
                with self.subTest(separator=separator, comment=comment):
                    data = (sep + comment).join((b"", b"P5", b"2", b"2", b"255")) + b"\n\x00 #\xff"
                    self.assertEqual(self.parse(data), (2, 2, sha(b"\x00 #\xff")))

    def test_comments_end_only_at_lf_and_hash_is_not_an_inline_delimiter(self):
        self.assertEqual(self.parse(b"# ignored\rP2 0 0 0\nP5 1 1 255\n#"), (1, 1, sha(b"#")))
        self.reject(b"# no LF\rP5 1 1 255\n", "truncated input")
        for data, message in ((b"P5#x\n1 1 255\nX", "expected binary P5"),
                              (b"P5 1#x\n1 255\nX", "unsigned decimal"),
                              (b"P5 1 1 255#x\nX", "max value")):
            with self.subTest(data=data):
                self.reject(data, message)

    def test_single_separator_preserves_every_first_pixel(self):
        for separator in SPACE:
            for pixel in range(256):
                # CR + LF is always interpreted as the two-byte separator.
                if separator == 13 and pixel == 10:
                    continue
                data = b"P5 1 1 255" + bytes([separator, pixel])
                with self.subTest(separator=separator, pixel=pixel):
                    self.assertEqual(self.parse(data), (1, 1, sha(bytes([pixel]))))

    def test_crlf_and_no_extra_whitespace_or_comment_after_maximum(self):
        for pixel in range(256):
            self.assertEqual(self.parse(b"P5 1 1 255\r\n" + bytes([pixel])),
                             (1, 1, sha(bytes([pixel]))))
        self.reject(b"P5 1 1 255\r\n", "truncated input", 12)
        self.assertEqual(self.parse(b"P5 1 1 255\n\n"), (1, 1, sha(b"\n")))
        self.reject(b"P5 1 1 255\n# comment\nX", "trailing bytes", 13)

    def test_full_digest_includes_header_but_pixel_digest_does_not(self):
        pixel = b"\n #\x00"
        headers = (b"P5 2 2 255\n", b"# leading\nP5\n2\t2\n255\r\n", b"P5 02 0002 255 ")
        self.assertEqual(len({sha(header + pixel) for header in headers}), 3)
        for header in headers:
            self.assertEqual(self.parse(header + pixel), (2, 2, sha(pixel)))

    def test_header_limit_counts_comments_separator_and_optional_lf(self):
        for ending in (b"\n", b"\r\n"):
            base = b"P5 1 1 255" + ending
            for length in (4095, 4096, 4097):
                header = b" " * (length - len(base)) + base
                with self.subTest(length=length, ending=ending):
                    if length <= 4096:
                        self.assertEqual(self.parse(header + b"#"), (1, 1, sha(b"#")))
                    else:
                        self.reject(header + b"#", "header exceeds 4096 bytes", 4096)
        base = b"P5 1 1 255\n"
        header = b"#" + b"x" * (4096 - len(base) - 2) + b"\n" + base
        self.assertEqual(self.parse(header + b"X"), (1, 1, sha(b"X")))
        self.reject(b"#" + b"x" * 4096, "header exceeds 4096 bytes", 4096)

    def test_magic_numbers_ascii_and_geometry(self):
        for magic in (b"P2", b"p5", b"P50", b"\xef\xbb\xbfP5"):
            self.reject(magic + b" 1 1 255\nX", "expected binary P5")
        for dimension in (b"0", b"65536", b"-1", b"+1", b"1.0", b"1x", b"0" * 20 + b"1"):
            for axis in (0, 1):
                dims = [b"1", b"1"]
                dims[axis] = dimension
                with self.subTest(dimension=dimension, axis=axis):
                    self.reject(b"P5 " + b" ".join(dims) + b" 255\nX", "PGM\\.(width|height)")
        self.reject(b"P5 \xff 1 255\nX", "ascii", exception=UnicodeDecodeError)
        for maximum in (b"0", b"254", b"256", b"0255", b"+255", b"65535", b"\xff"):
            self.reject(b"P5 1 1 " + maximum + b"\nX", "max value")
        pixels = b"X" * 65535
        self.assertEqual(self.parse(b"P5 65535 1 255\n" + pixels), (65535, 1, sha(pixels)))
        self.assertEqual(self.parse(b"P5 00000000000000000001 1 255\nX"), (1, 1, sha(b"X")))

    def test_every_truncation_hashes_exact_consumed_prefix(self):
        for good in (b"P5 2 2 255\n\x00 #\xff", b"# c\r\nP5\n2 2\n255\r\n\x00 #\xff"):
            for end in range(len(good)):
                with self.subTest(end=end, good=good):
                    self.reject(good[:end], "truncated input", end)

    def test_trailing_rejection_consumes_and_hashes_only_one_extra_byte(self):
        good = b"P5 1 1 255\nX"
        for suffix in (b"\x00", b"\n", b"# ignored?\n", good):
            self.reject(good + suffix, "trailing bytes", len(good) + 1)

    def test_real_64_mib_payload_bound_and_chunked_reads(self):
        header = b"P5 8192 8192 255\n"
        chunk = bytes(1024 * 1024)
        pixel_hash, file_hash = hashlib.sha256(), hashlib.sha256(header)
        for _ in range(64):
            pixel_hash.update(chunk)
            file_hash.update(chunk)
        with tempfile.TemporaryFile() as stream:
            stream.write(header)
            stream.truncate(len(header) + 64 * 1024 * 1024)
            stream.seek(0)
            reader = v.HashedReader(stream)
            with patch.object(reader, "read", wraps=reader.read) as reads:
                self.assertEqual(v.pgm(reader), (8192, 8192, pixel_hash.hexdigest()))
            counts = [call.args[0] for call in reads.call_args_list]
            self.assertEqual(counts.count(1024 * 1024), 64)
            self.assertLessEqual(max(counts), 1024 * 1024)
            self.assertEqual(reader.digest.hexdigest(), file_hash.hexdigest())
        for dims in (b"8192 8193", b"65535 65535"):
            header = b"P5 " + dims + b" 255\n"
            self.reject(header + b"X", "payload exceeds 64 MiB", len(header))

    def dataset(self, data):
        temporary = tempfile.TemporaryDirectory(prefix="pgm-contract-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        path = root / "frame.pgm"
        path.write_bytes(data)
        return v.Dataset(root), path

    def test_dataset_expected_digest_includes_comments_and_crlf(self):
        data = b"# comment\nP5 1 1 255\r\n#"
        store, _ = self.dataset(data)
        self.assertEqual(store.read("frame.pgm", v.pgm, sha(data)), (1, 1, sha(b"#")))
        self.assertEqual(store.inputs, {"frame.pgm": sha(data)})
        with self.assertRaisesRegex(v.Invalid, "SHA-256 mismatch"):
            store.read("frame.pgm", v.pgm, sha(b"#"))

    def test_dataset_same_size_mtime_mutation_is_rehashed(self):
        data = b"P5 1 1 255\nX"
        store, path = self.dataset(data)
        store.read("frame.pgm", v.pgm)
        before = path.stat()
        path.write_bytes(data[:-1] + b"Y")
        os.utime(path, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.assertEqual(path.stat().st_mtime_ns, before.st_mtime_ns)
        with self.assertRaisesRegex(v.Invalid, "file changed between reads"):
            store.read("frame.pgm", v.pgm)
        self.assertEqual(store.inputs, {"frame.pgm": sha(data)})

    def test_dataset_mutation_during_read_fails_before_recording_digest(self):
        data = b"P5 1 1 255\nX"
        store, path = self.dataset(data)
        before = path.stat()

        def parse(reader):
            result = v.pgm(reader)
            os.utime(path, ns=(before.st_atime_ns, before.st_mtime_ns + 1000000000))
            return result

        with self.assertRaisesRegex(v.Invalid, "file changed while reading"):
            store.read("frame.pgm", parse)
        self.assertEqual(store.inputs, {})

    def test_parser_and_io_errors_do_not_record_partial_inputs(self):
        store, path = self.dataset(b"P5 1 1 255\n")
        with self.assertRaisesRegex(v.Invalid, "truncated input"):
            store.read("frame.pgm", v.pgm)
        self.assertEqual(store.inputs, {})
        path.write_bytes(b"P5 1 1 255\nX")
        with patch.object(v.HashedReader, "payload_hash", side_effect=OSError("read failed")):
            with self.assertRaisesRegex(OSError, "read failed"):
                store.read("frame.pgm", v.pgm)
        self.assertEqual(store.inputs, {})


if __name__ == "__main__":
    unittest.main()
