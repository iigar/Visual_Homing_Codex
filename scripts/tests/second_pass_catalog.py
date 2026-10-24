"""Vary synthetic catalog entries while keeping the twelve replay workloads fixed."""
import copy
import shutil

import test_second_pass_campaign as f


def dataset_count(value):
    if type(value) is not int or value not in (3, 6, 12):
        raise RuntimeError("dataset counts must be 3, 6 or 12")
    return value


def expand_catalog(inputs, request, count):
    """Same-split copies share capture/content; they are not independent datasets.

    Each original split has four config/policy runs. Assign those unchanged runs
    round-robin to 1/2/4 copies so every catalog entry has an expected run.
    """
    dataset_count(count)
    if count == 3:
        return request
    originals = request["datasets"]
    if [row["dataset_id"] for row in originals] != list(f.SPLITS) or len(request["runs"]) != 12:
        raise RuntimeError("expected the original three-split twelve-run fixture")
    expanded, groups = [], {}
    for row in originals:
        did = row["dataset_id"]
        groups[did] = []
        for index in range(count // 3):
            target = copy.deepcopy(row)
            if index:
                name = f"{did}-copy-{index:02d}"
                target.update(dataset_id=name, path=name)
                shutil.copytree(inputs / row["path"], inputs / name)
                meta = f.read(inputs / name, "dataset.json")
                meta["dataset_id"] = name
                f.e.write_json(inputs / name / "dataset.json", meta)
            expanded.append(target)
            groups[did].append(target["dataset_id"])
    validation = f.v.validate_datasets([inputs / row["path"] for row in expanded], True)
    if validation["status"] != "structurally_valid":
        raise RuntimeError(str(validation))
    for row, checked in zip(expanded, validation["datasets"]):
        row["input_sha256"] = checked["input_sha256"]
    request["datasets"] = expanded
    assigned = dict.fromkeys(groups, 0)
    for row in request["runs"]:
        original = row["dataset_id"]
        row["dataset_id"] = groups[original][assigned[original] % len(groups[original])]
        assigned[original] += 1
    f.e.write_json(inputs / "request.json", request)
    return request
