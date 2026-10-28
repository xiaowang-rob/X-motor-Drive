import xcffib
import struct
import io

MAJOR_VERSION = 1
MINOR_VERSION = 0
key = xcffib.ExtensionKey("XKEYBOARD")
_events = {}
_errors = {}
from . import xproto


class Const:
    MaxLegalKeyCode = 255
    PerKeyBitArraySize = 32
    KeyNameLength = 4


class EventType:
    NewKeyboardNotify = 1 << 0
    MapNotify = 1 << 1
    StateNotify = 1 << 2
    ControlsNotify = 1 << 3
    IndicatorStateNotify = 1 << 4
    IndicatorMapNotify = 1 << 5
    NamesNotify = 1 << 6
    CompatMapNotify = 1 << 7
    BellNotify = 1 << 8
    ActionMessage = 1 << 9
    AccessXNotify = 1 << 10
    ExtensionDeviceNotify = 1 << 11


class NKNDetail:
    Keycodes = 1 << 0
    Geometry = 1 << 1
    DeviceID = 1 << 2


class AXNDetail:
    SKPress = 1 << 0
    SKAccept = 1 << 1
    SKReject = 1 << 2
    SKRelease = 1 << 3
    BKAccept = 1 << 4
    BKReject = 1 << 5
    AXKWarning = 1 << 6


class MapPart:
    KeyTypes = 1 << 0
    KeySyms = 1 << 1
    ModifierMap = 1 << 2
    ExplicitComponents = 1 << 3
    KeyActions = 1 << 4
    KeyBehaviors = 1 << 5
    VirtualMods = 1 << 6
    VirtualModMap = 1 << 7


class SetMapFlags:
    ResizeTypes = 1 << 0
    RecomputeActions = 1 << 1


class StatePart:
    ModifierState = 1 << 0
    ModifierBase = 1 << 1
    ModifierLatch = 1 << 2
    ModifierLock = 1 << 3
    GroupState = 1 << 4
    GroupBase = 1 << 5
    GroupLatch = 1 << 6
    GroupLock = 1 << 7
    CompatState = 1 << 8
    GrabMods = 1 << 9
    CompatGrabMods = 1 << 10
    LookupMods = 1 << 11
    CompatLookupMods = 1 << 12
    PointerButtons = 1 << 13


class BoolCtrl:
    RepeatKeys = 1 << 0
    SlowKeys = 1 << 1
    BounceKeys = 1 << 2
    StickyKeys = 1 << 3
    MouseKeys = 1 << 4
    MouseKeysAccel = 1 << 5
    AccessXKeys = 1 << 6
    AccessXTimeoutMask = 1 << 7
    AccessXFeedbackMask = 1 << 8
    AudibleBellMask = 1 << 9
    Overlay1Mask = 1 << 10
    Overlay2Mask = 1 << 11
    IgnoreGroupLockMask = 1 << 12


class Control:
    GroupsWrap = 1 << 27
    InternalMods = 1 << 28
    IgnoreLockMods = 1 << 29
    PerKeyRepeat = 1 << 30
    ControlsEnabled = 1 << 31


class AXOption:
    SKPressFB = 1 << 0
    SKAcceptFB = 1 << 1
    FeatureFB = 1 << 2
    SlowWarnFB = 1 << 3
    IndicatorFB = 1 << 4
    StickyKeysFB = 1 << 5
    TwoKeys = 1 << 6
    LatchToLock = 1 << 7
    SKReleaseFB = 1 << 8
    SKRejectFB = 1 << 9
    BKRejectFB = 1 << 10
    DumbBell = 1 << 11


class LedClassResult:
    KbdFeedbackClass = 0
    LedFeedbackClass = 4


class LedClass:
    KbdFeedbackClass = 0
    LedFeedbackClass = 4
    DfltXIClass = 768
    AllXIClasses = 1280


class BellClassResult:
    KbdFeedbackClass = 0
    BellFeedbackClass = 5


class BellClass:
    KbdFeedbackClass = 0
    BellFeedbackClass = 5
    DfltXIClass = 768


class ID:
    UseCoreKbd = 256
    UseCorePtr = 512
    DfltXIClass = 768
    DfltXIId = 1024
    AllXIClass = 1280
    AllXIId = 1536
    XINone = 65280


class Group:
    _1 = 0
    _2 = 1
    _3 = 2
    _4 = 3


class Groups:
    Any = 254
    All = 255


class SetOfGroup:
    Group1 = 1 << 0
    Group2 = 1 << 1
    Group3 = 1 << 2
    Group4 = 1 << 3


class SetOfGroups:
    Any = 1 << 7


class GroupsWrap:
    WrapIntoRange = 0
    ClampIntoRange = 1 << 6
    RedirectIntoRange = 1 << 7


class VModsHigh:
    _15 = 1 << 7
    _14 = 1 << 6
    _13 = 1 << 5
    _12 = 1 << 4
    _11 = 1 << 3
    _10 = 1 << 2
    _9 = 1 << 1
    _8 = 1 << 0


class VModsLow:
    _7 = 1 << 7
    _6 = 1 << 6
    _5 = 1 << 5
    _4 = 1 << 4
    _3 = 1 << 3
    _2 = 1 << 2
    _1 = 1 << 1
    _0 = 1 << 0


class VMod:
    _15 = 1 << 15
    _14 = 1 << 14
    _13 = 1 << 13
    _12 = 1 << 12
    _11 = 1 << 11
    _10 = 1 << 10
    _9 = 1 << 9
    _8 = 1 << 8
    _7 = 1 << 7
    _6 = 1 << 6
    _5 = 1 << 5
    _4 = 1 << 4
    _3 = 1 << 3
    _2 = 1 << 2
    _1 = 1 << 1
    _0 = 1 << 0


class Explicit:
    VModMap = 1 << 7
    Behavior = 1 << 6
    AutoRepeat = 1 << 5
    Interpret = 1 << 4
    KeyType4 = 1 << 3
    KeyType3 = 1 << 2
    KeyType2 = 1 << 1
    KeyType1 = 1 << 0


class SymInterpretMatch:
    NoneOf = 0
    AnyOfOrNone = 1
    AnyOf = 2
    AllOf = 3
    Exactly = 4


class SymInterpMatch:
    LevelOneOnly = 1 << 7
    OpMask = 127


class IMFlag:
    NoExplicit = 1 << 7
    NoAutomatic = 1 << 6
    LEDDrivesKB = 1 << 5


class IMModsWhich:
    UseCompat = 1 << 4
    UseEffective = 1 << 3
    UseLocked = 1 << 2
    UseLatched = 1 << 1
    UseBase = 1 << 0


class IMGroupsWhich:
    UseCompat = 1 << 4
    UseEffective = 1 << 3
    UseLocked = 1 << 2
    UseLatched = 1 << 1
    UseBase = 1 << 0


class IndicatorMap(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.flags,
            self.whichGroups,
            self.groups,
            self.whichMods,
            self.mods,
            self.realMods,
            self.vmods,
            self.ctrls,
        ) = unpacker.unpack("BBBBBBHI")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBBBHI",
                self.flags,
                self.whichGroups,
                self.groups,
                self.whichMods,
                self.mods,
                self.realMods,
                self.vmods,
                self.ctrls,
            )
        )
        return buf.getvalue()

    fixed_size = 12

    @classmethod
    def synthetic(
        cls, flags, whichGroups, groups, whichMods, mods, realMods, vmods, ctrls
    ):
        self = cls.__new__(cls)
        self.flags = flags
        self.whichGroups = whichGroups
        self.groups = groups
        self.whichMods = whichMods
        self.mods = mods
        self.realMods = realMods
        self.vmods = vmods
        self.ctrls = ctrls
        return self


class CMDetail:
    SymInterp = 1 << 0
    GroupCompat = 1 << 1


class NameDetail:
    Keycodes = 1 << 0
    Geometry = 1 << 1
    Symbols = 1 << 2
    PhysSymbols = 1 << 3
    Types = 1 << 4
    Compat = 1 << 5
    KeyTypeNames = 1 << 6
    KTLevelNames = 1 << 7
    IndicatorNames = 1 << 8
    KeyNames = 1 << 9
    KeyAliases = 1 << 10
    VirtualModNames = 1 << 11
    GroupNames = 1 << 12
    RGNames = 1 << 13


class GBNDetail:
    Types = 1 << 0
    CompatMap = 1 << 1
    ClientSymbols = 1 << 2
    ServerSymbols = 1 << 3
    IndicatorMaps = 1 << 4
    KeyNames = 1 << 5
    Geometry = 1 << 6
    OtherNames = 1 << 7


class XIFeature:
    Keyboards = 1 << 0
    ButtonActions = 1 << 1
    IndicatorNames = 1 << 2
    IndicatorMaps = 1 << 3
    IndicatorState = 1 << 4


class PerClientFlag:
    DetectableAutoRepeat = 1 << 0
    GrabsUseXKBState = 1 << 1
    AutoResetControls = 1 << 2
    LookupStateWhenGrabbed = 1 << 3
    SendEventUsesXKBState = 1 << 4


