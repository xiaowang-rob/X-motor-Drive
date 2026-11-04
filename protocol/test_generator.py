"""生成器布局校验及 C 宏字节回归，不依赖 Qt。"""
import copy
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from gen_protocol import (_c_header, _descriptor, _payload_macros, _render_c_headers,
                          _render_descriptor_c, _render_devices)
from schema import load_yaml, normalize

SOURCE = Path(__file__).resolve().parent


def _write_protocol_headers(root, headers, doc, types):
    """宏依赖描述表与通用引擎，编译测试需一并落盘。"""
    for name, content in headers.items():
        (root / name).write_text(content, encoding="utf-8")
    (root / "descriptor.h").write_text(
        _c_header("PROTO_DESCRIPTOR_H", _render_descriptor_c(*_descriptor(doc, types))),
        encoding="utf-8")
    (root / "payload_codec.h").write_text(
        (SOURCE.parent / "firmware" / "protocol" / "payload_codec.h").read_text(encoding="utf-8"),
        encoding="utf-8")


class GeneratorTest(unittest.TestCase):
    def test_device_ids_and_version_encoding(self):
        doc = load_yaml(SOURCE / "devices.yaml")
        header, module = _render_devices(doc)
        namespace = {}
        exec(module, namespace)
        expected = {"XDR_S_APP": 0x5760, "XDR_S_BL": 0x5761,
                    "XDR_P_APP": 0x5770, "XDR_P_BL": 0x5771}
        for name, pid in expected.items():
            self.assertEqual(namespace[name + "_USB_VID"], 0x0483)
            self.assertEqual(namespace[name + "_USB_PID"], pid)
            self.assertIn(f"#define {name}_USB_PID 0x{pid:04X}U", header)
        encode = namespace["VERSION_ENCODE"]
        cases = [(1, 2, 3), (0, 0, 0), (255, 255, 255), (256, 257, 258), (-1, -2, -3)]
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as directory:
            source = Path(directory) / "version.c"
            program = Path(directory) / "version"
            assertions = "\n".join(
                f'_Static_assert(VERSION_ENCODE({a}, {b}, {c}) == {encode(a, b, c)}U, "version");'
                for a, b, c in cases)
            source.write_text(header + assertions + "\nint main(void) { return 0; }\n")
            subprocess.run(["cc", "-std=c11", str(source), "-o", str(program)], check=True)
            subprocess.run([str(program)], check=True)
        self.assertEqual(encode(1, 2, 3), 0x010203)

    def test_invalid_usb_id(self):
        doc = load_yaml(SOURCE / "devices.yaml")
        doc["devices"]["xdr_s"]["app"]["pid"] = 0x10000
        with self.assertRaises(ValueError):
            _render_devices(doc)

    def setUp(self):
        self.source = load_yaml(SOURCE / "cmd.yaml")
        self.types = load_yaml(SOURCE / "fieldvaluetype.yaml")
        self.obs = load_yaml(SOURCE / "obsdata.yaml")
        self.doc = normalize(self.source, self.types["value_type"], self.obs)

    def test_explicit_layout_and_enum_reference(self):
        original = copy.deepcopy(self.source)
        normalize(self.source, self.types["value_type"], self.obs)
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
                normalize(broken, self.types["value_type"], self.obs)
        iap = next(c for c in self.doc["commands"] if c["id_name"] == "CMD_IAP_WRITE_FLASH")
        self.assertEqual(iap["response"][1]["enum"]["FB_EXECUTE"], 0x91)

    def test_macros_follow_yaml_and_exclude_dynamic_payloads(self):
        changed = copy.deepcopy(self.source)
        heartbeat = next(c for c in changed["commands"] if c["id_name"] == "CMD_HEARTBEAT")
        heartbeat["response"][2]["scale"] = 9
        heartbeat["response"].append({"name": "extra", "type": "FLD_U8", "offset": 6, "size": 1})
        text = "\n".join(_payload_macros(normalize(changed, self.types["value_type"], self.obs), self.types["value_type"]))
        self.assertIn("PROTO_LAYOUT_CMD_HEARTBEAT_RESPONSE", text)
        self.assertIn("motor_temp, extra)", text)
        self.assertNotIn("* 512", text)
        self.assertNotIn("ENPACK_CONNECT_RESPONSE", text)
        self.assertNotIn("DEPACK_HEARTBEAT_REQUEST", text)
        self.assertNotIn("DEPACK_PARAM_WRITE_REQUEST", text)
        self.assertNotIn("DEPACK_IAP_WRITE_FLASH_REQUEST", text)
        self.assertNotIn("ENPACK_DATA_STREAM_RESPONSE", text)
        self.assertEqual(text.count("#define ENPACK_FEEDBACK("), 1)
        self.assertNotIn("_SIZE", text)
        self.assertNotIn("_OFFSET", text)

    def test_control_config_and_parameter_special_id(self):
        from gen_protocol import _payload_constants, _render_init
        params = load_yaml(SOURCE / "parameter.yaml")
        obs = load_yaml(SOURCE / "obsdata.yaml")
        constants = dict(_payload_constants(self.doc, self.types, params, obs))
        prefix = "CMD_CTRL_CONFIG_"
        self.assertEqual(constants[prefix + "REQUEST_MIN_SIZE"], 2)
        self.assertEqual(constants[prefix + "REQUEST_MAX_SIZE"], 8)
        config = next(c for c in self.doc["commands"] if c["id_name"] == "CMD_CTRL_CONFIG")
        states = next(f for f in obs["fields"] if f["field"] == "motor_state")["options"]["items"]
        state_ids = {item["id_name"]: index for index, item in enumerate(states)}
        for name, fields in config["request_variants"].items():
            mode, value = next(iter(fields[0]["enum"].items()))
            self.assertEqual(value, state_ids[mode])
            self.assertEqual(constants[prefix + name.upper() + "_REQUEST_SIZE"],
                             8 if name == "mit" else 2)
        self.assertEqual(constants[prefix + "MIT_REQUEST_SCALE_OFFSET"], 1)
        self.assertEqual(constants[prefix + "MIT_REQUEST_SCALE_DEFAULT"], 16)
        self.assertEqual(constants[prefix + "MIT_REQUEST_KP_OFFSET"], 2)
        self.assertEqual(constants[prefix + "MIT_REQUEST_KD_OFFSET"], 5)
        for field in config["request_variants"]["mit"][2:]:
            self.assertEqual(field["scale_ref"], "scale")
        namespace = {}
        exec(_render_init(self.types, params, obs, self.doc), namespace)
        self.assertEqual(namespace["Pidx"].PARAM_ALL, 255)
        self.assertNotIn(255, namespace["PARAMETERS"])
    def test_internal_scale_decode_macros(self):
        headers = _render_c_headers(self.types, load_yaml(SOURCE / "parameter.yaml"),
                                    load_yaml(SOURCE / "obsdata.yaml"), self.doc)
        self.assertIn("#define DEPACK_CTRL_CONFIG_REQUEST(_out_length, _packet, mode)",
                      headers["cmd_list.h"])
        self.assertIn("#define DEPACK_CTRL_CONFIG_MIT_REQUEST(_packet, kp, kd)",
                      headers["cmd_list.h"])
        for variant in ("OPENLOOP", "CURRENT", "PID_SPEED", "PID_POSITION", "MIT"):
            macro = headers["cmd_list.h"].split("#define DEPACK_CTRL_CONFIG_" + variant + "_REQUEST(")[1].split("while (0)")[0]
            self.assertNotIn("_out_length", macro)
            self.assertNotIn("_proto_mode", macro)
            self.assertNotIn("(mode)", macro)
        self.assertNotIn("(scale) =", headers["cmd_list.h"])
        self.assertIn("#define DEPACK_MIT_CTRL_REQUEST(_out_length, _packet, position, velocity, torque)",
                      headers["cmd_list.h"])
        self.assertNotIn("(velocity_scale) =", headers["cmd_list.h"])
        self.assertNotIn("(torque_scale) =", headers["cmd_list.h"])
        program = '''#include "cmd_list.h"
#include <assert.h>
int main(void) {
    unsigned char packet[] = {8, 16, 0, 128, 1, 0, 64, 0};
    int length = 0, mode = 0, control = 0;
    float kp = 0, kd = 0;
    DEPACK_CTRL_CONFIG_REQUEST(length, packet, mode);
    assert(length == 8 && mode == 8);
    DEPACK_CTRL_CONFIG_MIT_REQUEST(packet, kp, kd);
    assert(length == 8 && mode == 8 && kp == 1.5f && kd == 0.25f);
    packet[1] = 8;
    DEPACK_CTRL_CONFIG_MIT_REQUEST(packet, kp, kd);
    assert(kp == 384.0f && kd == 64.0f);
    packet[1] = 0;
    DEPACK_CTRL_CONFIG_MIT_REQUEST(packet, kp, kd);
    assert(kp == 98304.0f && kd == 16384.0f);
    packet[1] = 32;
    DEPACK_CTRL_CONFIG_MIT_REQUEST(packet, kp, kd);
    assert(kp == 0x1.8p-16f && kd == 0x1p-18f);
    unsigned char high[] = {8, 16, 255, 255, 255, 0, 0, 128};
    DEPACK_CTRL_CONFIG_MIT_REQUEST(high, kp, kd);
    assert(kp > 255.99f && kd == 128.0f);
    unsigned char normal[] = {4, 1};
    DEPACK_CTRL_CONFIG_REQUEST(length, normal, mode);
    assert(length == 2 && mode == 4);
    DEPACK_CTRL_CONFIG_OPENLOOP_REQUEST(normal, control);
    assert(length == 2 && mode == 4 && control == 1);
    DEPACK_CTRL_CONFIG_CURRENT_REQUEST(normal, control);
    assert(length == 2 && control == 1);
    DEPACK_CTRL_CONFIG_PID_SPEED_REQUEST(normal, control);
    assert(length == 2 && control == 1);
    DEPACK_CTRL_CONFIG_PID_POSITION_REQUEST(normal, control);
    assert(length == 2 && control == 1);
    const unsigned char mit[] = {0, 0, 0xFA, 0x8A, 0xA0, 0, 0xC0, 0xFC};
    float position = 0, velocity = 0, torque = 0;
    DEPACK_MIT_CTRL_REQUEST(length, mit, position, velocity, torque);
    assert(length == 8 && position == -1.5f && velocity == 2.5f && torque == -3.25f);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as directory:
            root = Path(directory)
            _write_protocol_headers(root, headers, self.doc, self.types)
            (root / "config.c").write_text(program)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            str(root / "config.c"), "-o", str(root / "config")], check=True)
            subprocess.run([str(root / "config")], check=True)

    def test_control_requests_exclude_configuration(self):
        from gen_protocol import _payload_constants
        commands = {c["id_name"]: c for c in self.doc["commands"]}
        self.assertEqual([f["name"] for f in commands["CMD_OPENLOOP_CTRL"]["request"]],
                         ["duty", "value"])
        self.assertEqual([f["name"] for f in commands["CMD_CUR_TAU_CTRL"]["request"]],
                         ["value"])
        constants = dict(_payload_constants(self.doc, self.types,
                         load_yaml(SOURCE / "parameter.yaml"), load_yaml(SOURCE / "obsdata.yaml")))
        self.assertEqual(constants["CMD_OPENLOOP_CTRL_REQUEST_SIZE"], 6)
        self.assertEqual(constants["CMD_OPENLOOP_CTRL_REQUEST_DUTY_OFFSET"], 0)
        self.assertEqual(constants["CMD_OPENLOOP_CTRL_REQUEST_VALUE_OFFSET"], 2)
        self.assertEqual(constants["CMD_CUR_TAU_CTRL_REQUEST_SIZE"], 4)
        self.assertEqual(constants["CMD_CUR_TAU_CTRL_REQUEST_VALUE_OFFSET"], 0)

    def test_scale_exponent_validation(self):
        for scale in (0, -8, 8, 18):
            changed = copy.deepcopy(self.source)
            changed["commands"][1]["response"][3]["scale"] = scale
            normalize(changed, self.types["value_type"], self.obs)
        for scale in (256, 0.5, True):
            changed = copy.deepcopy(self.source)
            changed["commands"][1]["response"][3]["scale"] = scale
            with self.assertRaises(ValueError):
                normalize(changed, self.types["value_type"], self.obs)

    def test_c_field_value_type_sizes(self):
        for changed in (False, True):
            types = copy.deepcopy(self.types)
            if changed:
                types["value_type"]["FLD_I24"]["size"] = 2
            header = _render_c_headers(
                types, load_yaml(SOURCE / "parameter.yaml"),
                load_yaml(SOURCE / "obsdata.yaml"), self.doc)["fieldvaluetype.h"]
            assertions = "\n".join(
                f"    assert(field_value_type_size({name}) == {spec['size']}U);"
                for name, spec in types["value_type"].items())
            program = ('#include "fieldvaluetype.h"\n#include <assert.h>\n'
                       'int main(void) {\n' + assertions + '\n'
                       '    assert(field_value_type_size((eFieldValueType)-1) == 0U);\n'
                       '    assert(field_value_type_size((eFieldValueType)255) == 0U);\n'
                       '    return 0;\n}\n')
            with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as scratch:
                root = Path(scratch)
                (root / "fieldvaluetype.h").write_text(header, encoding="utf-8")
                source = root / "sizes.c"
                source.write_text(program, encoding="utf-8")
                for standard in ("c99", "c11"):
                    binary = root / standard
                    subprocess.run(["cc", f"-std={standard}", "-Wall", "-Wextra", "-Werror",
                                    str(source), "-o", str(binary)], check=True,
                                   capture_output=True, text=True)
                    subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_c_macros_golden_bytes(self):
        headers = _render_c_headers(self.types, load_yaml(SOURCE / "parameter.yaml"),
                                    load_yaml(SOURCE / "obsdata.yaml"), self.doc)
        program = r'''
