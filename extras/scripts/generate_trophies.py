#!/usr/bin/env python3
"""
Generate extras/trophy/CRDH00001_00/TROPHY.TRP for Carnivores: Dinosaur Hunter.

TRP format/hash/PNG8 handling copied from the Prince of Persia Classic port
(v01.55, confirmed working on real HW with NoTrpDrm).

Trophy N (1..25) == game achievement N-1 (Social_ShowAchievementNotification
in libdinHunter.so); source/java.c maps unlockAchievement(id) -> id + 1.
Icons are built from the APK's Help/*.png art, so run it from the repo root
with the APK extracted in carnivoresdinosaurhunter_extract/.
"""

import io
import os
import struct
import hashlib
from PIL import Image

COMM_ID = "CRDH00001_00"
HELP = "carnivoresdinosaurhunter_extract/assets/Help"
ICON0 = "extras/livearea/icon0.png"

# (ttype, name, detail, icon) -- order == game achievement id 0..24.
AWARDS = [
    ("B", "Scientist",          "Read the hunting guide to the end.",                       "button_binocular"),
    ("B", "First Blood",        "Hunt down your first dinosaur.",                           "velociraptor"),
    ("B", "Ornithologist",      "Shoot down a flying reptile.",                             "pteranodon"),
    ("B", "Swimmer",            "Swim 100 meters in a single hunt.",                        "parasaurolophus"),
    ("B", "Archaeologist",      "Find the hidden spot on the map.",                         "button_compass"),
    ("S", "Explorer",           "Hunt in every area.",                                      "button_binocular"),
    ("B", "Animal Activist",    "Hunt down a dinosaur with the tranquilizer.",              "gallimimus"),
    ("S", "Marathoner",         "Walk a total of 42,195 meters.",                           "moschops"),
    ("B", "Hunter",             "Hunt down a carnivore.",                                   "allosaurus"),
    ("S", "Sniper",             "Finish a hunt with 100% accuracy.",                        "button_fire"),
    ("B", "Sharpshooter",       "Hunt down two dinosaurs within 20 seconds.",               "button_weapon"),
    ("B", "Amateur",            "Earn 500 points.",                                         "stegosaurus"),
    ("S", "Last Second Save",   "Hunt down a carnivore at close range.",                    "exclamation"),
    ("B", "To The Bitter End",  "Hunt down a dinosaur with your last round.",               "ceratosaurus"),
    ("B", "Alive",              "Kill more than 10 dinosaurs in Survival mode.",            "chasmosaurus"),
    ("S", "Poacher",            "Bag a trophy of every species.",                           "anklyosaurus"),
    ("S", "Advanced",           "Earn 1,000 points.",                                       "pachycephalosaurus"),
    ("G", "Survived",           "Kill more than 20 dinosaurs in Survival mode.",            "spinosaurus"),
    ("G", "Expert",             "Earn 2,000 points.",                                       "brachiosaurus"),
    ("G", "Total Annihilation", "Hunt down every dinosaur on the map.",                     "dimetrodon"),
    ("S", "Collector",          "Hunt down every kind of carnivore.",                       "button_call"),
    ("S", "T-Rex Hunter",       "Hunt down a T-Rex.",                                       "trex"),
    ("G", "Madman",             "Hunt down a T-Rex without radar, cover scent or camouflage.", "trex"),
    ("G", "William Tell",       "Hunt down a T-Rex with the crossbow.",                     "trex"),
    ("G", "Guru",               "Bag a top trophy of every species.",                       "dimorphodon"),
]

TROPHIES = [{"id": "000", "ttype": "P", "pid": "-1", "name": "Carnivores: Dinosaur Hunter",
             "detail": "Earn every other trophy."}]
for i, (tt, name, detail, _) in enumerate(AWARDS, 1):
    TROPHIES.append({"id": f"{i:03d}", "ttype": tt, "pid": "000", "name": name, "detail": detail})

FRAME = {"P": (200, 210, 230), "G": (212, 175, 55), "S": (192, 192, 200), "B": (176, 110, 60)}


def make_sfm(with_text):
    sig = "x" * 320  # same placeholder length as working NoTrpDrm packs
    lines = [f"<!--Sce-Np-Trophy-Signature: {sig}-->",
             '<trophyconf version="1.1" platform="psp2" policy="large">',
             f" <npcommid>{COMM_ID}</npcommid>",
             " <trophyset-version>01.00</trophyset-version>",
             ' <parental-level license-area="default">0</parental-level>']
    if with_text:
        lines += [" <title-name>Carnivores: Dinosaur Hunter</title-name>",
                  " <title-detail>Carnivores: Dinosaur Hunter PlayStation Vita Port</title-detail>"]
    for t in TROPHIES:
        attrs = f'id="{t["id"]}" hidden="no" ttype="{t["ttype"]}" pid="{t["pid"]}"'
        if with_text:
            lines += [f" <trophy {attrs}>", f'  <name>{t["name"]}</name>',
                      f'  <detail>{t["detail"]}</detail>', " </trophy>"]
        else:
            lines.append(f" <trophy {attrs}/>")
    lines.append("</trophyconf>\n")
    return "\n".join(lines).encode("utf-8")


