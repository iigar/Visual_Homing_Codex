#!/usr/bin/env python3
"""Closed inventory characterization; real traversal and mutation boundaries."""
from contextlib import contextmanager
import importlib.util
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import socket
import stat
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


spec = importlib.util.spec_from_file_location(
    "inventory_checker", Path(__file__).resolve().parents[1] / "check-second-pass-bundle.py")
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class InventoryTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="inventory tests ")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.root = self.base / "archive"
        self.root.mkdir()

    def file(self, relative):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"inventory does not interpret content")
        return path

    def link(self, path, target):
        try:
            path.symlink_to(target, target_is_directory=target.is_dir())
        except OSError as error:
            self.skipTest(f"symlinks unavailable: {error}")

    def test_empty_and_nested_names_with_explicit_membership(self):
        self.assertEqual(b.inventory(self.root), (set(), set()))
        files = {".hidden", "MiXeD", "space name", "Дані/кадр", "a/b/file..name"}
        for name in files:
            self.file(name)
        (self.root / "empty").mkdir()
        self.assertEqual(b.inventory(self.root), (files, {"Дані", "a", "a/b", "empty"}))

    @unittest.skipUnless(os.name == "posix", "POSIX literal filename semantics")
    def test_inventory_keeps_names_that_portable_dataset_paths_reject(self):
        # Inventory enumerates all entries; the callers enforce closed membership.
        names = {"a\\b", "a:b", "a,b", " leading", "trailing ", "line\nbreak", "x\x7fy"}
        for name in names:
            self.file("odd/" + name)
        self.assertEqual(b.inventory(self.root), ({"odd/" + n for n in names}, {"odd"}))

    def test_relative_string_dot_and_root_alias_inputs(self):
        self.file("dir/file")
        expected = ({"dir/file"}, {"dir"})
        roots = (self.root, str(self.root) + os.sep, self.root / "dir" / "..")
        for root in roots:
            with self.subTest(root=root):
                self.assertEqual(b.inventory(root), expected)
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            self.assertEqual(b.inventory(Path(".")), expected)
            self.assertEqual(b.inventory("."), expected)
        finally:
            os.chdir(previous)
        alias = self.base / "alias"
        self.link(alias, self.root)
        self.assertEqual(b.inventory(alias), expected)

    def test_windows_and_posix_names_in_scandir_contract(self):
        # Simulated DirEntry paths, not native Windows/junction acceptance.
        for flavour, roots in ((PurePosixPath, ["/", "/archive", "//host/archive"]),
                               (PureWindowsPath, ["C:/", "C:/Archive", "//host/share/Archive"])):
            for root_name in roots:
                root = flavour(root_name)
                tree = {root: [("MiXeD", stat.S_IFDIR), (".hidden", stat.S_IFREG)],
                        root / "MiXeD": [("Дані space", stat.S_IFREG)]}

                @contextmanager
                def scan(path):
                    yield [SimpleNamespace(name=name, path=str(flavour(path) / name),
                           stat=Mock(return_value=SimpleNamespace(st_mode=mode)))
                           for name, mode in tree[flavour(path)]]

                with self.subTest(root=root), patch.object(b, "Path", flavour), patch.object(b.os, "scandir", scan):
                    self.assertEqual(b.inventory(root), ({"MiXeD/Дані space", ".hidden"}, {"MiXeD"}))

    def test_deep_and_wide_tree(self):
        prefix, directories = "", set()
        for index in range(80):
            prefix += ("/" if prefix else "") + f"d{index}"
            directories.add(prefix)
        files = {prefix + "/leaf"} | {f"wide/f{i}" for i in range(50)}
        for name in files:
            self.file(name)
        self.assertEqual(b.inventory(self.root), (files, directories | {"wide"}))

    def test_internal_external_broken_and_cyclic_links_are_rejected(self):
        target = self.file("dir/file")
        outside = self.base / "outside"
        outside.write_bytes(b"outside")
        link = self.root / "dir" / "link"
        for destination in (target, target.parent, outside, self.base, self.base / "missing", link):
            with self.subTest(target=destination):
                self.link(link, destination)
                try:
                    with self.assertRaisesRegex(b.v.Invalid, "dir/link: bundle symlinks"):
                        b.inventory(self.root)
                finally:
                    link.unlink()

    @unittest.skipUnless(hasattr(os, "mkfifo") and hasattr(socket, "AF_UNIX"), "POSIX special files")
    def test_fifo_and_socket_rejected_without_opening(self):
        fifo = self.root / "fifo"
        os.mkfifo(fifo)
        with self.assertRaisesRegex(b.v.Invalid, "fifo: expected regular file"):
            b.inventory(self.root)
        fifo.unlink()
        with socket.socket(socket.AF_UNIX) as server:
            server.bind(str(self.root / "socket"))
            with self.assertRaisesRegex(b.v.Invalid, "socket: expected regular file"):
                b.inventory(self.root)

    def test_inventory_limit_counts_files_and_empty_directories(self):
        self.file("dir/file")
        (self.root / "empty").mkdir()
        with patch.object(b, "INVENTORY_LIMIT", 3):
            self.assertEqual(b.inventory(self.root), ({"dir/file"}, {"dir", "empty"}))
        with patch.object(b, "INVENTORY_LIMIT", 2):
            with self.assertRaisesRegex(b.v.Invalid, "inventory limit exceeded"):
                b.inventory(self.root)

    def test_every_call_scans_and_lstats_each_entry_without_reading(self):
        self.file("dir/file")
        (self.root / "empty").mkdir()
        scan, scans, stats, closed = os.scandir, [], [], []

        @contextmanager
        def observed(path):
            scans.append(Path(path))
            try:
                with scan(path) as entries:
                    def items():
                        for entry in entries:
                            def fresh_stat(*, follow_symlinks, entry=entry):
                                self.assertIs(follow_symlinks, False)
                                stats.append(Path(entry.path))
                                return entry.stat(follow_symlinks=follow_symlinks)
                            yield SimpleNamespace(name=entry.name, path=entry.path, stat=fresh_stat)
                    yield items()
            finally:
                closed.append(Path(path))

        with patch.object(b.os, "scandir", observed), patch.object(Path, "open", side_effect=AssertionError("content read")):
            for _ in range(2):
                self.assertEqual(b.inventory(self.root), ({"dir/file"}, {"dir", "empty"}))
        self.assertCountEqual(scans, [self.root, self.root / "dir", self.root / "empty"] * 2)
        self.assertCountEqual(stats, [self.root / p for p in ("dir", "dir/file", "empty")] * 2)
        self.assertCountEqual(closed, scans)

    def test_membership_and_replaced_type_are_observed_on_next_call(self):
        target = self.file("file")
        self.assertEqual(b.inventory(self.root), ({"file"}, set()))
        self.file("new/file")
        target.unlink()
        target.mkdir()
        self.assertEqual(b.inventory(self.root), ({"new/file"}, {"new", "file"}))
        target.rmdir()
        self.link(target, self.root / "new/file")
        with self.assertRaisesRegex(b.v.Invalid, "file: bundle symlinks"):
            b.inventory(self.root)

    def test_removal_between_enumeration_and_stat_propagates_and_closes(self):
        target = self.file("file")
        scan, closed = os.scandir, []

        @contextmanager
        def disappearing(path):
            try:
                with scan(path) as entries:
                    entry = next(entries)
                    target.unlink()
                    yield [entry]
            finally:
                closed.append(Path(path))

        with patch.object(b.os, "scandir", disappearing):
            with self.assertRaises(FileNotFoundError):
                b.inventory(self.root)
        self.assertEqual(closed, [self.root])

    def test_missing_non_directory_and_scan_errors_propagate(self):
        with self.assertRaises(FileNotFoundError):
            b.inventory(self.root / "missing")
        with self.assertRaises(NotADirectoryError):
            b.inventory(self.file("file"))
        with patch.object(b.os, "scandir", side_effect=PermissionError("denied")):
            with self.assertRaisesRegex(PermissionError, "denied"):
                b.inventory(self.root)


if __name__ == "__main__":
    unittest.main()
