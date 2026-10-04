#!/usr/bin/env python3
"""Deterministic, read-only scoring of pinned second-pass predictions.

See docs/SECOND_PASS_SCORER_UA.md. Does not run a matcher or parse legacy logs.
"""
import argparse
from collections import Counter
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import sys


VALIDATOR_PATH = Path(__file__).with_name("validate-second-pass-dataset.py")
spec = importlib.util.spec_from_file_location("second_pass_validator", VALIDATOR_PATH)
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)

GATES = "navigation_command_valid route_readiness verification_accepted published".split()
PREDICTION_FIELDS = ("sequence frame_id timestamp_ns observation missing_reason match_valid "
                     "reference_index endpoint_stop").split() + GATES


def read_json(store, path):
    return json.loads(store.small(path, 4 * 1024 * 1024), object_pairs_hook=v.unique_object,
                      parse_constant=v.invalid_constant, parse_float=v.finite_float)


def check_hashes(actual, expected, where):
    v.require(type(expected) is dict, where, "expected complete path/hash map")
    for path, digest in expected.items():
        v.string(path, where + ".path")
        v.sha256(digest, where + ".sha256")
    v.require(actual == expected, where, "input hashes differ (missing/extra/changed inputs)")


def artifact(record, where):
    v.obj(record, "path sha256", where)
    v.string(record["path"], where + ".path")
    v.sha256(record["sha256"], where + ".sha256")


def read_policy(store, record):
    artifact(record, "policy")
    policy = read_json(store, record["path"])
    v.require(store.inputs[record["path"]] == record["sha256"], "policy", "SHA-256 mismatch")
    v.obj(policy, "schema_version policy_id scorable_visibility target_endpoint endpoint_early_tolerance_ns "
          "endpoint_late_tolerance_ns reacquisition_min_correct_frames reacquisition_min_duration_ns "
          "max_interframe_gap_ns", "policy")
    v.integer(policy["schema_version"], "policy.schema_version", 1, 1)
    v.string(policy["policy_id"], "policy.policy_id")
    v.strings(policy["scorable_visibility"], "policy.scorable_visibility", nonempty=True)
    for visibility in policy["scorable_visibility"]:
        v.choice(visibility, "clear degraded", "policy.scorable_visibility")
    v.choice(policy["target_endpoint"], "start end", "policy.target_endpoint")
    for name in ("endpoint_early_tolerance_ns", "endpoint_late_tolerance_ns", "reacquisition_min_duration_ns"):
        v.integer(policy[name], "policy." + name, 0, v.I64)
    v.integer(policy["max_interframe_gap_ns"], "policy.max_interframe_gap_ns", 1, v.I64)
    v.integer(policy["reacquisition_min_correct_frames"], "policy.reacquisition_min_correct_frames", 1, v.MAX_ROWS)
    return policy


def parse_flag(value, where, optional=False):
    if optional and value == "":
        return None
    v.choice(value, "true false", where)
    return value == "true"


def read_predictions(store, record, frames, entry_count):
    artifact(record, "predictions")
    predictions = []
    for index, row in store.rows(record["path"], PREDICTION_FIELDS):
        where = f"predictions row {index}"
        v.require(index < len(frames), where, "extra row")
        frame = frames[index]
        for key in ("sequence", "frame_id", "timestamp_ns"):
            number = v.decimal(row[key], where + "." + key)
            v.require(number == int(frame[key]), where, "sequence/ID/timestamp mismatch; no sorting or rebasing")
        v.choice(row["observation"], "observed missing", where + ".observation")
        result = {"observation": row["observation"], "missing_reason": row["missing_reason"]}
        if row["observation"] == "missing":
            v.string(row["missing_reason"], where + ".missing_reason")
            v.require(all(row[key] == "" for key in ["match_valid", "reference_index", "endpoint_stop"] + GATES),
                      where, "missing row cannot carry predictions or gate outcomes")
            result.update({key: None for key in ["match_valid", "reference_index", "endpoint_stop"] + GATES})
        else:
            v.require(row["missing_reason"] == "", where, "observed row cannot have missing_reason")
            result["match_valid"] = parse_flag(row["match_valid"], where + ".match_valid")
            if result["match_valid"]:
                result["reference_index"] = v.decimal(row["reference_index"], where + ".reference_index", 0, entry_count - 1)
            else:
                v.require(row["reference_index"] == "", where, "invalid match must have empty reference_index")
                result["reference_index"] = None
            result["endpoint_stop"] = parse_flag(row["endpoint_stop"], where + ".endpoint_stop", optional=True)
            for key in GATES:
                result[key] = parse_flag(row[key], where + "." + key, optional=True)
        predictions.append(result)
    v.require(len(predictions) == len(frames), "predictions", "missing rows; use explicit missing observations")
    v.require(store.inputs[record["path"]] == record["sha256"], "predictions", "SHA-256 mismatch")
    return predictions


