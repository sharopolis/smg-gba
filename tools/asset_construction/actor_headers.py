import json
import os
import struct

import actor_headers_candle
import actor_headers_colmesh
import actor_headers_freezeflame
import actor_headers_kamella
import actor_headers_kcl_mesh
import actor_headers_mario
import actor_headers_placement
import actor_headers_poses
from actor_headers_lib import Ctx
from common import IntReader, parse_args, read_pak, write_out

MODULES = (actor_headers_candle, actor_headers_colmesh, actor_headers_freezeflame, actor_headers_kamella,
           actor_headers_kcl_mesh, actor_headers_mario, actor_headers_placement, actor_headers_poses)


def render(tpl, values):
    parts = []
    k = 0
    start = 0
    for i, c in enumerate(tpl):
        if c in (1, 2):
            parts.append(tpl[start:i])
            v = values[k]
            k += 1
            if c == 1:
                parts.append(str(v).encode())
            else:
                parts.append(struct.unpack(">f", struct.pack(">I", v & 0xFFFFFFFF))[0].hex().encode())
            start = i + 1
    parts.append(tpl[start:])
    return b"".join(parts)


def main():
    args = parse_args()
    pak = read_pak(os.path.join(args.data, "actor_headers", "actor_headers.pak"))
    predictors = {}
    for module in MODULES:
        predictors.update(module.PREDICTORS)
    ctx = Ctx(args.orig, args.out)
    rels = sorted(json.loads(pak["index.json"]))
    for rel in rels:
        tpl = pak[rel + ".tpl"]
        n = tpl.count(1) + tpl.count(2)
        pred = predictors[rel](ctx, pak.get(rel + ".spec", b""))
        if len(pred) != n:
            raise ValueError(rel + ": wrong value count")
        res = IntReader(pak[rel + ".res"]).ints(n) if n else []
        write_out(args.out, rel, render(tpl, [p + r for p, r in zip(pred, res)]))
    print("actor_headers: %d files" % len(rels))


if __name__ == "__main__":
    main()