class ModDef(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.mask, self.realMods, self.vmods = unpacker.unpack("BBH")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BBH", self.mask, self.realMods, self.vmods))
        return buf.getvalue()

    fixed_size = 4

    @classmethod
    def synthetic(cls, mask, realMods, vmods):
        self = cls.__new__(cls)
        self.mask = mask
        self.realMods = realMods
        self.vmods = vmods
        return self


class KeyName(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.name = xcffib.List(unpacker, "c", 4)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(xcffib.pack_list(self.name, "c"))
        return buf.getvalue()

    fixed_size = 4

    @classmethod
    def synthetic(cls, name):
        self = cls.__new__(cls)
        self.name = name
        return self


class KeyAlias(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.real = xcffib.List(unpacker, "c", 4)
        unpacker.pad("c")
        self.alias = xcffib.List(unpacker, "c", 4)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(xcffib.pack_list(self.real, "c"))
        buf.write(xcffib.pack_list(self.alias, "c"))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, real, alias):
        self = cls.__new__(cls)
        self.real = real
        self.alias = alias
        return self


class CountedString16(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.length,) = unpacker.unpack("H")
        self.string = xcffib.List(unpacker, "c", self.length)
        unpacker.pad("c")
        self.alignment_pad = xcffib.List(
            unpacker, "c", ((self.length + 5) & (~3)) - (self.length + 2)
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=H", self.length))
        buf.write(xcffib.pack_list(self.string, "c"))
        buf.write(xcffib.pack_list(self.alignment_pad, "c"))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, length, string, alignment_pad):
        self = cls.__new__(cls)
        self.length = length
        self.string = string
        self.alignment_pad = alignment_pad
        return self


class KTMapEntry(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.active, self.mods_mask, self.level, self.mods_mods, self.mods_vmods = (
            unpacker.unpack("BBBBH2x")
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBH2x",
                self.active,
                self.mods_mask,
                self.level,
                self.mods_mods,
                self.mods_vmods,
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, active, mods_mask, level, mods_mods, mods_vmods):
        self = cls.__new__(cls)
        self.active = active
        self.mods_mask = mods_mask
        self.level = level
        self.mods_mods = mods_mods
        self.mods_vmods = mods_vmods
        return self


class KeyType(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.mods_mask,
            self.mods_mods,
            self.mods_vmods,
            self.numLevels,
            self.nMapEntries,
            self.hasPreserve,
        ) = unpacker.unpack("BBHBBBx")
        self.map = xcffib.List(unpacker, KTMapEntry, self.nMapEntries)
        unpacker.pad(ModDef)
        self.preserve = xcffib.List(
            unpacker, ModDef, self.hasPreserve * self.nMapEntries
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBHBBBx",
                self.mods_mask,
                self.mods_mods,
                self.mods_vmods,
                self.numLevels,
                self.nMapEntries,
                self.hasPreserve,
            )
        )
        buf.write(xcffib.pack_list(self.map, KTMapEntry))
        buf.write(xcffib.pack_list(self.preserve, ModDef))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        mods_mask,
        mods_mods,
        mods_vmods,
        numLevels,
        nMapEntries,
        hasPreserve,
        map,
        preserve,
    ):
        self = cls.__new__(cls)
        self.mods_mask = mods_mask
        self.mods_mods = mods_mods
        self.mods_vmods = mods_vmods
        self.numLevels = numLevels
        self.nMapEntries = nMapEntries
        self.hasPreserve = hasPreserve
        self.map = map
        self.preserve = preserve
        return self


class KeySymMap(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.kt_index = xcffib.List(unpacker, "B", 4)
        self.groupInfo, self.width, self.nSyms = unpacker.unpack("BBH")
        unpacker.pad("I")
        self.syms = xcffib.List(unpacker, "I", self.nSyms)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(xcffib.pack_list(self.kt_index, "B"))
        buf.write(struct.pack("=B", self.groupInfo))
        buf.write(struct.pack("=B", self.width))
        buf.write(struct.pack("=H", self.nSyms))
        buf.write(xcffib.pack_list(self.syms, "I"))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, kt_index, groupInfo, width, nSyms, syms):
        self = cls.__new__(cls)
        self.kt_index = kt_index
        self.groupInfo = groupInfo
        self.width = width
        self.nSyms = nSyms
        self.syms = syms
        return self


class CommonBehavior(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.data = unpacker.unpack("BB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB", self.type, self.data))
        return buf.getvalue()

    fixed_size = 2

    @classmethod
    def synthetic(cls, type, data):
        self = cls.__new__(cls)
        self.type = type
        self.data = data
        return self


class DefaultBehavior(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.type,) = unpacker.unpack("Bx")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=Bx", self.type))
        return buf.getvalue()

    fixed_size = 2

    @classmethod
    def synthetic(cls, type):
        self = cls.__new__(cls)
        self.type = type
        return self


class RadioGroupBehavior(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.group = unpacker.unpack("BB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB", self.type, self.group))
        return buf.getvalue()

    fixed_size = 2

    @classmethod
    def synthetic(cls, type, group):
        self = cls.__new__(cls)
        self.type = type
        self.group = group
        return self


class OverlayBehavior(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.key = unpacker.unpack("BB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB", self.type, self.key))
        return buf.getvalue()

    fixed_size = 2

    @classmethod
    def synthetic(cls, type, key):
        self = cls.__new__(cls)
        self.type = type
        self.key = key
        return self


class Behavior(xcffib.Union):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Union.__init__(self, unpacker)
        self.common = CommonBehavior(unpacker.copy())
        self.default = DefaultBehavior(unpacker.copy())
        self.lock = DefaultBehavior(unpacker.copy())
        self.radioGroup = RadioGroupBehavior(unpacker.copy())
        self.overlay1 = OverlayBehavior(unpacker.copy())
        self.overlay2 = OverlayBehavior(unpacker.copy())
        self.permamentLock = DefaultBehavior(unpacker.copy())
        self.permamentRadioGroup = RadioGroupBehavior(unpacker.copy())
        self.permamentOverlay1 = OverlayBehavior(unpacker.copy())
        self.permamentOverlay2 = OverlayBehavior(unpacker.copy())
        (self.type,) = unpacker.copy().unpack("B")

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            self.common.pack()
            if hasattr(self.common, "pack")
            else CommonBehavior.synthetic(*self.common).pack()
        )
        return buf.getvalue()


class BehaviorType:
    Default = 0
    Lock = 1
    RadioGroup = 2
    Overlay1 = 3
    Overlay2 = 4
    PermamentLock = 129
    PermamentRadioGroup = 130
    PermamentOverlay1 = 131
    PermamentOverlay2 = 132


class SetBehavior(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.keycode,) = unpacker.unpack("B")
        self.behavior = Behavior(unpacker)
        unpacker.unpack("x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", self.keycode))
        buf.write(
            self.behavior.pack()
            if hasattr(self.behavior, "pack")
            else Behavior.synthetic(*self.behavior).pack()
        )
        buf.write(
            struct.pack(
                "=x",
            )
        )
        return buf.getvalue()

    @classmethod
    def synthetic(cls, keycode, behavior):
        self = cls.__new__(cls)
        self.keycode = keycode
        self.behavior = behavior
        return self


class SetExplicit(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.keycode, self.explicit = unpacker.unpack("BB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB", self.keycode, self.explicit))
        return buf.getvalue()

    fixed_size = 2

    @classmethod
    def synthetic(cls, keycode, explicit):
        self = cls.__new__(cls)
        self.keycode = keycode
        self.explicit = explicit
        return self


class KeyModMap(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.keycode, self.mods = unpacker.unpack("BB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB", self.keycode, self.mods))
        return buf.getvalue()

    fixed_size = 2

    @classmethod
    def synthetic(cls, keycode, mods):
        self = cls.__new__(cls)
        self.keycode = keycode
        self.mods = mods
        return self


class KeyVModMap(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.keycode, self.vmods = unpacker.unpack("BxH")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BxH", self.keycode, self.vmods))
        return buf.getvalue()

    fixed_size = 4

    @classmethod
    def synthetic(cls, keycode, vmods):
        self = cls.__new__(cls)
        self.keycode = keycode
        self.vmods = vmods
        return self


class KTSetMapEntry(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.level, self.realMods, self.virtualMods = unpacker.unpack("BBH")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BBH", self.level, self.realMods, self.virtualMods))
        return buf.getvalue()

    fixed_size = 4

    @classmethod
    def synthetic(cls, level, realMods, virtualMods):
        self = cls.__new__(cls)
        self.level = level
        self.realMods = realMods
        self.virtualMods = virtualMods
        return self


class SetKeyType(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.mask,
            self.realMods,
            self.virtualMods,
            self.numLevels,
            self.nMapEntries,
            self.preserve,
        ) = unpacker.unpack("BBHBBBx")
        self.entries = xcffib.List(unpacker, KTSetMapEntry, self.nMapEntries)
        unpacker.pad(KTSetMapEntry)
        self.preserve_entries = xcffib.List(
            unpacker, KTSetMapEntry, self.preserve * self.nMapEntries
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBHBBBx",
                self.mask,
                self.realMods,
                self.virtualMods,
                self.numLevels,
                self.nMapEntries,
                self.preserve,
            )
        )
        buf.write(xcffib.pack_list(self.entries, KTSetMapEntry))
        buf.write(xcffib.pack_list(self.preserve_entries, KTSetMapEntry))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        mask,
        realMods,
        virtualMods,
        numLevels,
        nMapEntries,
        preserve,
        entries,
        preserve_entries,
    ):
        self = cls.__new__(cls)
        self.mask = mask
        self.realMods = realMods
        self.virtualMods = virtualMods
        self.numLevels = numLevels
        self.nMapEntries = nMapEntries
        self.preserve = preserve
        self.entries = entries
        self.preserve_entries = preserve_entries
        return self


class Outline(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.nPoints, self.cornerRadius = unpacker.unpack("BB2x")
        self.points = xcffib.List(unpacker, xproto.POINT, self.nPoints)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB2x", self.nPoints, self.cornerRadius))
        buf.write(xcffib.pack_list(self.points, xproto.POINT))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, nPoints, cornerRadius, points):
        self = cls.__new__(cls)
        self.nPoints = nPoints
        self.cornerRadius = cornerRadius
        self.points = points
        return self


class Shape(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.name, self.nOutlines, self.primaryNdx, self.approxNdx = unpacker.unpack(
            "IBBBx"
        )
        self.outlines = xcffib.List(unpacker, Outline, self.nOutlines)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=IBBBx", self.name, self.nOutlines, self.primaryNdx, self.approxNdx
            )
        )
        buf.write(xcffib.pack_list(self.outlines, Outline))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, name, nOutlines, primaryNdx, approxNdx, outlines):
        self = cls.__new__(cls)
        self.name = name
        self.nOutlines = nOutlines
        self.primaryNdx = primaryNdx
        self.approxNdx = approxNdx
        self.outlines = outlines
        return self


class Key(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.name = xcffib.List(unpacker, "c", 4)
        self.gap, self.shapeNdx, self.colorNdx = unpacker.unpack("hBB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(xcffib.pack_list(self.name, "c"))
        buf.write(struct.pack("=h", self.gap))
        buf.write(struct.pack("=B", self.shapeNdx))
        buf.write(struct.pack("=B", self.colorNdx))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, name, gap, shapeNdx, colorNdx):
        self = cls.__new__(cls)
        self.name = name
        self.gap = gap
        self.shapeNdx = shapeNdx
        self.colorNdx = colorNdx
        return self


class OverlayKey(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.over = xcffib.List(unpacker, "c", 4)
        unpacker.pad("c")
        self.under = xcffib.List(unpacker, "c", 4)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(xcffib.pack_list(self.over, "c"))
        buf.write(xcffib.pack_list(self.under, "c"))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, over, under):
        self = cls.__new__(cls)
        self.over = over
        self.under = under
        return self


class OverlayRow(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.rowUnder, self.nKeys = unpacker.unpack("BB2x")
        self.keys = xcffib.List(unpacker, OverlayKey, self.nKeys)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB2x", self.rowUnder, self.nKeys))
        buf.write(xcffib.pack_list(self.keys, OverlayKey))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, rowUnder, nKeys, keys):
        self = cls.__new__(cls)
        self.rowUnder = rowUnder
        self.nKeys = nKeys
        self.keys = keys
        return self


class Overlay(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.name, self.nRows = unpacker.unpack("IB3x")
        self.rows = xcffib.List(unpacker, OverlayRow, self.nRows)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=IB3x", self.name, self.nRows))
        buf.write(xcffib.pack_list(self.rows, OverlayRow))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, name, nRows, rows):
        self = cls.__new__(cls)
        self.name = name
        self.nRows = nRows
        self.rows = rows
        return self


class Row(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.top, self.left, self.nKeys, self.vertical = unpacker.unpack("hhBB2x")
        self.keys = xcffib.List(unpacker, Key, self.nKeys)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack("=hhBB2x", self.top, self.left, self.nKeys, self.vertical)
        )
        buf.write(xcffib.pack_list(self.keys, Key))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, top, left, nKeys, vertical, keys):
        self = cls.__new__(cls)
        self.top = top
        self.left = left
        self.nKeys = nKeys
        self.vertical = vertical
        self.keys = keys
        return self