def tile(src, ttype, size=240):
    """Game art scaled up onto a dark tile with a tier-colored border."""
    art = Image.open(src).convert("RGBA")
    canvas = Image.new("RGBA", (size, size), FRAME[ttype] + (255,))
    inner = Image.new("RGBA", (size - 16, size - 16), (24, 28, 22, 255))
    canvas.paste(inner, (8, 8))
    box = size - 40
    s = min(box / art.width, box / art.height)
    art = art.resize((round(art.width * s), round(art.height * s)), Image.Resampling.LANCZOS)
    canvas.alpha_composite(art, ((size - art.width) // 2, (size - art.height) // 2))
    return canvas




def png8(path):
    """8-bit palette PNG with transparency, like working NoTrpDrm packs.
    The full-RGBA ~100 KB icons are far above what retail trophy packs ship
    and the set install failed with NP-6185-0 ("data corrupted")."""
    img = Image.open(path).convert("RGBA").quantize(256, method=Image.Quantize.FASTOCTREE)
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()

def build_trp(file_dict, output_path):
    def align64(v):
        return (v + 63) & ~63

    num_files = len(file_dict)
    header_size = 64
    entries_size = num_files * 64
    first_file_offset = align64(header_size + entries_size)

    current_offset = first_file_offset
    file_offsets = {}
    for name, data in file_dict.items():
        file_offsets[name] = (current_offset, len(data))
        current_offset = align64(current_offset + len(data))

    total_size = current_offset

    # The 20-byte header field is a plain SHA-1 CONTENT hash, not a Sony
    # signature: it is SHA-1 over the whole file with this field zeroed.
    # Verified against Rinnegatamante's working Eldritch TROPHY.TRP, whose
    # header hash recomputes exactly. NoTrpDrm bypasses SIGNATURE checks,
    # but the firmware still verifies this hash during
    # sceNpTrophySetupDialogInit -- a zeroed field fails install (0x800204C4
    # -> SETUP_REQUIRED, no system notification). Assemble with zeros first,
    # then hash and patch.
    sha_dummy = b"\x00" * 20
    header = struct.pack(">4sIQIII20s16s",
                         b"\xdc\xa2\x4d\x00",
                         2,
                         total_size,
                         num_files,
                         64,
                         0,
                         sha_dummy,
                         b"\x00" * 16)

    entries = bytearray()
    for name, data in file_dict.items():
        offset, size = file_offsets[name]
        entry = struct.pack(">32sQQI12s",
                            name.encode("latin1"),
                            offset,
                            size,
                            0,
                            b"\x00" * 12)
        entries.extend(entry)

    out = bytearray()
    out.extend(header)
    out.extend(entries)

    pad_len = first_file_offset - len(out)
    out.extend(b"\x00" * pad_len)

    for name, data in file_dict.items():
        target_offset, size = file_offsets[name]
        current_pos = len(out)
        if target_offset > current_pos:
            out.extend(b"\x00" * (target_offset - current_pos))
        out.extend(data)

    final_pad = total_size - len(out)
    if final_pad > 0:
        out.extend(b"\x00" * final_pad)

    # Patch the real content hash into the header (bytes 28..48).
    real_sha = hashlib.sha1(bytes(out)).digest()
    out[28:48] = real_sha

    with open(output_path, "wb") as f:
        f.write(out)
    print(f"TRP written to {output_path} ({len(out)} bytes, header sha1={real_sha.hex()})")


def main():
    workdir = "extras/trophy/icons"
    outdir = f"extras/trophy/{COMM_ID}"
    os.makedirs(workdir, exist_ok=True)
    os.makedirs(outdir, exist_ok=True)

    icon0 = Image.new("RGBA", (320, 176), (0, 0, 0, 0))
    logo = Image.open(ICON0).convert("RGBA").resize((160, 160), Image.Resampling.LANCZOS)
    icon0.paste(logo, (80, 8))
    icon0.save(os.path.join(workdir, "ICON0.PNG"))
    tile(ICON0, "P").save(os.path.join(workdir, "TROP000.PNG"))
    for i, (tt, _, _, icon) in enumerate(AWARDS, 1):
        tile(os.path.join(HELP, icon + ".png"), tt).save(os.path.join(workdir, f"TROP{i:03d}.PNG"))

    files = {"TROPCONF.SFM": make_sfm(False), "TROP.SFM": make_sfm(True)}
    for name in ["ICON0.PNG"] + [f"TROP{i:03d}.PNG" for i in range(len(TROPHIES))]:
        files[name] = png8(os.path.join(workdir, name))
    build_trp(files, os.path.join(outdir, "TROPHY.TRP"))


if __name__ == "__main__":
    main()
