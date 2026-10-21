#!/usr/bin/env python3
"""Dataset path characterization, including fresh filesystem and hash checks."""
import hashlib
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import socket
import stat
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import test_second_pass_dataset as fixture

v = fixture.validator


class PosixFile(PurePosixPath):
    def stat(self):
        return SimpleNamespace(st_mode=stat.S_IFREG)


class WindowsFile(PureWindowsPath):
    def stat(self):
        return SimpleNamespace(st_mode=stat.S_IFREG)


class PathTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="dataset path tests ")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.root = self.base / "data"
        self.root.mkdir()
        self.file = self.root / "file"
        self.file.write_bytes(b"original")
        self.store = v.Dataset(self.root)

    def link(self, path, target):
        try:
            path.symlink_to(target, target_is_directory=target.is_dir())
        except OSError as error:
            self.skipTest(f"symlinks unavailable: {error}")

    def test_portable_path_rejections_precede_resolution(self):
        cases = [None, 1, True, b"file", "", " ", "/file", "//server/share/file",
                 "C:/file", "C:file", "a\\file", "a,file", ".", "..", "a/./file",
                 "a/../file", "a//file", "a/", " file", "file ", "a/ file",
                 "a /file", "a/\u00a0file", "a\u2003/file"]
        cases += [f"a{chr(code)}file" for code in range(32)]
        with patch.object(v, "resolved", side_effect=AssertionError("invalid path resolved")):
            for value in cases:
                with self.subTest(value=value), self.assertRaises(v.Invalid):
                    self.store.path(value)

    def test_character_policy_at_component_edges_and_interior(self):
        # Separate lexical policy from filesystem-specific filename support.
        # DEL, non-ASCII controls and interior Unicode spaces were never banned.
        characters = [chr(i) for i in range(256)] + list("\u2003\u2028\u2029\u3000\U0001f642")
        forbidden = set(map(chr, range(32))) | set("\\:,")
        with patch.object(v, "resolved", return_value=self.file):
            for char in characters:
                for placement in ("interior", "leading", "trailing"):
                    component = {"interior": f"x{char}y", "leading": f"{char}xy",
                                 "trailing": f"xy{char}"}[placement]
                    invalid = char in forbidden or (char.isspace() and placement != "interior")
                    invalid |= char == "/" and placement != "interior"
                    with self.subTest(char=repr(char), placement=placement):
                        if invalid:
                            with self.assertRaises(v.Invalid):
                                self.store.path("dir/" + component + "/file")
                        else:
                            self.assertEqual(self.store.path("dir/" + component + "/file"), self.file)

    def test_real_nested_names_and_case_follow_native_filesystem(self):
        directory = self.root / "nested space" / "\u0414\u0430\u043d\u0456"
        directory.mkdir(parents=True)
        for name in (".hidden", "file..name", "under_score-01", "inside space", "MiXeD"):
            path = directory / name
            path.write_bytes(b"bytes")
            self.assertEqual(self.store.path(path.relative_to(self.root).as_posix()), path.resolve())
        candidate = directory / "mixed"
        if candidate.exists():
            self.assertTrue(self.store.path(candidate.relative_to(self.root).as_posix()).samefile(directory / "MiXeD"))
        else:
            with self.assertRaises(FileNotFoundError):
                self.store.path(candidate.relative_to(self.root).as_posix())

    def test_containment_matches_ancestor_contract_for_both_path_flavours(self):
        # Pure Windows paths exercise drive/UNC/case semantics on Linux too;
        # this is not a Windows filesystem or junction acceptance test.
        groups = ((PosixFile, ["/", "/data", "/data/sub", "/Data", "//host/data"],
                   ["/", "/data", "/data/file", "/data/sub/file", "/database/file",
                    "/Data/file", "/other/file", "//host/data/file"]),
                  (WindowsFile, ["C:/", "C:/Data", "C:/data/sub", "D:/Data", "//host/share/", "//host/share/Data"],
                   ["C:/", "C:/Data", "c:/DATA/file", "C:/Data/sub/file", "C:/Database/file",
                    "D:/Data/file", "//host/share/Data/file", "//HOST/SHARE/data/sub/file",
                    "//host/share2/Data/file", "//other/share/Data/file"]))
        for flavour, roots, candidates in groups:
            for root in roots:
                store = object.__new__(v.Dataset)
                store.root = flavour(root)
                for candidate in candidates + [str(flavour(root).joinpath(*(["deep"] * 32), "file"))]:
                    path = flavour(candidate)
                    with self.subTest(root=root, candidate=candidate), patch.object(v, "resolved", return_value=path):
                        if store.root in path.parents:
                            self.assertEqual(store.path("fixture"), path)
                        else:
                            with self.assertRaisesRegex(v.Invalid, "path escapes dataset root"):
                                store.path("fixture")

    def test_internal_links_and_root_aliases_are_allowed(self):
        self.link(self.root / "link", self.file)
        self.link(self.root / "directory", self.root)
        self.link(self.base / "alias", self.root)
        for store in (self.store, v.Dataset(self.base / "alias"), v.Dataset(self.root / "../data")):
            self.assertEqual(store.path("link"), self.file.resolve())
            self.assertEqual(store.path("directory/file"), self.file.resolve())
            with self.assertRaisesRegex(v.Invalid, "path escapes dataset root"):
                store.path("directory")  # Root itself is not a strict descendant.

    def test_escape_prefix_sibling_missing_and_cycles(self):
        sibling = self.base / "database"
        sibling.mkdir()
        outside = sibling / "file"
        outside.write_bytes(b"outside")
        self.link(self.root / "escape", outside)
        with self.assertRaisesRegex(v.Invalid, "path escapes dataset root"):
            self.store.path("escape")
        self.link(self.root / "cycle", self.root / "cycle")
        with self.assertRaises((v.Invalid, OSError)):
            self.store.path("cycle")
        self.link(self.root / "missing", self.root / "absent")
        with self.assertRaises(FileNotFoundError):
            self.store.path("missing")

    def test_replacement_symlink_is_resolved_again(self):
        self.assertEqual(self.store.small("file", 100), "original")
        outside = self.base / "outside"
        outside.write_bytes(b"original")
        self.file.unlink()
        self.link(self.file, outside)
        with self.assertRaisesRegex(v.Invalid, "path escapes dataset root"):
            self.store.small("file", 100)

    def test_replaced_root_ancestor_is_resolved_again(self):
        self.store.path("file")
        moved = self.base / "moved"
        self.root.rename(moved)
        self.link(self.root, moved)
        with self.assertRaisesRegex(v.Invalid, "path escapes dataset root"):
            self.store.path("file")

    def test_internal_link_retarget_hash_and_expected_hash_are_checked(self):
        target = self.root / "other"
        target.write_bytes(b"modified")
        link = self.root / "link"
        self.link(link, self.file)
        original_hash = hashlib.sha256(b"original").hexdigest()
        self.store.artifact(dict(path="link", sha256=original_hash), "input")
        link.unlink()
        self.link(link, target)
        with self.assertRaisesRegex(v.Invalid, "file changed between reads"):
            self.store.small("link", 100)
        with self.assertRaisesRegex(v.Invalid, "SHA-256 mismatch"):
            v.Dataset(self.root).artifact(dict(path="link", sha256=original_hash), "input")

    def test_same_size_and_mtime_do_not_replace_content_hashing(self):
        before = self.file.stat()
        self.store.small("file", 100)
        self.file.write_bytes(b"modified")
        os.utime(self.file, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.assertEqual((self.file.stat().st_size, self.file.stat().st_mtime_ns),
                         (before.st_size, before.st_mtime_ns))
        with self.assertRaisesRegex(v.Invalid, "file changed between reads"):
            self.store.small("file", 100)

    def test_read_hashes_consumed_bytes_and_checks_in_read_mutation(self):
        def consume(reader):
            return reader.read(3) + reader.read(100)
        self.assertEqual(self.store.read("file", consume), b"original")
        self.assertEqual(self.store.inputs["file"], hashlib.sha256(b"original").hexdigest())
        def mutate(reader):
            result = reader.read(100)
            with self.file.open("ab") as stream:
                stream.write(b"changed during parse")
            return result
        with self.assertRaisesRegex(v.Invalid, "file changed while reading"):
            self.store.read("file", mutate)

    def test_fresh_stat_rejects_directory_fifo_and_socket_without_opening(self):
        self.store.path("file")
        self.file.unlink()
        self.file.mkdir()
        with self.assertRaisesRegex(v.Invalid, "expected regular file"):
            self.store.small("file", 100)
        if hasattr(os, "mkfifo"):
            os.mkfifo(self.root / "fifo")
            with self.assertRaisesRegex(v.Invalid, "expected regular file"):
                self.store.small("fifo", 100)
        if hasattr(socket, "AF_UNIX") and os.name == "posix":
            with socket.socket(socket.AF_UNIX) as sock:
                sock.bind(str(self.root / "socket"))
                with self.assertRaisesRegex(v.Invalid, "expected regular file"):
                    self.store.small("socket", 100)


if __name__ == "__main__":
    unittest.main()
