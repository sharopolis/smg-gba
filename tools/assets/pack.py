import sys

from assembly import Assets
from figures import share_pose_descriptors, share_texture_overlaps, share_tico_bank, share_whole_arrays
from poses import reanchor_poses
from stages import grid_arrays, metadata_arrays, share_stage_arrays, sky_arrays
from textures import pool_textures

SKY_STAGES = ["stage_observatory", "stage_honeyhive", "stage_honeyhive_tower"]
HONEYHIVE_STAGES = ["stage_honeyhive", "stage_honeyhive_tower"]
POSE_DONORS = [("figure_ghostmario", "figure_mario"), ("figure_flowerblue", "figure_flower")]
TEXTURE_OVERLAPS = [("figure_plant", "figure_planttop"),
                    ("figure_broadbeanmovestepa", "figure_broadbeanmovestepb"),
                    ("figure_powerstar", "figure_powerstarempty")]
MARIO = ["figure_mario", "figure_mariofire", "figure_marioice"]
STAR_BITS = [f"figure_starbit{i}" for i in range(6)]
COINS = ["figure_coin", "figure_purplecoin"]
WHOLE_ARRAYS = [("axes", MARIO), ("bind", MARIO), ("axes", STAR_BITS), ("bind", STAR_BITS),
                ("axes", COINS), ("bind", COINS), ("axes", ["figure_bluechip", "figure_yellowchip"]),
                ("axes", ["figure_launchstar", "figure_slingstar"])]
CONTAINED_TEXTURES = [("figure_plantstalk", "figure_plant")]


def pack(folder, pose_helper):
    assets = Assets(folder)
    share_tico_bank(assets)
    share_stage_arrays(assets, SKY_STAGES, "stage_sky_shared_0", sky_arrays)
    share_stage_arrays(assets, HONEYHIVE_STAGES, "stage_metadata_shared_0_", metadata_arrays)
    share_stage_arrays(assets, HONEYHIVE_STAGES, "stage_grid_shared_0_", grid_arrays)
    share_pose_descriptors(assets, POSE_DONORS)
    share_texture_overlaps(assets, TEXTURE_OVERLAPS)
    share_whole_arrays(assets, WHOLE_ARRAYS, CONTAINED_TEXTURES)
    pool_textures(assets)
    reanchor_poses(assets, pose_helper)
    assets.save()


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: pack.py <generated assets folder> <pose_anchor_costs program>")
    pack(sys.argv[1], sys.argv[2])