class DoodadType:
    Outline = 1
    Solid = 2
    Text = 3
    Indicator = 4
    Logo = 5


class Listing(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.flags, self.length = unpacker.unpack("HH")
        self.string = xcffib.List(unpacker, "c", self.length)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=HH", self.flags, self.length))
        buf.write(xcffib.pack_list(self.string, "c"))
        buf.write(
            struct.pack(
                "=2x",
            )
        )
        return buf.getvalue()

    @classmethod
    def synthetic(cls, flags, length, string):
        self = cls.__new__(cls)
        self.flags = flags
        self.length = length
        self.string = string
        return self


class DeviceLedInfo(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.ledClass,
            self.ledID,
            self.namesPresent,
            self.mapsPresent,
            self.physIndicators,
            self.state,
        ) = unpacker.unpack("HHIIII")
        self.names = xcffib.List(unpacker, "I", xcffib.popcount(self.namesPresent))
        unpacker.pad(IndicatorMap)
        self.maps = xcffib.List(
            unpacker, IndicatorMap, xcffib.popcount(self.mapsPresent)
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=HHIIII",
                self.ledClass,
                self.ledID,
                self.namesPresent,
                self.mapsPresent,
                self.physIndicators,
                self.state,
            )
        )
        buf.write(xcffib.pack_list(self.names, "I"))
        buf.write(xcffib.pack_list(self.maps, IndicatorMap))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        ledClass,
        ledID,
        namesPresent,
        mapsPresent,
        physIndicators,
        state,
        names,
        maps,
    ):
        self = cls.__new__(cls)
        self.ledClass = ledClass
        self.ledID = ledID
        self.namesPresent = namesPresent
        self.mapsPresent = mapsPresent
        self.physIndicators = physIndicators
        self.state = state
        self.names = names
        self.maps = maps
        return self


class Error:
    BadDevice = 255
    BadClass = 254
    BadId = 253


class KeyboardError(xcffib.Error):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Error.__init__(self, unpacker)
        base = unpacker.offset
        self.value, self.minorOpcode, self.majorOpcode = unpacker.unpack("xx2xIHB21x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 0))
        buf.write(
            struct.pack("=x2xIHB21x", self.value, self.minorOpcode, self.majorOpcode)
        )
        return buf.getvalue()


BadKeyboard = KeyboardError
_errors[0] = KeyboardError


class SA:
    ClearLocks = 1 << 0
    LatchToLock = 1 << 1
    UseModMapMods = 1 << 2
    GroupAbsolute = 1 << 2


class SAType:
    NoAction = 0
    SetMods = 1
    LatchMods = 2
    LockMods = 3
    SetGroup = 4
    LatchGroup = 5
    LockGroup = 6
    MovePtr = 7
    PtrBtn = 8
    LockPtrBtn = 9
    SetPtrDflt = 10
    ISOLock = 11
    Terminate = 12
    SwitchScreen = 13
    SetControls = 14
    LockControls = 15
    ActionMessage = 16
    RedirectKey = 17
    DeviceBtn = 18
    LockDeviceBtn = 19
    DeviceValuator = 20


class SANoAction(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.type,) = unpacker.unpack("B7x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B7x", self.type))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type):
        self = cls.__new__(cls)
        self.type = type
        return self


class SASetMods(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.type,
            self.flags,
            self.mask,
            self.realMods,
            self.vmodsHigh,
            self.vmodsLow,
        ) = unpacker.unpack("BBBBBB2x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBBB2x",
                self.type,
                self.flags,
                self.mask,
                self.realMods,
                self.vmodsHigh,
                self.vmodsLow,
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, mask, realMods, vmodsHigh, vmodsLow):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.mask = mask
        self.realMods = realMods
        self.vmodsHigh = vmodsHigh
        self.vmodsLow = vmodsLow
        return self


class SASetGroup(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.group = unpacker.unpack("BBb5x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BBb5x", self.type, self.flags, self.group))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, group):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.group = group
        return self


class SAMovePtrFlag:
    NoAcceleration = 1 << 0
    MoveAbsoluteX = 1 << 1
    MoveAbsoluteY = 1 << 2


class SAMovePtr(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.xHigh, self.xLow, self.yHigh, self.yLow = (
            unpacker.unpack("BBbBbB2x")
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBbBbB2x",
                self.type,
                self.flags,
                self.xHigh,
                self.xLow,
                self.yHigh,
                self.yLow,
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, xHigh, xLow, yHigh, yLow):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.xHigh = xHigh
        self.xLow = xLow
        self.yHigh = yHigh
        self.yLow = yLow
        return self


class SAPtrBtn(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.count, self.button = unpacker.unpack("BBBB4x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack("=BBBB4x", self.type, self.flags, self.count, self.button)
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, count, button):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.count = count
        self.button = button
        return self


class SALockPtrBtn(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.button = unpacker.unpack("BBxB4x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BBxB4x", self.type, self.flags, self.button))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, button):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.button = button
        return self


class SASetPtrDfltFlag:
    DfltBtnAbsolute = 1 << 2
    AffectDfltButton = 1 << 0


class SASetPtrDflt(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.affect, self.value = unpacker.unpack("BBBb4x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack("=BBBb4x", self.type, self.flags, self.affect, self.value)
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, affect, value):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.affect = affect
        self.value = value
        return self


class SAIsoLockFlag:
    NoLock = 1 << 0
    NoUnlock = 1 << 1
    UseModMapMods = 1 << 2
    GroupAbsolute = 1 << 2
    ISODfltIsGroup = 1 << 3


class SAIsoLockNoAffect:
    Ctrls = 1 << 3
    Ptr = 1 << 4
    Group = 1 << 5
    Mods = 1 << 6


class SAIsoLock(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.type,
            self.flags,
            self.mask,
            self.realMods,
            self.group,
            self.affect,
            self.vmodsHigh,
            self.vmodsLow,
        ) = unpacker.unpack("BBBBbBBB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBbBBB",
                self.type,
                self.flags,
                self.mask,
                self.realMods,
                self.group,
                self.affect,
                self.vmodsHigh,
                self.vmodsLow,
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, mask, realMods, group, affect, vmodsHigh, vmodsLow):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.mask = mask
        self.realMods = realMods
        self.group = group
        self.affect = affect
        self.vmodsHigh = vmodsHigh
        self.vmodsLow = vmodsLow
        return self


class SATerminate(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.type,) = unpacker.unpack("B7x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B7x", self.type))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type):
        self = cls.__new__(cls)
        self.type = type
        return self


class SwitchScreenFlag:
    Application = 1 << 0
    Absolute = 1 << 2


class SASwitchScreen(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.newScreen = unpacker.unpack("BBb5x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BBb5x", self.type, self.flags, self.newScreen))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, newScreen):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.newScreen = newScreen
        return self


class BoolCtrlsHigh:
    AccessXFeedback = 1 << 0
    AudibleBell = 1 << 1
    Overlay1 = 1 << 2
    Overlay2 = 1 << 3
    IgnoreGroupLock = 1 << 4


class BoolCtrlsLow:
    RepeatKeys = 1 << 0
    SlowKeys = 1 << 1
    BounceKeys = 1 << 2
    StickyKeys = 1 << 3
    MouseKeys = 1 << 4
    MouseKeysAccel = 1 << 5
    AccessXKeys = 1 << 6
    AccessXTimeout = 1 << 7


