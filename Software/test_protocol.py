"""不依赖 Qt 的协议生成与编解码回归测试。"""
import math
import unittest

from app.services import payload_codec as codec
from protocol import Didx, FbId, PARAMETERS, Pidx
from protocol import payload as wire


class ProtocolTest(unittest.TestCase):
    def test_schema_generated_fixed_payloads(self):
        self.assertEqual(codec.pack_connect(True), b"\x01")
        self.assertEqual(codec.pack_openloop_ctrl(False, 0.5, 1.0).hex(), "0080000000803f")
        self.assertEqual(codec.pack_current_torque(True, 2.0).hex(), "0100000040")
        self.assertEqual(codec.pack_single(1.0).hex(), "0000803f")
        self.assertEqual(codec.pack_mit_config(1.0, 2.0).hex(), "0000803f00000040")
        self.assertEqual(codec.pack_iap_erase(1024), b"\x00\x04\x00\x00")
        self.assertEqual(codec.pack_iap_verify(0x12345678, 12).hex(), "785634120c000000")

    def test_parameters_and_special_requests(self):
        meta = PARAMETERS[Pidx.PARAM_MOTOR_RS]
        payload = codec.pack_param(meta, 1.25)
        self.assertEqual(payload[0], meta.id)
        returned, value = codec.unpack_param(payload)
        self.assertIs(returned, meta)
        self.assertAlmostEqual(value, 1.25)
        self.assertEqual(codec.pack_param_read(meta), bytes([meta.id]))
        self.assertEqual(codec.special_request("CMD_PARAM_WRITE", "apply"), b"\xff")
        self.assertEqual(codec.special_request("CMD_PARAM_RELOAD", "all"), b"\xff")
        with self.assertRaises(ValueError):
            codec.unpack_param(b"\xff\x00")

    def test_streams_and_dynamic_length_validation(self):
        channels = (Didx.DATA_POSITION, Didx.DATA_VBUS)
        subscription = codec.pack_stream_set(channels)
        self.assertEqual(subscription, bytes([2, *channels]))
        packet = codec.pack_data_stream(channels, (3.25, 48.0))
        values = codec.unpack_data_stream(packet, channels)
        self.assertAlmostEqual(values[0], 3.25)
        self.assertAlmostEqual(values[1], 48.0)
        with self.assertRaises(ValueError):
            codec.pack_stream_set(range(6))
        with self.assertRaises(ValueError):
            codec.unpack_data_stream(b"\x02\x00", channels)

    def test_heartbeat_scale_and_mit_round_trip(self):
        heartbeat = codec.pack_heartbeat([1, 0, 0, 48.5, 40, 41])
        self.assertEqual(heartbeat.hex(), "01000080302829")
        decoded = codec.unpack_heartbeat(heartbeat)
        self.assertEqual(decoded[:3], [1, 0, 0])
        self.assertAlmostEqual(decoded[3], 48.5)
        self.assertEqual(decoded[4:], [40, 41])
        payload = codec.pack_mit_ctrl(-1.5, 2.5, -3.25, velocity_exp=6, torque_exp=8)
        self.assertEqual(len(payload), 8)
        self.assertEqual(payload.hex(), "0000fa8aa000c0fc")
        self.assertEqual(int.from_bytes(payload[:3], "little", signed=True), -393216)
        self.assertEqual(payload[3], 0x8A)
        with self.assertRaises(ValueError):
            codec.pack_mit_ctrl(0, velocity_exp=9)

    def test_generated_layout_and_feedback_values(self):
        self.assertFalse(hasattr(wire, "SCHEMA"))
        self.assertFalse(any(callable(value) for name, value in vars(wire).items()
                             if not name.startswith("__")))
        self.assertEqual(wire.CMD_MIT_CTRL_REQUEST_SIZE, 8)
        self.assertEqual(wire.CMD_MIT_CTRL_REQUEST_VELOCITY_OFFSET, 4)
        self.assertEqual(wire.CMD_IAP_WRITE_FLASH_REQUEST_MAX_SIZE, 128)
        self.assertEqual(FbId.FB_EXECUTE, 0x91)
        self.assertEqual(FbId.FB_FAILURE, 0x9E)

    def test_limits_and_invalid_values(self):
        self.assertEqual(len(codec.pack_iap_write(0, b"x" * 124)), 128)
        with self.assertRaises(ValueError):
            codec.pack_iap_write(0, b"x" * 125)
        saturated = codec.pack_openloop_ctrl(False, 1e9, 0)
        self.assertEqual(saturated[1:3], b"\xff\xff")
        with self.assertRaises(ValueError):
            codec.pack_openloop_ctrl(False, math.nan, 0)
        with self.assertRaises(ValueError):
            codec.unpack_heartbeat(b"\x01")
        with self.assertRaises(ValueError):
            codec.pack_value(PARAMETERS[Pidx.PARAM_MOTOR_RS], math.inf)
        offset, status = codec.unpack_iap_write_ack(b"\x78\x56\x34\x12" + bytes([FbId.FB_EXECUTE]))
        self.assertEqual((offset, status), (0x12345678, FbId.FB_EXECUTE))


if __name__ == "__main__":
    unittest.main()
