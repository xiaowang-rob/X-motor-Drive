"""生成器布局校验及 C 宏字节回归，不依赖 Qt。"""
import copy
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from gen_protocol import _payload_macros, _render_c_headers
from schema import load_json, normalize

SOURCE = Path(__file__).resolve().parent


class GeneratorTest(unittest.TestCase):
    def setUp(self):
        self.source = load_json(SOURCE / "cmd.json")
        self.types = load_json(SOURCE / "fieldvaluetype.json")
        self.doc = normalize(self.source, self.types["value_type"])

    def test_explicit_layout_and_enum_reference(self):
        original = copy.deepcopy(self.source)
        normalize(self.source, self.types["value_type"])
        self.assertEqual(self.source, original)
        for command in self.source["commands"]:
            for direction in ("request", "response"):
                if isinstance(command[direction], list):
                    for field in command[direction]:
                        self.assertIn("size", field)
                        self.assertIn("offset", field)
        for key, wrong in (("size", 2), ("offset", 1)):
            broken = copy.deepcopy(self.source)
            broken["commands"][0]["request"][0][key] = wrong
            with self.assertRaises(ValueError):
                normalize(broken, self.types["value_type"])
        iap = next(c for c in self.doc["commands"] if c["id_name"] == "CMD_IAP_WRITE_FLASH")
        self.assertEqual(iap["response"][1]["enum"]["FB_EXECUTE"], 0x91)

    def test_macros_follow_json_and_exclude_dynamic_payloads(self):
        changed = copy.deepcopy(self.source)
        heartbeat = next(c for c in changed["commands"] if c["id_name"] == "CMD_HEARTBEAT")
        heartbeat["response"][3]["scale"] = 512
        heartbeat["response"].append({"name": "extra", "type": "FLD_U8", "offset": 7, "size": 1})
        text = "\n".join(_payload_macros(normalize(changed, self.types["value_type"]), self.types["value_type"]))
        self.assertIn("* 512", text)
        self.assertIn("(_out_length) = 8;", text)
        self.assertIn("_proto_extra", text)
        self.assertNotIn("ENPACK_CONNECT_RESPONSE", text)
        self.assertNotIn("DEPACK_HEARTBEAT_REQUEST", text)
        self.assertNotIn("DEPACK_PARAM_WRITE_REQUEST", text)
        self.assertNotIn("DEPACK_IAP_WRITE_FLASH_REQUEST", text)
        self.assertNotIn("ENPACK_DATA_STREAM_RESPONSE", text)
        self.assertEqual(text.count("#define ENPACK_FEEDBACK("), 1)
        self.assertNotIn("_SIZE", text)
        self.assertNotIn("_OFFSET", text)

    def test_c_macros_golden_bytes(self):
        headers = _render_c_headers(self.types, load_json(SOURCE / "parameter.json"),
                                    load_json(SOURCE / "obsdata.json"), self.doc)
        program = r'''
#include "cmd_list.h"
#include <assert.h>
int main(void) {
    uint8_t packet[16], length = 0, mode = 0, id = 0;
    float value = 0, duty = 0, position = 0, velocity = 0, torque = 0, kp = 0, kd = 0;
    int velocity_scale = 0, torque_scale = 0;
    uint32_t size = 0, crc32 = 0;
    const uint8_t heartbeat[] = {1, 2, 3, 0x80, 0x30, 40, 41};
    ENPACK_HEARTBEAT_RESPONSE(length, packet, 1, 2, 3, 48.5f, 40, 41);
    assert(length == 7 && memcmp(packet, heartbeat, 7) == 0);
    ENPACK_FEEDBACK(length, packet, true);
    assert(length == 1 && packet[0] == 0x91);
    ENPACK_FEEDBACK(length, packet, false);
    assert(length == 1 && packet[0] == 0x9E);
    DEPACK_CONNECT_REQUEST(length, packet, mode);
    assert(length == 1 && mode == 0x9E);
    DEPACK_PARAM_READ_REQUEST(length, packet, id);
    assert(length == 1 && id == 0x9E);
    DEPACK_DATA_ASK_REQUEST(length, packet, id);
    assert(length == 1 && id == 0x9E);
    const uint8_t openloop[] = {0, 0x80, 0, 0, 0, 0x80, 0x3F};
    DEPACK_OPENLOOP_CTRL_REQUEST(length, openloop, mode, duty, value);
    assert(length == 7 && mode == 0 && duty == 0.5f && value == 1.0f);
    const uint8_t current[] = {1, 0, 0, 0, 0x40};
    DEPACK_CUR_TAU_CTRL_REQUEST(length, current, mode, value);
    assert(length == 5 && mode == 1 && value == 2.0f);
    DEPACK_PID_VEL_CTRL_REQUEST(length, current + 1, velocity);
    assert(length == 4 && velocity == 2.0f);
    DEPACK_PID_POS_CTRL_REQUEST(length, current + 1, position);
    assert(length == 4 && position == 2.0f);
    const uint8_t mit[] = {0, 0, 0xFA, 0x8A, 0xA0, 0, 0xC0, 0xFC};
    DEPACK_MIT_CTRL_REQUEST(length, mit, position, velocity_scale, torque_scale, velocity, torque);
    assert(length == 8 && position == -1.5f && velocity_scale == 6 && torque_scale == 8);
    assert(velocity == 2.5f && torque == -3.25f);
    const uint8_t config[] = {0, 0, 0x80, 0x3F, 0, 0, 0, 0x40};
    DEPACK_MIT_CONFIG_REQUEST(length, config, kp, kd);
    assert(length == 8 && kp == 1.0f && kd == 2.0f);
    const uint8_t erase[] = {0, 4, 0, 0};
    DEPACK_IAP_ERASE_FLASH_REQUEST(length, erase, size);
    assert(length == 4 && size == 1024);
    ENPACK_IAP_WRITE_FLASH_RESPONSE(length, packet, 0x12345678u, FB_EXECUTE);
    const uint8_t ack[] = {0x78, 0x56, 0x34, 0x12, 0x91};
    assert(length == 5 && memcmp(packet, ack, 5) == 0);
    const uint8_t verify[] = {0x78, 0x56, 0x34, 0x12, 12, 0, 0, 0};
    DEPACK_IAP_VERIFY_FLASH_REQUEST(length, verify, crc32, size);
    assert(length == 8 && crc32 == 0x12345678u && size == 12);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as scratch:
            root = Path(scratch)
            (root / "cmd_list.h").write_text(headers["cmd_list.h"], encoding="utf-8")
            source = root / "macros.c"
            source.write_text(program, encoding="utf-8")
            for standard in ("c99", "c11"):
                binary = root / standard
                subprocess.run(["cc", f"-std={standard}", "-Wall", "-Wextra", "-Werror",
                                str(source), "-o", str(binary)], check=True, capture_output=True, text=True)
                subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