class SASetControls(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.boolCtrlsHigh, self.boolCtrlsLow = unpacker.unpack("B3xBB2x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack("=B3xBB2x", self.type, self.boolCtrlsHigh, self.boolCtrlsLow)
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, boolCtrlsHigh, boolCtrlsLow):
        self = cls.__new__(cls)
        self.type = type
        self.boolCtrlsHigh = boolCtrlsHigh
        self.boolCtrlsLow = boolCtrlsLow
        return self


class ActionMessageFlag:
    OnPress = 1 << 0
    OnRelease = 1 << 1
    GenKeyEvent = 1 << 2


class SAActionMessage(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags = unpacker.unpack("BB")
        self.message = xcffib.List(unpacker, "B", 6)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=BB", self.type, self.flags))
        buf.write(xcffib.pack_list(self.message, "B"))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, message):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.message = message
        return self


class SARedirectKey(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.type,
            self.newkey,
            self.mask,
            self.realModifiers,
            self.vmodsMaskHigh,
            self.vmodsMaskLow,
            self.vmodsHigh,
            self.vmodsLow,
        ) = unpacker.unpack("BBBBBBBB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBBBBB",
                self.type,
                self.newkey,
                self.mask,
                self.realModifiers,
                self.vmodsMaskHigh,
                self.vmodsMaskLow,
                self.vmodsHigh,
                self.vmodsLow,
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(
        cls,
        type,
        newkey,
        mask,
        realModifiers,
        vmodsMaskHigh,
        vmodsMaskLow,
        vmodsHigh,
        vmodsLow,
    ):
        self = cls.__new__(cls)
        self.type = type
        self.newkey = newkey
        self.mask = mask
        self.realModifiers = realModifiers
        self.vmodsMaskHigh = vmodsMaskHigh
        self.vmodsMaskLow = vmodsMaskLow
        self.vmodsHigh = vmodsHigh
        self.vmodsLow = vmodsLow
        return self


class SADeviceBtn(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.count, self.button, self.device = unpacker.unpack(
            "BBBBB3x"
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBB3x", self.type, self.flags, self.count, self.button, self.device
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, count, button, device):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.count = count
        self.button = button
        self.device = device
        return self


class LockDeviceFlags:
    NoLock = 1 << 0
    NoUnlock = 1 << 1


class SALockDeviceBtn(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.type, self.flags, self.button, self.device = unpacker.unpack("BBxBB3x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack("=BBxBB3x", self.type, self.flags, self.button, self.device)
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, flags, button, device):
        self = cls.__new__(cls)
        self.type = type
        self.flags = flags
        self.button = button
        self.device = device
        return self


class SAValWhat:
    IgnoreVal = 0
    SetValMin = 1
    SetValCenter = 2
    SetValMax = 3
    SetValRelative = 4
    SetValAbsolute = 5


class SADeviceValuator(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.type,
            self.device,
            self.val1what,
            self.val1index,
            self.val1value,
            self.val2what,
            self.val2index,
            self.val2value,
        ) = unpacker.unpack("BBBBBBBB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=BBBBBBBB",
                self.type,
                self.device,
                self.val1what,
                self.val1index,
                self.val1value,
                self.val2what,
                self.val2index,
                self.val2value,
            )
        )
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(
        cls,
        type,
        device,
        val1what,
        val1index,
        val1value,
        val2what,
        val2index,
        val2value,
    ):
        self = cls.__new__(cls)
        self.type = type
        self.device = device
        self.val1what = val1what
        self.val1index = val1index
        self.val1value = val1value
        self.val2what = val2what
        self.val2index = val2index
        self.val2value = val2value
        return self


class SIAction(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.type,) = unpacker.unpack("B")
        self.data = xcffib.List(unpacker, "B", 7)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", self.type))
        buf.write(xcffib.pack_list(self.data, "B"))
        return buf.getvalue()

    fixed_size = 8

    @classmethod
    def synthetic(cls, type, data):
        self = cls.__new__(cls)
        self.type = type
        self.data = data
        return self


class SymInterpret(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        self.sym, self.mods, self.match, self.virtualMod, self.flags = unpacker.unpack(
            "IBBBB"
        )
        self.action = SIAction(unpacker)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=IBBBB", self.sym, self.mods, self.match, self.virtualMod, self.flags
            )
        )
        buf.write(
            self.action.pack()
            if hasattr(self.action, "pack")
            else SIAction.synthetic(*self.action).pack()
        )
        return buf.getvalue()

    @classmethod
    def synthetic(cls, sym, mods, match, virtualMod, flags, action):
        self = cls.__new__(cls)
        self.sym = sym
        self.mods = mods
        self.match = match
        self.virtualMod = virtualMod
        self.flags = flags
        self.action = action
        return self


class Action(xcffib.Union):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Union.__init__(self, unpacker)
        self.noaction = SANoAction(unpacker.copy())
        self.setmods = SASetMods(unpacker.copy())
        self.latchmods = SASetMods(unpacker.copy())
        self.lockmods = SASetMods(unpacker.copy())
        self.setgroup = SASetGroup(unpacker.copy())
        self.latchgroup = SASetGroup(unpacker.copy())
        self.lockgroup = SASetGroup(unpacker.copy())
        self.moveptr = SAMovePtr(unpacker.copy())
        self.ptrbtn = SAPtrBtn(unpacker.copy())
        self.lockptrbtn = SALockPtrBtn(unpacker.copy())
        self.setptrdflt = SASetPtrDflt(unpacker.copy())
        self.isolock = SAIsoLock(unpacker.copy())
        self.terminate = SATerminate(unpacker.copy())
        self.switchscreen = SASwitchScreen(unpacker.copy())
        self.setcontrols = SASetControls(unpacker.copy())
        self.lockcontrols = SASetControls(unpacker.copy())
        self.message = SAActionMessage(unpacker.copy())
        self.redirect = SARedirectKey(unpacker.copy())
        self.devbtn = SADeviceBtn(unpacker.copy())
        self.lockdevbtn = SALockDeviceBtn(unpacker.copy())
        self.devval = SADeviceValuator(unpacker.copy())
        (self.type,) = unpacker.copy().unpack("B")

    def pack(self):
        buf = io.BytesIO()
        buf.write(
            self.noaction.pack()
            if hasattr(self.noaction, "pack")
            else SANoAction.synthetic(*self.noaction).pack()
        )
        return buf.getvalue()


class UseExtensionReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.supported, self.serverMajor, self.serverMinor = unpacker.unpack(
            "xB2x4xHH20x"
        )
        self.bufsize = unpacker.offset - base


class UseExtensionCookie(xcffib.Cookie):
    reply_type = UseExtensionReply


class GetStateReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.mods,
            self.baseMods,
            self.latchedMods,
            self.lockedMods,
            self.group,
            self.lockedGroup,
            self.baseGroup,
            self.latchedGroup,
            self.compatState,
            self.grabMods,
            self.compatGrabMods,
            self.lookupMods,
            self.compatLookupMods,
            self.ptrBtnState,
        ) = unpacker.unpack("xB2x4xBBBBBBhhBBBBBxH6x")
        self.bufsize = unpacker.offset - base


class GetStateCookie(xcffib.Cookie):
    reply_type = GetStateReply


class GetControlsReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.mouseKeysDfltBtn,
            self.numGroups,
            self.groupsWrap,
            self.internalModsMask,
            self.ignoreLockModsMask,
            self.internalModsRealMods,
            self.ignoreLockModsRealMods,
            self.internalModsVmods,
            self.ignoreLockModsVmods,
            self.repeatDelay,
            self.repeatInterval,
            self.slowKeysDelay,
            self.debounceDelay,
            self.mouseKeysDelay,
            self.mouseKeysInterval,
            self.mouseKeysTimeToMax,
            self.mouseKeysMaxSpeed,
            self.mouseKeysCurve,
            self.accessXOption,
            self.accessXTimeout,
            self.accessXTimeoutOptionsMask,
            self.accessXTimeoutOptionsValues,
            self.accessXTimeoutMask,
            self.accessXTimeoutValues,
            self.enabledControls,
        ) = unpacker.unpack("xB2x4xBBBBBBBxHHHHHHHHHHhHHHH2xIII")
        self.perKeyRepeat = xcffib.List(unpacker, "B", 32)
        self.bufsize = unpacker.offset - base


class GetControlsCookie(xcffib.Cookie):
    reply_type = GetControlsReply


class GetMapReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.minKeyCode,
            self.maxKeyCode,
            self.present,
            self.firstType,
            self.nTypes,
            self.totalTypes,
            self.firstKeySym,
            self.totalSyms,
            self.nKeySyms,
            self.firstKeyAction,
            self.totalActions,
            self.nKeyActions,
            self.firstKeyBehavior,
            self.nKeyBehaviors,
            self.totalKeyBehaviors,
            self.firstKeyExplicit,
            self.nKeyExplicit,
            self.totalKeyExplicit,
            self.firstModMapKey,
            self.nModMapKeys,
            self.totalModMapKeys,
            self.firstVModMapKey,
            self.nVModMapKeys,
            self.totalVModMapKeys,
            self.virtualMods,
        ) = unpacker.unpack("xB2x4x2xBBHBBBBHBBHBBBBBBBBBBBBBxH")
        if self.present & MapPart.KeyTypes:
            self.types_rtrn = xcffib.List(unpacker, KeyType, self.nTypes)
        if self.present & MapPart.KeySyms:
            self.syms_rtrn = xcffib.List(unpacker, KeySymMap, self.nKeySyms)
        if self.present & MapPart.KeyActions:
            self.acts_rtrn_count = xcffib.List(unpacker, "B", self.nKeyActions)
            unpacker.pad(Action)
            self.acts_rtrn_acts = xcffib.List(unpacker, Action, self.totalActions)
        if self.present & MapPart.KeyBehaviors:
            self.behaviors_rtrn = xcffib.List(
                unpacker, SetBehavior, self.totalKeyBehaviors
            )
        if self.present & MapPart.VirtualMods:
            self.vmods_rtrn = xcffib.List(
                unpacker, "B", xcffib.popcount(self.virtualMods)
            )
        if self.present & MapPart.ExplicitComponents:
            self.explicit_rtrn = xcffib.List(
                unpacker, SetExplicit, self.totalKeyExplicit
            )
        if self.present & MapPart.ModifierMap:
            self.modmap_rtrn = xcffib.List(unpacker, KeyModMap, self.totalModMapKeys)
        if self.present & MapPart.VirtualModMap:
            self.vmodmap_rtrn = xcffib.List(unpacker, KeyVModMap, self.totalVModMapKeys)
        self.bufsize = unpacker.offset - base


class GetMapCookie(xcffib.Cookie):
    reply_type = GetMapReply


class GetCompatMapReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.groupsRtrn,
            self.firstSIRtrn,
            self.nSIRtrn,
            self.nTotalSI,
        ) = unpacker.unpack("xB2x4xBxHHH16x")
        self.si_rtrn = xcffib.List(unpacker, SymInterpret, self.nSIRtrn)
        unpacker.pad(ModDef)
        self.group_rtrn = xcffib.List(
            unpacker, ModDef, xcffib.popcount(self.groupsRtrn)
        )
        self.bufsize = unpacker.offset - base


class GetCompatMapCookie(xcffib.Cookie):
    reply_type = GetCompatMapReply


class GetIndicatorStateReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.deviceID, self.state = unpacker.unpack("xB2x4xI20x")
        self.bufsize = unpacker.offset - base


class GetIndicatorStateCookie(xcffib.Cookie):
    reply_type = GetIndicatorStateReply


class GetIndicatorMapReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.deviceID, self.which, self.realIndicators, self.nIndicators = (
            unpacker.unpack("xB2x4xIIB15x")
        )
        self.maps = xcffib.List(unpacker, IndicatorMap, xcffib.popcount(self.which))
        self.bufsize = unpacker.offset - base


class GetIndicatorMapCookie(xcffib.Cookie):
    reply_type = GetIndicatorMapReply


class GetNamedIndicatorReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.indicator,
            self.found,
            self.on,
            self.realIndicator,
            self.ndx,
            self.map_flags,
            self.map_whichGroups,
            self.map_groups,
            self.map_whichMods,
            self.map_mods,
            self.map_realMods,
            self.map_vmod,
            self.map_ctrls,
            self.supported,
        ) = unpacker.unpack("xB2x4xIBBBBBBBBBBHIB3x")
        self.bufsize = unpacker.offset - base


class GetNamedIndicatorCookie(xcffib.Cookie):
    reply_type = GetNamedIndicatorReply


class GetNamesReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.which,
            self.minKeyCode,
            self.maxKeyCode,
            self.nTypes,
            self.groupNames,
            self.virtualMods,
            self.firstKey,
            self.nKeys,
            self.indicators,
            self.nRadioGroups,
            self.nKeyAliases,
            self.nKTLevels,
        ) = unpacker.unpack("xB2x4xIBBBBHBBIBBH4x")
        if self.which & NameDetail.Keycodes:
            (self.keycodesName,) = unpacker.unpack("I")
        if self.which & NameDetail.Geometry:
            (self.geometryName,) = unpacker.unpack("I")
        if self.which & NameDetail.Symbols:
            (self.symbolsName,) = unpacker.unpack("I")
        if self.which & NameDetail.PhysSymbols:
            (self.physSymbolsName,) = unpacker.unpack("I")
        if self.which & NameDetail.Types:
            (self.typesName,) = unpacker.unpack("I")
        if self.which & NameDetail.Compat:
            (self.compatName,) = unpacker.unpack("I")
        if self.which & NameDetail.KeyTypeNames:
            self.typeNames = xcffib.List(unpacker, "I", self.nTypes)
        if self.which & NameDetail.KTLevelNames:
            self.nLevelsPerType = xcffib.List(unpacker, "B", self.nTypes)
            unpacker.pad("I")
            self.ktLevelNames = xcffib.List(unpacker, "I", sum(self.nLevelsPerType))
        if self.which & NameDetail.IndicatorNames:
            self.indicatorNames = xcffib.List(
                unpacker, "I", xcffib.popcount(self.indicators)
            )
        if self.which & NameDetail.VirtualModNames:
            self.virtualModNames = xcffib.List(
                unpacker, "I", xcffib.popcount(self.virtualMods)
            )
        if self.which & NameDetail.GroupNames:
            self.groups = xcffib.List(unpacker, "I", xcffib.popcount(self.groupNames))
        if self.which & NameDetail.KeyNames:
            self.keyNames = xcffib.List(unpacker, KeyName, self.nKeys)
        if self.which & NameDetail.KeyAliases:
            self.keyAliases = xcffib.List(unpacker, KeyAlias, self.nKeyAliases)
        if self.which & NameDetail.RGNames:
            self.radioGroupNames = xcffib.List(unpacker, "I", self.nRadioGroups)
        self.bufsize = unpacker.offset - base


class GetNamesCookie(xcffib.Cookie):
    reply_type = GetNamesReply


class PerClientFlagsReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.supported,
            self.value,
            self.autoCtrls,
            self.autoCtrlsValues,
        ) = unpacker.unpack("xB2x4xIIII8x")
        self.bufsize = unpacker.offset - base


class PerClientFlagsCookie(xcffib.Cookie):
    reply_type = PerClientFlagsReply


class ListComponentsReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.nKeymaps,
            self.nKeycodes,
            self.nTypes,
            self.nCompatMaps,
            self.nSymbols,
            self.nGeometries,
            self.extra,
        ) = unpacker.unpack("xB2x4xHHHHHHH10x")
        self.keymaps = xcffib.List(unpacker, Listing, self.nKeymaps)
        unpacker.pad(Listing)
        self.keycodes = xcffib.List(unpacker, Listing, self.nKeycodes)
        unpacker.pad(Listing)
        self.types = xcffib.List(unpacker, Listing, self.nTypes)
        unpacker.pad(Listing)
        self.compatMaps = xcffib.List(unpacker, Listing, self.nCompatMaps)
        unpacker.pad(Listing)
        self.symbols = xcffib.List(unpacker, Listing, self.nSymbols)
        unpacker.pad(Listing)
        self.geometries = xcffib.List(unpacker, Listing, self.nGeometries)
        self.bufsize = unpacker.offset - base


class ListComponentsCookie(xcffib.Cookie):
    reply_type = ListComponentsReply


class GetKbdByNameReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.minKeyCode,
            self.maxKeyCode,
            self.loaded,
            self.newKeyboard,
            self.found,
            self.reported,
        ) = unpacker.unpack("xB2x4xBBBBHH16x")
        if self.reported == GBNDetail.Types:
            (
                self.getmap_type,
                self.typeDeviceID,
                self.getmap_sequence,
                self.getmap_length,
                self.typeMinKeyCode,
                self.typeMaxKeyCode,
                self.present,
                self.firstType,
                self.nTypes,
                self.totalTypes,
                self.firstKeySym,
                self.totalSyms,
                self.nKeySyms,
                self.firstKeyAction,
                self.totalActions,
                self.nKeyActions,
                self.firstKeyBehavior,
                self.nKeyBehaviors,
                self.totalKeyBehaviors,
                self.firstKeyExplicit,
                self.nKeyExplicit,
                self.totalKeyExplicit,
                self.firstModMapKey,
                self.nModMapKeys,
                self.totalModMapKeys,
                self.firstVModMapKey,
                self.nVModMapKeys,
                self.totalVModMapKeys,
                self.virtualMods,
            ) = unpacker.unpack("BBHI2xBBHBBBBHBBHBBBBBBBBBBBBBxH")
            if self.present & MapPart.KeyTypes:
                self.types_rtrn = xcffib.List(unpacker, KeyType, self.nTypes)
            if self.present & MapPart.KeySyms:
                self.syms_rtrn = xcffib.List(unpacker, KeySymMap, self.nKeySyms)
            if self.present & MapPart.KeyActions:
                self.acts_rtrn_count = xcffib.List(unpacker, "B", self.nKeyActions)
                unpacker.pad(Action)
                self.acts_rtrn_acts = xcffib.List(unpacker, Action, self.totalActions)
            if self.present & MapPart.KeyBehaviors:
                self.behaviors_rtrn = xcffib.List(
                    unpacker, SetBehavior, self.totalKeyBehaviors
                )
            if self.present & MapPart.VirtualMods:
                self.vmods_rtrn = xcffib.List(
                    unpacker, "B", xcffib.popcount(self.virtualMods)
                )
            if self.present & MapPart.ExplicitComponents:
                self.explicit_rtrn = xcffib.List(
                    unpacker, SetExplicit, self.totalKeyExplicit
                )
            if self.present & MapPart.ModifierMap:
                self.modmap_rtrn = xcffib.List(
                    unpacker, KeyModMap, self.totalModMapKeys
                )
            if self.present & MapPart.VirtualModMap:
                self.vmodmap_rtrn = xcffib.List(
                    unpacker, KeyVModMap, self.totalVModMapKeys
                )
        if self.reported == GBNDetail.CompatMap:
            (
                self.compatmap_type,
                self.compatDeviceID,
                self.compatmap_sequence,
                self.compatmap_length,
                self.groupsRtrn,
                self.firstSIRtrn,
                self.nSIRtrn,
                self.nTotalSI,
            ) = unpacker.unpack("BBHIBxHHH16x")
            self.si_rtrn = xcffib.List(unpacker, SymInterpret, self.nSIRtrn)
            unpacker.pad(ModDef)
            self.group_rtrn = xcffib.List(
                unpacker, ModDef, xcffib.popcount(self.groupsRtrn)
            )
        if self.reported == GBNDetail.IndicatorMaps:
            (
                self.indicatormap_type,
                self.indicatorDeviceID,
                self.indicatormap_sequence,
                self.indicatormap_length,
                self.which,
                self.realIndicators,
                self.nIndicators,
            ) = unpacker.unpack("BBHIIIB15x")
            self.maps = xcffib.List(unpacker, IndicatorMap, self.nIndicators)
        if self.reported == GBNDetail.KeyNames:
            (
                self.keyname_type,
                self.keyDeviceID,
                self.keyname_sequence,
                self.keyname_length,
                self.which,
                self.keyMinKeyCode,
                self.keyMaxKeyCode,
                self.nTypes,
                self.groupNames,
                self.virtualMods,
                self.firstKey,
                self.nKeys,
                self.indicators,
                self.nRadioGroups,
                self.nKeyAliases,
                self.nKTLevels,
            ) = unpacker.unpack("BBHIIBBBBHBBIBBH4x")
            if self.which & NameDetail.Keycodes:
                (self.keycodesName,) = unpacker.unpack("I")
            if self.which & NameDetail.Geometry:
                (self.geometryName,) = unpacker.unpack("I")
            if self.which & NameDetail.Symbols:
                (self.symbolsName,) = unpacker.unpack("I")
            if self.which & NameDetail.PhysSymbols:
                (self.physSymbolsName,) = unpacker.unpack("I")
            if self.which & NameDetail.Types:
                (self.typesName,) = unpacker.unpack("I")
            if self.which & NameDetail.Compat:
                (self.compatName,) = unpacker.unpack("I")
            if self.which & NameDetail.KeyTypeNames:
                self.typeNames = xcffib.List(unpacker, "I", self.nTypes)
            if self.which & NameDetail.KTLevelNames:
                self.nLevelsPerType = xcffib.List(unpacker, "B", self.nTypes)
                unpacker.pad("I")
                self.ktLevelNames = xcffib.List(unpacker, "I", sum(self.nLevelsPerType))
            if self.which & NameDetail.IndicatorNames:
                self.indicatorNames = xcffib.List(
                    unpacker, "I", xcffib.popcount(self.indicators)
                )
            if self.which & NameDetail.VirtualModNames:
                self.virtualModNames = xcffib.List(
                    unpacker, "I", xcffib.popcount(self.virtualMods)
                )
            if self.which & NameDetail.GroupNames:
                self.groups = xcffib.List(
                    unpacker, "I", xcffib.popcount(self.groupNames)
                )
            if self.which & NameDetail.KeyNames:
                self.keyNames = xcffib.List(unpacker, KeyName, self.nKeys)
            if self.which & NameDetail.KeyAliases:
                self.keyAliases = xcffib.List(unpacker, KeyAlias, self.nKeyAliases)
            if self.which & NameDetail.RGNames:
                self.radioGroupNames = xcffib.List(unpacker, "I", self.nRadioGroups)
        if self.reported == GBNDetail.Geometry:
            (
                self.geometry_type,
                self.geometryDeviceID,
                self.geometry_sequence,
                self.geometry_length,
                self.name,
                self.geometryFound,
                self.widthMM,
                self.heightMM,
                self.nProperties,
                self.nColors,
                self.nShapes,
                self.nSections,
                self.nDoodads,
                self.nKeyAliases,
                self.baseColorNdx,
                self.labelColorNdx,
            ) = unpacker.unpack("BBHIIBxHHHHHHHHBB")
            self.labelFont = CountedString16(unpacker)
        self.bufsize = unpacker.offset - base


class GetKbdByNameCookie(xcffib.Cookie):
    reply_type = GetKbdByNameReply


class GetDeviceInfoReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.deviceID,
            self.present,
            self.supported,
            self.unsupported,
            self.nDeviceLedFBs,
            self.firstBtnWanted,
            self.nBtnsWanted,
            self.firstBtnRtrn,
            self.nBtnsRtrn,
            self.totalBtns,
            self.hasOwnState,
            self.dfltKbdFB,
            self.dfltLedFB,
            self.devType,
            self.nameLen,
        ) = unpacker.unpack("xB2x4xHHHHBBBBBBHH2xIH")
        self.name = xcffib.List(unpacker, "c", self.nameLen)
        unpacker.pad(Action)
        self.btnActions = xcffib.List(unpacker, Action, self.nBtnsRtrn)
        unpacker.pad(DeviceLedInfo)
        self.leds = xcffib.List(unpacker, DeviceLedInfo, self.nDeviceLedFBs)
        self.bufsize = unpacker.offset - base