def ratio(numerator, denominator):
    return {"numerator": numerator, "denominator": denominator,
            "value": numerator / denominator if denominator else None}


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return {"count": 0, "min": None, "max": None, "p50": None, "p95": None}
    return {"count": len(ordered), "min": ordered[0], "max": ordered[-1],
            **{f"p{p}": ordered[(len(ordered) * p + 99) // 100 - 1] for p in (50, 95)}}


def frame_decisions(frames, labels, predictions, meta, policy):
    decisions, boundaries = [], []
    explicit = {(g["after_frame_id"], g["before_frame_id"]): g for g in meta["sequence"]["gaps"]}
    segment = 0
    for index, (frame, prediction) in enumerate(zip(frames, predictions)):
        fid, stamp = int(frame["frame_id"]), int(frame["timestamp_ns"])
        label = labels[fid]
        if index:
            previous = decisions[-1]
            delta = stamp - previous["timestamp_ns"]
            reasons = []
            if (previous["frame_id"], fid) in explicit:
                reasons.append("declared_gap")
            if delta > policy["max_interframe_gap_ns"]:
                reasons.append("interframe_limit")
            if reasons:
                segment += 1
                boundaries.append({"after_frame_id": previous["frame_id"], "before_frame_id": fid,
                                   "unrepresented_interval_ns": delta, "reasons": reasons})
            else:
                previous["represented_interval_ns"] = delta
        reason = None
        if label["location"] == "unknown":
            reason = "unknown_location"
        elif label["visibility"] not in policy["scorable_visibility"]:
            reason = "excluded_visibility"
        elif label["location"] == "on_route" and not label["reference_index_min"]:
            reason = "missing_correspondence"
        kind = "unscorable" if reason else label["location"]
        match_index = prediction["reference_index"]
        error = None
        if kind == "on_route" and prediction["match_valid"]:
            lower, upper = int(label["reference_index_min"]), int(label["reference_index_max"])
            error = max(lower - match_index, match_index - upper, 0)
        tp = int(error == 0) if error is not None else 0
        fp = int(bool(prediction["match_valid"]) and (kind == "off_route" or (error is not None and error > 0)))
        fn = int(kind == "on_route" and not tp)
        if kind == "unscorable":
            outcome = "unscorable"
        elif kind == "on_route":
            outcome = "tp" if tp else "fp_fn" if fp else "fn"
        else:
            outcome = "fp" if fp else "missing_negative" if prediction["observation"] == "missing" else "tn"
        decisions.append({"sequence": index, "frame_id": fid, "timestamp_ns": stamp, "segment": segment,
                          "represented_interval_ns": 0, "label": label, "prediction": prediction,
                          "scorable_kind": kind, "unscorable_reason": reason, "outcome": outcome,
                          "tp": tp, "fp": fp, "fn": fn, "index_error": error,
                          "normalized_index_error": error / (meta["reference"]["entry_count"] - 1)
                          if error is not None and meta["reference"]["entry_count"] > 1 else None})
    return decisions, boundaries


def metrics(decisions):
    counts = Counter()
    durations = Counter()
    reasons = Counter()
    missing_reasons = Counter()
    gates = {key: {"true": 0, "false": 0, "unknown": 0} for key in ["endpoint_stop"] + GATES}
    for row in decisions:
        pred = row["prediction"]
        observed = pred["observation"] == "observed"
        categories = [row["scorable_kind"], "observed" if observed else "missing", row["outcome"]]
        # A category such as off_route and an outcome such as fp are distinct;
        # unscorable occurs in both, so count each key once per source frame.
        for category in set(categories):
            counts[category] += 1
            durations[category] += row["represented_interval_ns"]
        counts["tp_total"] += row["tp"]
        counts["fp_total"] += row["fp"]
        counts["fn_total"] += row["fn"]
        if observed and pred["match_valid"] is False:
            counts["abstentions"] += 1
        if row["scorable_kind"] == "off_route" and observed:
            counts["observed_negative"] += 1
            counts["negative_fp"] += row["fp"]
        if row["unscorable_reason"]:
            reasons[row["unscorable_reason"]] += 1
        if not observed:
            missing_reasons[pred["missing_reason"]] += 1
        for key in gates:
            value = pred[key]
            gates[key]["unknown" if value is None else "true" if value else "false"] += 1
    tp, fp, fn = (counts[key] for key in ("tp_total", "fp_total", "fn_total"))
    return {"frames": len(decisions), "tp": tp, "fp": fp, "fn": fn,
            "precision": ratio(tp, tp + fp), "recall": ratio(tp, tp + fn),
            "negative_fp_fraction": ratio(counts["negative_fp"], counts["observed_negative"]),
            "negative_prediction_coverage": ratio(counts["observed_negative"], counts["off_route"]),
            "prediction_coverage": ratio(counts["observed"], len(decisions)),
            "abstention_fraction": ratio(counts["abstentions"], counts["observed"]),
            "unscorable_fraction": ratio(counts["unscorable"], len(decisions)),
            "counts": dict(sorted(counts.items())), "represented_duration_ns": dict(sorted(durations.items())),
            "unscorable_reasons": dict(sorted(reasons.items())), "missing_reasons": dict(sorted(missing_reasons.items())),
            "index_error": distribution([r["index_error"] for r in decisions if r["index_error"] is not None]),
            "normalized_index_error": distribution([r["normalized_index_error"] for r in decisions
                                                      if r["normalized_index_error"] is not None]),
            "gate_outcomes": gates}


def groups(decisions, predicate):
    """Contiguous selected regions; declared/long gaps always break continuity."""
    result = []
    start = None
    for index, row in enumerate(decisions):
        if start is not None and (not predicate(row) or row["segment"] != decisions[start]["segment"]):
            result.append((start, index - 1))
            start = None
        if predicate(row) and start is None:
            start = index
    if start is not None:
        result.append((start, len(decisions) - 1))
    return result


def endpoints(decisions, policy):
    target = policy["target_endpoint"]

    def known(row):
        label = row["label"]
        return (label["location"] != "unknown" and label["endpoint"] != "unknown" and
                label["visibility"] in policy["scorable_visibility"])

    bounds = {}
    for index, row in enumerate(decisions):
        bounds.setdefault(row["segment"], [index, index])[1] = index
    events = []
    by_segment = {}
    for first, last in groups(decisions, lambda r: known(r) and r["label"]["endpoint"] == target):
        a, b = decisions[first], decisions[last]
        lo, hi = bounds[a["segment"]]
        start = max(decisions[lo]["timestamp_ns"], a["timestamp_ns"] - policy["endpoint_early_tolerance_ns"])
        end = min(decisions[hi]["timestamp_ns"], b["timestamp_ns"] + policy["endpoint_late_tolerance_ns"])
        segment_events = by_segment.setdefault(a["segment"], [])
        v.require(not segment_events or segment_events[-1]["window_end_ns"] < start,
                  "endpoint policy", "overlapping endpoint tolerance windows; assignment would be ambiguous")
        event = {"event": len(events), "segment": a["segment"], "entry_frame_id": a["frame_id"],
                 "last_target_frame_id": b["frame_id"], "entry_timestamp_ns": a["timestamp_ns"],
                 "last_target_timestamp_ns": b["timestamp_ns"], "window_start_ns": start, "window_end_ns": end,
                 "entry_left_censored": first == lo or not known(decisions[first - 1]),
                 "window_right_censored": last == hi or b["timestamp_ns"] + policy["endpoint_late_tolerance_ns"] > decisions[hi]["timestamp_ns"],
                 "detected": False, "stop_frame_id": None, "sample_delay_ns": None, "entry_delay_ns": None,
                 "unobserved_stop_frames": 0}
        segment_events.append(event)
        events.append(event)
    cursors = Counter()
    stops = []
    for row in decisions:
        candidates = by_segment.get(row["segment"], [])
        cursor = cursors[row["segment"]]
        stamp = row["timestamp_ns"]
        while cursor < len(candidates) and candidates[cursor]["window_end_ns"] < stamp:
            cursor += 1
        cursors[row["segment"]] = cursor
        event = candidates[cursor] if cursor < len(candidates) and candidates[cursor]["window_start_ns"] <= stamp else None
        stop = row["prediction"]["endpoint_stop"]
        if event and stop is None:
            event["unobserved_stop_frames"] += 1
        if stop is not True:
            continue
        record = {"frame_id": row["frame_id"], "timestamp_ns": stamp, "event": None}
        if not known(row):
            record["classification"] = "unscorable_stop"
        elif event and not event["detected"]:
            event["detected"] = True
            event["stop_frame_id"] = row["frame_id"]
            event["sample_delay_ns"] = stamp - event["entry_timestamp_ns"]
            if not event["entry_left_censored"]:
                event["entry_delay_ns"] = event["sample_delay_ns"]
            record.update(classification="accepted", event=event["event"])
        else:
            record["classification"] = "duplicate_stop" if event else "early_stop" if cursor < len(candidates) else "false_stop"
        stops.append(record)
    return {"target": target, "events": events, "stops": stops,
            "detected": sum(e["detected"] for e in events), "not_detected": sum(not e["detected"] for e in events),
            "not_detected_right_censored": sum(not e["detected"] and e["window_right_censored"] for e in events),
            "stop_counts": dict(sorted(Counter(s["classification"] for s in stops).items())),
            "entry_delay_ns": distribution([e["entry_delay_ns"] for e in events if e["entry_delay_ns"] is not None])}


def reacquisition(decisions, policy):
    episodes = []
    for first, last in groups(decisions, lambda r: r["scorable_kind"] == "on_route"):
        a, b = decisions[first], decisions[last]
        if first == 0:
            kind = "initial_acquisition"
        elif decisions[first - 1]["segment"] != a["segment"]:
            kind = "after_gap"
        else:
            kind = "return_to_scorable_route"
        if last + 1 == len(decisions):
            termination = "clip_end"
        elif decisions[last + 1]["segment"] != b["segment"]:
            termination = "gap"
        else:
            termination = "region_exit"
        confirmed = None
        streak_start = None
        streak_count = 0
        for index in range(first, last + 1):
            row = decisions[index]
            if row["tp"]:
                if streak_start is None:
                    streak_start = row["timestamp_ns"]
                streak_count += 1
                if (streak_count >= policy["reacquisition_min_correct_frames"] and
                        row["timestamp_ns"] - streak_start >= policy["reacquisition_min_duration_ns"]):
                    confirmed = row
                    break
            else:
                streak_start = None
                streak_count = 0
        episodes.append({"kind": kind, "entry_frame_id": a["frame_id"], "last_frame_id": b["frame_id"],
                         "entry_timestamp_ns": a["timestamp_ns"], "termination": termination,
                         "status": "confirmed" if confirmed else "not_confirmed",
                         "confirmation_frame_id": confirmed["frame_id"] if confirmed else None,
                         "observed_delay_ns": confirmed["timestamp_ns"] - a["timestamp_ns"] if confirmed else None,
                         "observation_span_ns": b["timestamp_ns"] - a["timestamp_ns"],
                         "right_censored": confirmed is None and termination != "region_exit"})
    returns = [e for e in episodes if e["kind"] == "return_to_scorable_route"]
    return {"episodes": episodes, "returns": len(returns),
            "confirmed_returns": sum(e["status"] == "confirmed" for e in returns),
            "unconfirmed_returns": sum(e["status"] != "confirmed" for e in returns),
            "right_censored_returns": sum(e["right_censored"] for e in returns),
            "return_delay_ns": distribution([e["observed_delay_ns"] for e in returns if e["observed_delay_ns"] is not None])}


def score(dataset_root, run_root, peers=(), disjoint_references=False):
    validation = v.validate_datasets([dataset_root, *peers], disjoint_references)
    report = {"report_version": 1, "status": validation["status"], "dataset_validation": validation,
              "semantic_independence_verified": False, "errors": []}
    if validation["status"] != "structurally_valid":
        return report
    run_store = None
    try:
        validated = validation["datasets"][0]
        data_store = v.Dataset(dataset_root)
        meta = read_json(data_store, "dataset.json")
        frames = [row for _, row in data_store.rows("frames.csv", v.FRAME_FIELDS)]
        labels = {int(row["frame_id"]): row for _, row in data_store.rows("labels.csv", v.LABEL_FIELDS)}
        check_hashes(data_store.inputs, {p: validated["input_sha256"][p] for p in data_store.inputs}, "dataset reread")
        run_store = v.Dataset(run_root)
        run = read_json(run_store, "run.json")
        v.obj(run, "schema_version run_id dataset_id input_sha256 inputs_frozen_before_run producer policy predictions", "run")
        v.integer(run["schema_version"], "run.schema_version", 1, 1)
        v.string(run["run_id"], "run.run_id")
        v.require(run["dataset_id"] == meta["dataset_id"], "run.dataset_id", "different dataset")
        check_hashes(validated["input_sha256"], run["input_sha256"], "run.input_sha256")
        v.require(run["inputs_frozen_before_run"] is True, "run.inputs_frozen_before_run", "must explicitly declare pre-run pinning")
        producer = v.obj(run["producer"], "kind code_revision implementation configuration state_reset_policy preprocessing", "producer")
        v.choice(producer["kind"], "synthetic external_export", "producer.kind")
        for key in ("code_revision", "state_reset_policy", "preprocessing"):
            v.string(producer[key], "producer." + key)
        for key in ("implementation", "configuration"):
            artifact(producer[key], "producer." + key)
            run_store.artifact(producer[key], "producer." + key)
        policy = read_policy(run_store, run["policy"])
        predictions = read_predictions(run_store, run["predictions"], frames, meta["reference"]["entry_count"])
        decisions, boundaries = frame_decisions(frames, labels, predictions, meta, policy)
        endpoint_report = endpoints(decisions, policy)
        report.update(status="scored", run_id=run["run_id"], dataset_id=meta["dataset_id"],
                      query_session_id=meta["query_session_id"], capture_conditions=meta["capture"]["conditions"],
                      capture_counts={key: meta["sequence"][key] for key in ("acquired_count", "stored_count", "dropped_count")},
                      input_kind=producer["kind"], producer=producer, policy=policy,
                      pinning_provenance="pre-run order is declared, not independently verified",
                      metrics=metrics(decisions), endpoints=endpoint_report, reacquisition=reacquisition(decisions, policy),
                      by_direction={key: metrics([r for r in decisions if r["label"]["direction"] == key])
                                    for key in sorted({r["label"]["direction"] for r in decisions})},
                      by_visibility={key: metrics([r for r in decisions if r["label"]["visibility"] == key])
                                     for key in sorted({r["label"]["visibility"] for r in decisions})},
                      temporal_coverage={"capture_span_ns": decisions[-1]["timestamp_ns"] - decisions[0]["timestamp_ns"],
                                         "represented_span_ns": sum(r["represented_interval_ns"] for r in decisions),
                                         "unrepresented_span_ns": sum(b["unrepresented_interval_ns"] for b in boundaries),
                                         "boundaries": boundaries, "declared_gaps": meta["sequence"]["gaps"]},
                      decisions=decisions,
                      scorer_sha256={path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                     for path in (Path(__file__), VALIDATOR_PATH)})
    except (v.Invalid, OSError, ValueError, csv.Error, RecursionError) as exc:
        report.update(status="invalid", errors=[str(exc)])
    if run_store is not None:
        report["run_input_sha256"] = run_store.inputs
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_root", type=Path)
    parser.add_argument("run_root", type=Path)
    parser.add_argument("--split-peer", action="append", default=[], type=Path,
                        help="other dataset root to include in split leakage validation; repeat as needed")
    parser.add_argument("--disjoint-references", action="store_true")
    args = parser.parse_args(argv)
    report = score(args.dataset_root, args.run_root, args.split_peer, args.disjoint_references)
    print(json.dumps(report, indent=2, sort_keys=True, ensure_ascii=True))
    return {"scored": 0, "invalid": 2, "needs_review": 3}[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
