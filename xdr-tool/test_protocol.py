"""不依赖 Qt 的协议生成与编解码回归测试。"""
import math
import unittest

from app.services import payload_codec as codec
from protocol import CmdId, Didx, FbId, PARAMETERS, Pidx, eMotorState


class ProtocolTest(unittest.TestCase):
    def test_fixed_payload_bytes(self):
        self.assertEqual(codec.pack_connect(True), b"\x01")
        self.assertEqual(codec.pack_openloop_ctrl(0.5, 1.0).hex(), "80000000803f")
        self.assertEqual(codec.pack_current_torque(2.0).hex(), "00000040")
        self.assertEqual(codec.pack_single(1.0).hex(), "0000803f")
        self.assertEqual(codec.pack_ctrl_config(eMotorState.MOTOR_OPENLOOP, 1),
                         bytes([eMotorState.MOTOR_OPENLOOP, 1]))
        self.assertEqual(codec.pack_ctrl_config_mit(1.0, 2.0),
                         bytes([eMotorState.MOTOR_MIT_MODE]) + bytes.fromhex("10000001000002"))
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
        heartbeat = codec.pack_heartbeat([1, 0, 48.5, 40, 41])
        self.assertEqual(heartbeat.hex(), "010080302829")
        decoded = codec.unpack_heartbeat(heartbeat)
        self.assertEqual(decoded[:2], [1, 0])
        self.assertAlmostEqual(decoded[2], 48.5)
        self.assertEqual(decoded[3:], [40, 41])
        payload = codec.pack_mit_ctrl(-1.5, 2.5, -3.25, velocity_exp=6, torque_exp=8)
        self.assertEqual(len(payload), 8)
        self.assertEqual(payload.hex(), "0000fa8aa000c0fc")
        self.assertEqual(int.from_bytes(payload[:3], "little", signed=True), -393216)
        self.assertEqual(payload[3], 0x8A)
        with self.assertRaises(ValueError):
            codec.pack_mit_ctrl(0, velocity_exp=9)

    def test_calibration_bytes(self):
        values = {"tune_current": 1.0, "tune_vel": -2.0}
        self.assertEqual(codec.pack_calibration(CmdId.CMD_RL_TUNNING,
                                               {"tune_current": 1.0}).hex(), "0000803f")
        self.assertEqual(codec.pack_calibration(CmdId.CMD_PSIF_TUNNING,
                                               {"tune_vel": -2.0}).hex(), "000000c0")
        for command in (CmdId.CMD_MECH_TUNNING, CmdId.CMD_OBS_TUNNING,
                        CmdId.CMD_PID_TUNNING, CmdId.CMD_MIT_TUNNING):
            self.assertEqual(codec.pack_calibration(command, values).hex(),
                             "0000803f000000c0")

    def test_mit_saturation_and_rounding(self):
        self.assertEqual(codec.pack_mit_ctrl(1000, 1000, -1000).hex(),
                         "ffff7f8aff7f0080")
        self.assertEqual(codec.pack_mit_ctrl(-0.5 / (1 << 18), -0.5, 0.5,
                                            velocity_exp=0, torque_exp=0).hex(),
                         "ffffff00ffff0100")
        for mode in (eMotorState.MOTOR_IDLE, eMotorState.MOTOR_MIT_MODE, 255):
            with self.assertRaises(ValueError):
                codec.pack_ctrl_config(mode)

    def test_limits_and_invalid_values(self):
        self.assertEqual(len(codec.pack_iap_write(0, b"x" * 124)), 128)
        with self.assertRaises(ValueError):
            codec.pack_iap_write(0, b"x" * 125)
        saturated = codec.pack_openloop_ctrl(1e9, 0)
        self.assertEqual(saturated[0:2], b"\xff\xff")
        with self.assertRaises(ValueError):
            codec.pack_openloop_ctrl(math.nan, 0)
        with self.assertRaises(ValueError):
            codec.unpack_heartbeat(b"\x01")
        with self.assertRaises(ValueError):
            codec.pack_value(PARAMETERS[Pidx.PARAM_MOTOR_RS], math.inf)
        offset, status = codec.unpack_iap_write_ack(b"\x78\x56\x34\x12" + bytes([FbId.FB_EXECUTE]))
        self.assertEqual((offset, status), (0x12345678, FbId.FB_EXECUTE))


if __name__ == "__main__":
    unittest.main()