#include "cmd_list.h"
#include <assert.h>
int main(void) {
    uint8_t packet[16], length = 0, mode = 0, id = 0;
    float value = 0, duty = 0, position = 0, velocity = 0, torque = 0, kp = 0, kd = 0;
    uint32_t size = 0, crc32 = 0;
    const uint8_t heartbeat[] = {1, 2, 0x80, 0x30, 40, 41};
    ENPACK_HEARTBEAT_RESPONSE(length, packet, 1, 2, 48.5f, 40, 41);
    assert(length == 6 && memcmp(packet, heartbeat, 6) == 0);
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
    const uint8_t openloop[] = {0x80, 0, 0, 0, 0x80, 0x3F};
    DEPACK_OPENLOOP_CTRL_REQUEST(length, openloop, duty, value);
    assert(length == 6 && duty == 0.5f && value == 1.0f);
    const uint8_t current[] = {0, 0, 0, 0x40};
    DEPACK_CUR_TAU_CTRL_REQUEST(length, current, value);
    assert(length == 4 && value == 2.0f);
    DEPACK_PID_VEL_CTRL_REQUEST(length, current, velocity);
    assert(length == 4 && velocity == 2.0f);
    DEPACK_PID_POS_CTRL_REQUEST(length, current, position);
    assert(length == 4 && position == 2.0f);
    const uint8_t mit[] = {0, 0, 0xFA, 0x8A, 0xA0, 0, 0xC0, 0xFC};
    DEPACK_MIT_CTRL_REQUEST(length, mit, position, velocity, torque);
    assert(length == 8 && position == -1.5f);
    assert(velocity == 2.5f && torque == -3.25f);
    const uint8_t config[] = {8, 16, 0, 0, 1, 0, 0, 2};
    DEPACK_CTRL_CONFIG_REQUEST(length, config, mode);
    assert(length == 8 && mode == 8);
    DEPACK_CTRL_CONFIG_MIT_REQUEST(config, kp, kd);
    assert(length == 8 && mode == 8 && kp == 1.0f && kd == 2.0f);
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
            _write_protocol_headers(root, headers, self.doc, self.types)
            source = root / "macros.c"
            source.write_text(program, encoding="utf-8")
            for standard in ("c99", "c11"):
                binary = root / standard
                subprocess.run(["cc", f"-std={standard}", "-Wall", "-Wextra", "-Werror",
                                str(source), "-o", str(binary)], check=True, capture_output=True, text=True)
                subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
