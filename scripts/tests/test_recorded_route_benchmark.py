#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "recorded_benchmark", Path(__file__).resolve().parents[1] / "benchmark-recorded-routes.py")
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


class RecordedBenchmarkTests(unittest.TestCase):
    def parse(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sessions.log"
            path.write_text(text)
            return benchmark.stop_bindings(path)

    def test_nearest_rank_small_and_unsorted_sample(self):
        result = benchmark.distribution([30, 10, 20])
        self.assertEqual(result["p50_us"], 20)
        self.assertEqual(result["p95_us"], 30)
        self.assertEqual(result["p99_us"], 30)
        self.assertIsNone(benchmark.distribution([]))

    def test_binding_uses_explicit_fields_not_filename_index_or_date(self):
        rows = self.parse(
            "/board/run.log:live_route_match_start route=/data/old-route.vhrs\n"
            "/board/other.log:live_route_match_start route=/data/new-route.vhrs\n"
            "/board/run.log:live_route_match_done endpoint_stop_frame_written=true "
            "endpoint_stop_frame_path=/data/image-route-999.pgm "
            "endpoint_stop_route_index=17 endpoint_stop_confidence=0.8\n")
        self.assertEqual(rows["image-route-999.pgm"]["route"], "old-route.vhrs")
        self.assertEqual(rows["image-route-999.pgm"]["historical_index"], 17)

    def test_missing_start_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Missing start"):
            self.parse("run.log:live_route_match_done endpoint_stop_frame_written=true\n")

    def test_conflicting_bindings_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "Ambiguous"):
            self.parse("".join(
                f"{log}:live_route_match_start route={route}\n"
                f"{log}:live_route_match_done endpoint_stop_frame_written=true "
                "endpoint_stop_frame_path=image.pgm endpoint_stop_route_index=1 "
                "endpoint_stop_confidence=0.8\n"
                for log, route in (("one.log", "one.vhrs"), ("two.log", "two.vhrs"))))

    def test_non_written_stop_is_ignored(self):
        self.assertEqual(self.parse(
            "run.log:live_route_match_done endpoint_stop_frame_written=false\n"), {})


if __name__ == "__main__":
    unittest.main()