class GetDeviceInfoCookie(xcffib.Cookie):
    reply_type = GetDeviceInfoReply


class SetDebuggingFlagsReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.currentFlags,
            self.currentCtrls,
            self.supportedFlags,
            self.supportedCtrls,
        ) = unpacker.unpack("xx2x4xIIII8x")
        self.bufsize = unpacker.offset - base


class SetDebuggingFlagsCookie(xcffib.Cookie):
    reply_type = SetDebuggingFlagsReply


class NewKeyboardNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.oldDeviceID,
            self.minKeyCode,
            self.maxKeyCode,
            self.oldMinKeyCode,
            self.oldMaxKeyCode,
            self.requestMajor,
            self.requestMinor,
            self.changed,
        ) = unpacker.unpack("xB2xIBBBBBBBBH14x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 0))
        buf.write(
            struct.pack(
                "=B2xIBBBBBBBBH14x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.oldDeviceID,
                self.minKeyCode,
                self.maxKeyCode,
                self.oldMinKeyCode,
                self.oldMaxKeyCode,
                self.requestMajor,
                self.requestMinor,
                self.changed,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        oldDeviceID,
        minKeyCode,
        maxKeyCode,
        oldMinKeyCode,
        oldMaxKeyCode,
        requestMajor,
        requestMinor,
        changed,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.oldDeviceID = oldDeviceID
        self.minKeyCode = minKeyCode
        self.maxKeyCode = maxKeyCode
        self.oldMinKeyCode = oldMinKeyCode
        self.oldMaxKeyCode = oldMaxKeyCode
        self.requestMajor = requestMajor
        self.requestMinor = requestMinor
        self.changed = changed
        return self


_events[0] = NewKeyboardNotifyEvent


class MapNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.ptrBtnActions,
            self.changed,
            self.minKeyCode,
            self.maxKeyCode,
            self.firstType,
            self.nTypes,
            self.firstKeySym,
            self.nKeySyms,
            self.firstKeyAct,
            self.nKeyActs,
            self.firstKeyBehavior,
            self.nKeyBehavior,
            self.firstKeyExplicit,
            self.nKeyExplicit,
            self.firstModMapKey,
            self.nModMapKeys,
            self.firstVModMapKey,
            self.nVModMapKeys,
            self.virtualMods,
        ) = unpacker.unpack("xB2xIBBHBBBBBBBBBBBBBBBBH2x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 1))
        buf.write(
            struct.pack(
                "=B2xIBBHBBBBBBBBBBBBBBBBH2x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.ptrBtnActions,
                self.changed,
                self.minKeyCode,
                self.maxKeyCode,
                self.firstType,
                self.nTypes,
                self.firstKeySym,
                self.nKeySyms,
                self.firstKeyAct,
                self.nKeyActs,
                self.firstKeyBehavior,
                self.nKeyBehavior,
                self.firstKeyExplicit,
                self.nKeyExplicit,
                self.firstModMapKey,
                self.nModMapKeys,
                self.firstVModMapKey,
                self.nVModMapKeys,
                self.virtualMods,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        ptrBtnActions,
        changed,
        minKeyCode,
        maxKeyCode,
        firstType,
        nTypes,
        firstKeySym,
        nKeySyms,
        firstKeyAct,
        nKeyActs,
        firstKeyBehavior,
        nKeyBehavior,
        firstKeyExplicit,
        nKeyExplicit,
        firstModMapKey,
        nModMapKeys,
        firstVModMapKey,
        nVModMapKeys,
        virtualMods,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.ptrBtnActions = ptrBtnActions
        self.changed = changed
        self.minKeyCode = minKeyCode
        self.maxKeyCode = maxKeyCode
        self.firstType = firstType
        self.nTypes = nTypes
        self.firstKeySym = firstKeySym
        self.nKeySyms = nKeySyms
        self.firstKeyAct = firstKeyAct
        self.nKeyActs = nKeyActs
        self.firstKeyBehavior = firstKeyBehavior
        self.nKeyBehavior = nKeyBehavior
        self.firstKeyExplicit = firstKeyExplicit
        self.nKeyExplicit = nKeyExplicit
        self.firstModMapKey = firstModMapKey
        self.nModMapKeys = nModMapKeys
        self.firstVModMapKey = firstVModMapKey
        self.nVModMapKeys = nVModMapKeys
        self.virtualMods = virtualMods
        return self


_events[1] = MapNotifyEvent


class StateNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.mods,
            self.baseMods,
            self.latchedMods,
            self.lockedMods,
            self.group,
            self.baseGroup,
            self.latchedGroup,
            self.lockedGroup,
            self.compatState,
            self.grabMods,
            self.compatGrabMods,
            self.lookupMods,
            self.compatLoockupMods,
            self.ptrBtnState,
            self.changed,
            self.keycode,
            self.eventType,
            self.requestMajor,
            self.requestMinor,
        ) = unpacker.unpack("xB2xIBBBBBBhhBBBBBBHHBBBB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 2))
        buf.write(
            struct.pack(
                "=B2xIBBBBBBhhBBBBBBHHBBBB",
                self.xkbType,
                self.time,
                self.deviceID,
                self.mods,
                self.baseMods,
                self.latchedMods,
                self.lockedMods,
                self.group,
                self.baseGroup,
                self.latchedGroup,
                self.lockedGroup,
                self.compatState,
                self.grabMods,
                self.compatGrabMods,
                self.lookupMods,
                self.compatLoockupMods,
                self.ptrBtnState,
                self.changed,
                self.keycode,
                self.eventType,
                self.requestMajor,
                self.requestMinor,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        mods,
        baseMods,
        latchedMods,
        lockedMods,
        group,
        baseGroup,
        latchedGroup,
        lockedGroup,
        compatState,
        grabMods,
        compatGrabMods,
        lookupMods,
        compatLoockupMods,
        ptrBtnState,
        changed,
        keycode,
        eventType,
        requestMajor,
        requestMinor,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.mods = mods
        self.baseMods = baseMods
        self.latchedMods = latchedMods
        self.lockedMods = lockedMods
        self.group = group
        self.baseGroup = baseGroup
        self.latchedGroup = latchedGroup
        self.lockedGroup = lockedGroup
        self.compatState = compatState
        self.grabMods = grabMods
        self.compatGrabMods = compatGrabMods
        self.lookupMods = lookupMods
        self.compatLoockupMods = compatLoockupMods
        self.ptrBtnState = ptrBtnState
        self.changed = changed
        self.keycode = keycode
        self.eventType = eventType
        self.requestMajor = requestMajor
        self.requestMinor = requestMinor
        return self


_events[2] = StateNotifyEvent


class ControlsNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.numGroups,
            self.changedControls,
            self.enabledControls,
            self.enabledControlChanges,
            self.keycode,
            self.eventType,
            self.requestMajor,
            self.requestMinor,
        ) = unpacker.unpack("xB2xIBB2xIIIBBBB4x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 3))
        buf.write(
            struct.pack(
                "=B2xIBB2xIIIBBBB4x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.numGroups,
                self.changedControls,
                self.enabledControls,
                self.enabledControlChanges,
                self.keycode,
                self.eventType,
                self.requestMajor,
                self.requestMinor,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        numGroups,
        changedControls,
        enabledControls,
        enabledControlChanges,
        keycode,
        eventType,
        requestMajor,
        requestMinor,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.numGroups = numGroups
        self.changedControls = changedControls
        self.enabledControls = enabledControls
        self.enabledControlChanges = enabledControlChanges
        self.keycode = keycode
        self.eventType = eventType
        self.requestMajor = requestMajor
        self.requestMinor = requestMinor
        return self


_events[3] = ControlsNotifyEvent


class IndicatorStateNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        self.xkbType, self.time, self.deviceID, self.state, self.stateChanged = (
            unpacker.unpack("xB2xIB3xII12x")
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 4))
        buf.write(
            struct.pack(
                "=B2xIB3xII12x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.state,
                self.stateChanged,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, xkbType, time, deviceID, state, stateChanged):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.state = state
        self.stateChanged = stateChanged
        return self


_events[4] = IndicatorStateNotifyEvent


class IndicatorMapNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        self.xkbType, self.time, self.deviceID, self.state, self.mapChanged = (
            unpacker.unpack("xB2xIB3xII12x")
        )
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 5))
        buf.write(
            struct.pack(
                "=B2xIB3xII12x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.state,
                self.mapChanged,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, xkbType, time, deviceID, state, mapChanged):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.state = state
        self.mapChanged = mapChanged
        return self


_events[5] = IndicatorMapNotifyEvent


class NamesNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.changed,
            self.firstType,
            self.nTypes,
            self.firstLevelName,
            self.nLevelNames,
            self.nRadioGroups,
            self.nKeyAliases,
            self.changedGroupNames,
            self.changedVirtualMods,
            self.firstKey,
            self.nKeys,
            self.changedIndicators,
        ) = unpacker.unpack("xB2xIBxHBBBBxBBBHBBI4x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 6))
        buf.write(
            struct.pack(
                "=B2xIBxHBBBBxBBBHBBI4x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.changed,
                self.firstType,
                self.nTypes,
                self.firstLevelName,
                self.nLevelNames,
                self.nRadioGroups,
                self.nKeyAliases,
                self.changedGroupNames,
                self.changedVirtualMods,
                self.firstKey,
                self.nKeys,
                self.changedIndicators,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        changed,
        firstType,
        nTypes,
        firstLevelName,
        nLevelNames,
        nRadioGroups,
        nKeyAliases,
        changedGroupNames,
        changedVirtualMods,
        firstKey,
        nKeys,
        changedIndicators,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.changed = changed
        self.firstType = firstType
        self.nTypes = nTypes
        self.firstLevelName = firstLevelName
        self.nLevelNames = nLevelNames
        self.nRadioGroups = nRadioGroups
        self.nKeyAliases = nKeyAliases
        self.changedGroupNames = changedGroupNames
        self.changedVirtualMods = changedVirtualMods
        self.firstKey = firstKey
        self.nKeys = nKeys
        self.changedIndicators = changedIndicators
        return self


_events[6] = NamesNotifyEvent


class CompatMapNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.changedGroups,
            self.firstSI,
            self.nSI,
            self.nTotalSI,
        ) = unpacker.unpack("xB2xIBBHHH16x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 7))
        buf.write(
            struct.pack(
                "=B2xIBBHHH16x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.changedGroups,
                self.firstSI,
                self.nSI,
                self.nTotalSI,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, xkbType, time, deviceID, changedGroups, firstSI, nSI, nTotalSI):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.changedGroups = changedGroups
        self.firstSI = firstSI
        self.nSI = nSI
        self.nTotalSI = nTotalSI
        return self


_events[7] = CompatMapNotifyEvent


class BellNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.bellClass,
            self.bellID,
            self.percent,
            self.pitch,
            self.duration,
            self.name,
            self.window,
            self.eventOnly,
        ) = unpacker.unpack("xB2xIBBBBHHIIB7x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 8))
        buf.write(
            struct.pack(
                "=B2xIBBBBHHIIB7x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.bellClass,
                self.bellID,
                self.percent,
                self.pitch,
                self.duration,
                self.name,
                self.window,
                self.eventOnly,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        bellClass,
        bellID,
        percent,
        pitch,
        duration,
        name,
        window,
        eventOnly,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.bellClass = bellClass
        self.bellID = bellID
        self.percent = percent
        self.pitch = pitch
        self.duration = duration
        self.name = name
        self.window = window
        self.eventOnly = eventOnly
        return self


_events[8] = BellNotifyEvent


class ActionMessageEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.keycode,
            self.press,
            self.keyEventFollows,
            self.mods,
            self.group,
        ) = unpacker.unpack("xB2xIBBBBBB")
        self.message = xcffib.List(unpacker, "c", 8)
        unpacker.unpack("10x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 9))
        buf.write(
            struct.pack(
                "=B2xIBBBBBB",
                self.xkbType,
                self.time,
                self.deviceID,
                self.keycode,
                self.press,
                self.keyEventFollows,
                self.mods,
                self.group,
            )
        )
        buf.write(xcffib.pack_list(self.message, "c"))
        buf.write(
            struct.pack(
                "=10x",
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        keycode,
        press,
        keyEventFollows,
        mods,
        group,
        message,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.keycode = keycode
        self.press = press
        self.keyEventFollows = keyEventFollows
        self.mods = mods
        self.group = group
        self.message = message
        return self


_events[9] = ActionMessageEvent


class AccessXNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.keycode,
            self.detailt,
            self.slowKeysDelay,
            self.debounceDelay,
        ) = unpacker.unpack("xB2xIBBHHH16x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 10))
        buf.write(
            struct.pack(
                "=B2xIBBHHH16x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.keycode,
                self.detailt,
                self.slowKeysDelay,
                self.debounceDelay,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls, xkbType, time, deviceID, keycode, detailt, slowKeysDelay, debounceDelay
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.keycode = keycode
        self.detailt = detailt
        self.slowKeysDelay = slowKeysDelay
        self.debounceDelay = debounceDelay
        return self


_events[10] = AccessXNotifyEvent


class ExtensionDeviceNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.xkbType,
            self.time,
            self.deviceID,
            self.reason,
            self.ledClass,
            self.ledID,
            self.ledsDefined,
            self.ledState,
            self.firstButton,
            self.nButtons,
            self.supported,
            self.unsupported,
        ) = unpacker.unpack("xB2xIBxHHHIIBBHH2x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 11))
        buf.write(
            struct.pack(
                "=B2xIBxHHHIIBBHH2x",
                self.xkbType,
                self.time,
                self.deviceID,
                self.reason,
                self.ledClass,
                self.ledID,
                self.ledsDefined,
                self.ledState,
                self.firstButton,
                self.nButtons,
                self.supported,
                self.unsupported,
            )
        )
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(
        cls,
        xkbType,
        time,
        deviceID,
        reason,
        ledClass,
        ledID,
        ledsDefined,
        ledState,
        firstButton,
        nButtons,
        supported,
        unsupported,
    ):
        self = cls.__new__(cls)
        self.xkbType = xkbType
        self.time = time
        self.deviceID = deviceID
        self.reason = reason
        self.ledClass = ledClass
        self.ledID = ledID
        self.ledsDefined = ledsDefined
        self.ledState = ledState
        self.firstButton = firstButton
        self.nButtons = nButtons
        self.supported = supported
        self.unsupported = unsupported
        return self


_events[11] = ExtensionDeviceNotifyEvent


class xkbExtension(xcffib.Extension):
    def UseExtension(self, wantedMajor, wantedMinor, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xHH", wantedMajor, wantedMinor))
        return self.send_request(0, buf, UseExtensionCookie, is_checked=is_checked)

    def SelectEvents(
        self,
        deviceSpec,
        affectWhich,
        clear,
        selectAll,
        affectMap,
        map,
        details,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHHHHH", deviceSpec, affectWhich, clear, selectAll, affectMap, map
            )
        )
        if affectWhich & ((~clear) & (~selectAll)) & EventType.NewKeyboardNotify:
            affectNewKeyboard = details.pop(0)
            newKeyboardDetails = details.pop(0)
            buf.write(struct.pack("=HH", affectNewKeyboard, newKeyboardDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.StateNotify:
            affectState = details.pop(0)
            stateDetails = details.pop(0)
            buf.write(struct.pack("=HH", affectState, stateDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.ControlsNotify:
            affectCtrls = details.pop(0)
            ctrlDetails = details.pop(0)
            buf.write(struct.pack("=II", affectCtrls, ctrlDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.IndicatorStateNotify:
            affectIndicatorState = details.pop(0)
            indicatorStateDetails = details.pop(0)
            buf.write(struct.pack("=II", affectIndicatorState, indicatorStateDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.IndicatorMapNotify:
            affectIndicatorMap = details.pop(0)
            indicatorMapDetails = details.pop(0)
            buf.write(struct.pack("=II", affectIndicatorMap, indicatorMapDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.NamesNotify:
            affectNames = details.pop(0)
            namesDetails = details.pop(0)
            buf.write(struct.pack("=HH", affectNames, namesDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.CompatMapNotify:
            affectCompat = details.pop(0)
            compatDetails = details.pop(0)
            buf.write(struct.pack("=BB", affectCompat, compatDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.BellNotify:
            affectBell = details.pop(0)
            bellDetails = details.pop(0)
            buf.write(struct.pack("=BB", affectBell, bellDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.ActionMessage:
            affectMsgDetails = details.pop(0)
            msgDetails = details.pop(0)
            buf.write(struct.pack("=BB", affectMsgDetails, msgDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.AccessXNotify:
            affectAccessX = details.pop(0)
            accessXDetails = details.pop(0)
            buf.write(struct.pack("=HH", affectAccessX, accessXDetails))
        if affectWhich & ((~clear) & (~selectAll)) & EventType.ExtensionDeviceNotify:
            affectExtDev = details.pop(0)
            extdevDetails = details.pop(0)
            buf.write(struct.pack("=HH", affectExtDev, extdevDetails))
        return self.send_request(1, buf, is_checked=is_checked)

    def Bell(
        self,
        deviceSpec,
        bellClass,
        bellID,
        percent,
        forceSound,
        eventOnly,
        pitch,
        duration,
        name,
        window,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHHbBBxhh2xII",
                deviceSpec,
                bellClass,
                bellID,
                percent,
                forceSound,
                eventOnly,
                pitch,
                duration,
                name,
                window,
            )
        )
        return self.send_request(3, buf, is_checked=is_checked)

    def GetState(self, deviceSpec, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xH2x", deviceSpec))
        return self.send_request(4, buf, GetStateCookie, is_checked=is_checked)

    def LatchLockState(
        self,
        deviceSpec,
        affectModLocks,
        modLocks,
        lockGroup,
        groupLock,
        affectModLatches,
        latchGroup,
        groupLatch,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHBBBBBxxBH",
                deviceSpec,
                affectModLocks,
                modLocks,
                lockGroup,
                groupLock,
                affectModLatches,
                latchGroup,
                groupLatch,
            )
        )
        return self.send_request(5, buf, is_checked=is_checked)

    def GetControls(self, deviceSpec, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xH2x", deviceSpec))
        return self.send_request(6, buf, GetControlsCookie, is_checked=is_checked)

    def SetControls(
        self,
        deviceSpec,
        affectInternalRealMods,
        internalRealMods,
        affectIgnoreLockRealMods,
        ignoreLockRealMods,
        affectInternalVirtualMods,
        internalVirtualMods,
        affectIgnoreLockVirtualMods,
        ignoreLockVirtualMods,
        mouseKeysDfltBtn,
        groupsWrap,
        accessXOptions,
        affectEnabledControls,
        enabledControls,
        changeControls,
        repeatDelay,
        repeatInterval,
        slowKeysDelay,
        debounceDelay,
        mouseKeysDelay,
        mouseKeysInterval,
        mouseKeysTimeToMax,
        mouseKeysMaxSpeed,
        mouseKeysCurve,
        accessXTimeout,
        accessXTimeoutMask,
        accessXTimeoutValues,
        accessXTimeoutOptionsMask,
        accessXTimeoutOptionsValues,
        perKeyRepeat,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHBBBBHHHHBBH2xIIIHHHHHHHHhHIIHH",
                deviceSpec,
                affectInternalRealMods,
                internalRealMods,
                affectIgnoreLockRealMods,
                ignoreLockRealMods,
                affectInternalVirtualMods,
                internalVirtualMods,
                affectIgnoreLockVirtualMods,
                ignoreLockVirtualMods,
                mouseKeysDfltBtn,
                groupsWrap,
                accessXOptions,
                affectEnabledControls,
                enabledControls,
                changeControls,
                repeatDelay,
                repeatInterval,
                slowKeysDelay,
                debounceDelay,
                mouseKeysDelay,
                mouseKeysInterval,
                mouseKeysTimeToMax,
                mouseKeysMaxSpeed,
                mouseKeysCurve,
                accessXTimeout,
                accessXTimeoutMask,
                accessXTimeoutValues,
                accessXTimeoutOptionsMask,
                accessXTimeoutOptionsValues,
            )
        )
        buf.write(xcffib.pack_list(perKeyRepeat, "B"))
        return self.send_request(7, buf, is_checked=is_checked)

    def GetMap(
        self,
        deviceSpec,
        full,
        partial,
        firstType,
        nTypes,
        firstKeySym,
        nKeySyms,
        firstKeyAction,
        nKeyActions,
        firstKeyBehavior,
        nKeyBehaviors,
        virtualMods,
        firstKeyExplicit,
        nKeyExplicit,
        firstModMapKey,
        nModMapKeys,
        firstVModMapKey,
        nVModMapKeys,
        is_checked=True,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHHBBBBBBBBHBBBBBB2x",
                deviceSpec,
                full,
                partial,
                firstType,
                nTypes,
                firstKeySym,
                nKeySyms,
                firstKeyAction,
                nKeyActions,
                firstKeyBehavior,
                nKeyBehaviors,
                virtualMods,
                firstKeyExplicit,
                nKeyExplicit,
                firstModMapKey,
                nModMapKeys,
                firstVModMapKey,
                nVModMapKeys,
            )
        )
        return self.send_request(8, buf, GetMapCookie, is_checked=is_checked)

    def SetMap(
        self,
        deviceSpec,
        present,
        flags,
        minKeyCode,
        maxKeyCode,
        firstType,
        nTypes,
        firstKeySym,
        nKeySyms,
        totalSyms,
        firstKeyAction,
        nKeyActions,
        totalActions,
        firstKeyBehavior,
        nKeyBehaviors,
        totalKeyBehaviors,
        firstKeyExplicit,
        nKeyExplicit,
        totalKeyExplicit,
        firstModMapKey,
        nModMapKeys,
        totalModMapKeys,
        firstVModMapKey,
        nVModMapKeys,
        totalVModMapKeys,
        virtualMods,
        values,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHHBBBBBBHBBHBBBBBBBBBBBBH",
                deviceSpec,
                present,
                flags,
                minKeyCode,
                maxKeyCode,
                firstType,
                nTypes,
                firstKeySym,
                nKeySyms,
                totalSyms,
                firstKeyAction,
                nKeyActions,
                totalActions,
                firstKeyBehavior,
                nKeyBehaviors,
                totalKeyBehaviors,
                firstKeyExplicit,
                nKeyExplicit,
                totalKeyExplicit,
                firstModMapKey,
                nModMapKeys,
                totalModMapKeys,
                firstVModMapKey,
                nVModMapKeys,
                totalVModMapKeys,
                virtualMods,
            )
        )
        if present & MapPart.KeyTypes:
            types = values.pop(0)
            buf.write(xcffib.pack_list(types, SetKeyType))
        if present & MapPart.KeySyms:
            syms = values.pop(0)
            buf.write(xcffib.pack_list(syms, KeySymMap))
        if present & MapPart.KeyActions:
            actionsCount = values.pop(0)
            values.pop(0)
            actions = values.pop(0)
            buf.write(xcffib.pack_list(actionsCount, "B"))
            buf.write(
                struct.pack(
                    "=4x",
                )
            )
            buf.write(xcffib.pack_list(actions, Action))
        if present & MapPart.KeyBehaviors:
            behaviors = values.pop(0)
            buf.write(xcffib.pack_list(behaviors, SetBehavior))
        if present & MapPart.VirtualMods:
            vmods = values.pop(0)
            values.pop(0)
            buf.write(xcffib.pack_list(vmods, "B"))
            buf.write(
                struct.pack(
                    "=4x",
                )
            )
        if present & MapPart.ExplicitComponents:
            explicit = values.pop(0)
            buf.write(xcffib.pack_list(explicit, SetExplicit))
        if present & MapPart.ModifierMap:
            modmap = values.pop(0)
            buf.write(xcffib.pack_list(modmap, KeyModMap))
        if present & MapPart.VirtualModMap:
            vmodmap = values.pop(0)
            buf.write(xcffib.pack_list(vmodmap, KeyVModMap))
        return self.send_request(9, buf, is_checked=is_checked)

    def GetCompatMap(self, deviceSpec, groups, getAllSI, firstSI, nSI, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xHBBHH", deviceSpec, groups, getAllSI, firstSI, nSI))
        return self.send_request(10, buf, GetCompatMapCookie, is_checked=is_checked)

    def SetCompatMap(
        self,
        deviceSpec,
        recomputeActions,
        truncateSI,
        groups,
        firstSI,
        nSI,
        si,
        groupMaps,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHxBBBHH2x",
                deviceSpec,
                recomputeActions,
                truncateSI,
                groups,
                firstSI,
                nSI,
            )
        )
        buf.write(xcffib.pack_list(si, SymInterpret))
        buf.write(xcffib.pack_list(groupMaps, ModDef))
        return self.send_request(11, buf, is_checked=is_checked)

    def GetIndicatorState(self, deviceSpec, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xH2x", deviceSpec))
        return self.send_request(
            12, buf, GetIndicatorStateCookie, is_checked=is_checked
        )

    def GetIndicatorMap(self, deviceSpec, which, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xH2xI", deviceSpec, which))
        return self.send_request(13, buf, GetIndicatorMapCookie, is_checked=is_checked)

    def SetIndicatorMap(self, deviceSpec, which, maps, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xH2xI", deviceSpec, which))
        buf.write(xcffib.pack_list(maps, IndicatorMap))
        return self.send_request(14, buf, is_checked=is_checked)

    def GetNamedIndicator(
        self, deviceSpec, ledClass, ledID, indicator, is_checked=True
    ):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xHHH2xI", deviceSpec, ledClass, ledID, indicator))
        return self.send_request(
            15, buf, GetNamedIndicatorCookie, is_checked=is_checked
        )

    def SetNamedIndicator(
        self,
        deviceSpec,
        ledClass,
        ledID,
        indicator,
        setState,
        on,
        setMap,
        createMap,
        map_flags,
        map_whichGroups,
        map_groups,
        map_whichMods,
        map_realMods,
        map_vmods,
        map_ctrls,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHH2xIBBBBxBBBBBHI",
                deviceSpec,
                ledClass,
                ledID,
                indicator,
                setState,
                on,
                setMap,
                createMap,
                map_flags,
                map_whichGroups,
                map_groups,
                map_whichMods,
                map_realMods,
                map_vmods,
                map_ctrls,
            )
        )
        return self.send_request(16, buf, is_checked=is_checked)

    def GetNames(self, deviceSpec, which, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xH2xI", deviceSpec, which))
        return self.send_request(17, buf, GetNamesCookie, is_checked=is_checked)

    def SetNames(
        self,
        deviceSpec,
        virtualMods,
        which,
        firstType,
        nTypes,
        firstKTLevelt,
        nKTLevels,
        indicators,
        groupNames,
        nRadioGroups,
        firstKey,
        nKeys,
        nKeyAliases,
        totalKTLevelNames,
        values,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHIBBBBIBBBBBxH",
                deviceSpec,
                virtualMods,
                which,
                firstType,
                nTypes,
                firstKTLevelt,
                nKTLevels,
                indicators,
                groupNames,
                nRadioGroups,
                firstKey,
                nKeys,
                nKeyAliases,
                totalKTLevelNames,
            )
        )
        if which & NameDetail.Keycodes:
            keycodesName = values.pop(0)
            buf.write(struct.pack("=I", keycodesName))
        if which & NameDetail.Geometry:
            geometryName = values.pop(0)
            buf.write(struct.pack("=I", geometryName))
        if which & NameDetail.Symbols:
            symbolsName = values.pop(0)
            buf.write(struct.pack("=I", symbolsName))
        if which & NameDetail.PhysSymbols:
            physSymbolsName = values.pop(0)
            buf.write(struct.pack("=I", physSymbolsName))
        if which & NameDetail.Types:
            typesName = values.pop(0)
            buf.write(struct.pack("=I", typesName))
        if which & NameDetail.Compat:
            compatName = values.pop(0)
            buf.write(struct.pack("=I", compatName))
        if which & NameDetail.KeyTypeNames:
            typeNames = values.pop(0)
            buf.write(xcffib.pack_list(typeNames, "I"))
        if which & NameDetail.KTLevelNames:
            nLevelsPerType = values.pop(0)
            values.pop(0)
            ktLevelNames = values.pop(0)
            buf.write(xcffib.pack_list(nLevelsPerType, "B"))
            buf.write(
                struct.pack(
                    "=4x",
                )
            )
            buf.write(xcffib.pack_list(ktLevelNames, "I"))
        if which & NameDetail.IndicatorNames:
            indicatorNames = values.pop(0)
            buf.write(xcffib.pack_list(indicatorNames, "I"))
        if which & NameDetail.VirtualModNames:
            virtualModNames = values.pop(0)
            buf.write(xcffib.pack_list(virtualModNames, "I"))
        if which & NameDetail.GroupNames:
            groups = values.pop(0)
            buf.write(xcffib.pack_list(groups, "I"))
        if which & NameDetail.KeyNames:
            keyNames = values.pop(0)
            buf.write(xcffib.pack_list(keyNames, KeyName))
        if which & NameDetail.KeyAliases:
            keyAliases = values.pop(0)
            buf.write(xcffib.pack_list(keyAliases, KeyAlias))
        if which & NameDetail.RGNames:
            radioGroupNames = values.pop(0)
            buf.write(xcffib.pack_list(radioGroupNames, "I"))
        return self.send_request(18, buf, is_checked=is_checked)

    def PerClientFlags(
        self,
        deviceSpec,
        change,
        value,
        ctrlsToChange,
        autoCtrls,
        autoCtrlsValues,
        is_checked=True,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xH2xIIIII",
                deviceSpec,
                change,
                value,
                ctrlsToChange,
                autoCtrls,
                autoCtrlsValues,
            )
        )
        return self.send_request(21, buf, PerClientFlagsCookie, is_checked=is_checked)

    def ListComponents(self, deviceSpec, maxNames, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xHH", deviceSpec, maxNames))
        return self.send_request(22, buf, ListComponentsCookie, is_checked=is_checked)

    def GetKbdByName(self, deviceSpec, need, want, load, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xHHHBx", deviceSpec, need, want, load))
        return self.send_request(23, buf, GetKbdByNameCookie, is_checked=is_checked)

    def GetDeviceInfo(
        self,
        deviceSpec,
        wanted,
        allButtons,
        firstButton,
        nButtons,
        ledClass,
        ledID,
        is_checked=True,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHHBBBxHH",
                deviceSpec,
                wanted,
                allButtons,
                firstButton,
                nButtons,
                ledClass,
                ledID,
            )
        )
        return self.send_request(24, buf, GetDeviceInfoCookie, is_checked=is_checked)

    def SetDeviceInfo(
        self,
        deviceSpec,
        firstBtn,
        nBtns,
        change,
        nDeviceLedFBs,
        btnActions,
        leds,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xHBBHH", deviceSpec, firstBtn, nBtns, change, nDeviceLedFBs
            )
        )
        buf.write(xcffib.pack_list(btnActions, Action))
        buf.write(xcffib.pack_list(leds, DeviceLedInfo))
        return self.send_request(25, buf, is_checked=is_checked)

    def SetDebuggingFlags(
        self,
        msgLength,
        affectFlags,
        flags,
        affectCtrls,
        ctrls,
        message,
        is_checked=True,
    ):
        buf = io.BytesIO()
        buf.write(
            struct.pack(
                "=xx2xH2xIIII", msgLength, affectFlags, flags, affectCtrls, ctrls
            )
        )
        buf.write(xcffib.pack_list(message, "c"))
        return self.send_request(
            101, buf, SetDebuggingFlagsCookie, is_checked=is_checked
        )


xcffib._add_ext(key, xkbExtension, _events, _errors)
