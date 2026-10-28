import xcffib
import struct
import io

MAJOR_VERSION = 1
MINOR_VERSION = 0
key = xcffib.ExtensionKey("XpExtension")
_events = {}
_errors = {}
from . import xproto


class PRINTER(xcffib.Struct):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Struct.__init__(self, unpacker)
        base = unpacker.offset
        (self.nameLen,) = unpacker.unpack("I")
        self.name = xcffib.List(unpacker, "c", self.nameLen)
        (self.descLen,) = unpacker.unpack("I")
        unpacker.pad("c")
        self.description = xcffib.List(unpacker, "c", self.descLen)
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=I", self.nameLen))
        buf.write(xcffib.pack_list(self.name, "c"))
        buf.write(
            struct.pack(
                "=4x",
            )
        )
        buf.write(struct.pack("=I", self.descLen))
        buf.write(xcffib.pack_list(self.description, "c"))
        buf.write(
            struct.pack(
                "=4x",
            )
        )
        return buf.getvalue()

    @classmethod
    def synthetic(cls, nameLen, name, descLen, description):
        self = cls.__new__(cls)
        self.nameLen = nameLen
        self.name = name
        self.descLen = descLen
        self.description = description
        return self


class GetDoc:
    Finished = 0
    SecondConsumer = 1


class EvMask:
    NoEventMask = 0
    PrintMask = 1 << 0
    AttributeMask = 1 << 1


class Detail:
    StartJobNotify = 1
    EndJobNotify = 2
    StartDocNotify = 3
    EndDocNotify = 4
    StartPageNotify = 5
    EndPageNotify = 6


class Attr:
    JobAttr = 1
    DocAttr = 2
    PageAttr = 3
    PrinterAttr = 4
    ServerAttr = 5
    MediumAttr = 6
    SpoolerAttr = 7


class PrintQueryVersionReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.major_version, self.minor_version = unpacker.unpack("xx2x4xHH")
        self.bufsize = unpacker.offset - base


class PrintQueryVersionCookie(xcffib.Cookie):
    reply_type = PrintQueryVersionReply


class PrintGetPrinterListReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.listCount,) = unpacker.unpack("xx2x4xI20x")
        self.printers = xcffib.List(unpacker, PRINTER, self.listCount)
        self.bufsize = unpacker.offset - base


class PrintGetPrinterListCookie(xcffib.Cookie):
    reply_type = PrintGetPrinterListReply


class PrintGetContextReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.context,) = unpacker.unpack("xx2x4xI")
        self.bufsize = unpacker.offset - base


class PrintGetContextCookie(xcffib.Cookie):
    reply_type = PrintGetContextReply


class PrintGetScreenOfContextReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.root,) = unpacker.unpack("xx2x4xI")
        self.bufsize = unpacker.offset - base


class PrintGetScreenOfContextCookie(xcffib.Cookie):
    reply_type = PrintGetScreenOfContextReply


class PrintGetDocumentDataReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.status_code, self.finished_flag, self.dataLen = unpacker.unpack(
            "xx2x4xIII12x"
        )
        self.data = xcffib.List(unpacker, "B", self.dataLen)
        self.bufsize = unpacker.offset - base


class PrintGetDocumentDataCookie(xcffib.Cookie):
    reply_type = PrintGetDocumentDataReply


class PrintInputSelectedReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.event_mask, self.all_events_mask = unpacker.unpack("xx2x4xII")
        self.bufsize = unpacker.offset - base


class PrintInputSelectedCookie(xcffib.Cookie):
    reply_type = PrintInputSelectedReply


class PrintGetAttributesReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.stringLen,) = unpacker.unpack("xx2x4xI20x")
        self.attributes = xcffib.List(unpacker, "c", self.stringLen)
        self.bufsize = unpacker.offset - base


class PrintGetAttributesCookie(xcffib.Cookie):
    reply_type = PrintGetAttributesReply


class PrintGetOneAttributesReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.valueLen,) = unpacker.unpack("xx2x4xI20x")
        self.value = xcffib.List(unpacker, "c", self.valueLen)
        self.bufsize = unpacker.offset - base


class PrintGetOneAttributesCookie(xcffib.Cookie):
    reply_type = PrintGetOneAttributesReply


class PrintGetPageDimensionsReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (
            self.width,
            self.height,
            self.offset_x,
            self.offset_y,
            self.reproducible_width,
            self.reproducible_height,
        ) = unpacker.unpack("xx2x4xHHHHHH")
        self.bufsize = unpacker.offset - base


class PrintGetPageDimensionsCookie(xcffib.Cookie):
    reply_type = PrintGetPageDimensionsReply


class PrintQueryScreensReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.listCount,) = unpacker.unpack("xx2x4xI20x")
        self.roots = xcffib.List(unpacker, "I", self.listCount)
        self.bufsize = unpacker.offset - base


class PrintQueryScreensCookie(xcffib.Cookie):
    reply_type = PrintQueryScreensReply


class PrintSetImageResolutionReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        self.status, self.previous_resolutions = unpacker.unpack("xB2x4xH")
        self.bufsize = unpacker.offset - base


class PrintSetImageResolutionCookie(xcffib.Cookie):
    reply_type = PrintSetImageResolutionReply


class PrintGetImageResolutionReply(xcffib.Reply):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Reply.__init__(self, unpacker)
        base = unpacker.offset
        (self.image_resolution,) = unpacker.unpack("xx2x4xH")
        self.bufsize = unpacker.offset - base


class PrintGetImageResolutionCookie(xcffib.Cookie):
    reply_type = PrintGetImageResolutionReply


class NotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        self.detail, self.context, self.cancel = unpacker.unpack("xB2xIB")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 0))
        buf.write(struct.pack("=B2xIB", self.detail, self.context, self.cancel))
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, detail, context, cancel):
        self = cls.__new__(cls)
        self.detail = detail
        self.context = context
        self.cancel = cancel
        return self


_events[0] = NotifyEvent


class AttributNotifyEvent(xcffib.Event):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Event.__init__(self, unpacker)
        base = unpacker.offset
        self.detail, self.context = unpacker.unpack("xB2xI")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 1))
        buf.write(struct.pack("=B2xI", self.detail, self.context))
        buf_len = len(buf.getvalue())
        if buf_len < 32:
            buf.write(struct.pack(("%dx" % (32 - buf_len))))
        return buf.getvalue()

    @classmethod
    def synthetic(cls, detail, context):
        self = cls.__new__(cls)
        self.detail = detail
        self.context = context
        return self


_events[1] = AttributNotifyEvent


class BadContextError(xcffib.Error):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Error.__init__(self, unpacker)
        base = unpacker.offset
        unpacker.unpack("xx2x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 0))
        buf.write(struct.pack("=x2x"))
        return buf.getvalue()


BadBadContext = BadContextError
_errors[0] = BadContextError


class BadSequenceError(xcffib.Error):
    xge = False

    def __init__(self, unpacker):
        if isinstance(unpacker, xcffib.Protobj):
            unpacker = xcffib.MemoryUnpacker(unpacker.pack())
        xcffib.Error.__init__(self, unpacker)
        base = unpacker.offset
        unpacker.unpack("xx2x")
        self.bufsize = unpacker.offset - base

    def pack(self):
        buf = io.BytesIO()
        buf.write(struct.pack("=B", 1))
        buf.write(struct.pack("=x2x"))
        return buf.getvalue()


BadBadSequence = BadSequenceError
_errors[1] = BadSequenceError


class xprintExtension(xcffib.Extension):
    def PrintQueryVersion(self, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2x"))
        return self.send_request(0, buf, PrintQueryVersionCookie, is_checked=is_checked)

    def PrintGetPrinterList(
        self, printerNameLen, localeLen, printer_name, locale, is_checked=True
    ):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xII", printerNameLen, localeLen))
        buf.write(xcffib.pack_list(printer_name, "c"))
        buf.write(
            struct.pack(
                "=4x",
            )
        )
        buf.write(xcffib.pack_list(locale, "c"))
        return self.send_request(
            1, buf, PrintGetPrinterListCookie, is_checked=is_checked
        )

    def PrintRehashPrinterList(self, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2x"))
        return self.send_request(20, buf, is_checked=is_checked)

    def CreateContext(
        self,
        context_id,
        printerNameLen,
        localeLen,
        printerName,
        locale,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xIII", context_id, printerNameLen, localeLen))
        buf.write(xcffib.pack_list(printerName, "c"))
        buf.write(
            struct.pack(
                "=4x",
            )
        )
        buf.write(xcffib.pack_list(locale, "c"))
        return self.send_request(2, buf, is_checked=is_checked)

    def PrintSetContext(self, context, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xI", context))
        return self.send_request(3, buf, is_checked=is_checked)

    def PrintGetContext(self, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2x"))
        return self.send_request(4, buf, PrintGetContextCookie, is_checked=is_checked)

    def PrintDestroyContext(self, context, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xI", context))
        return self.send_request(5, buf, is_checked=is_checked)

    def PrintGetScreenOfContext(self, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2x"))
        return self.send_request(
            6, buf, PrintGetScreenOfContextCookie, is_checked=is_checked
        )

    def PrintStartJob(self, output_mode, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xB", output_mode))
        return self.send_request(7, buf, is_checked=is_checked)

    def PrintEndJob(self, cancel, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xB", cancel))
        return self.send_request(8, buf, is_checked=is_checked)

    def PrintStartDoc(self, driver_mode, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xB", driver_mode))
        return self.send_request(9, buf, is_checked=is_checked)

    def PrintEndDoc(self, cancel, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xB", cancel))
        return self.send_request(10, buf, is_checked=is_checked)

    def PrintPutDocumentData(
        self,
        drawable,
        len_data,
        len_fmt,
        len_options,
        data,
        doc_format,
        options,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xIIHH", drawable, len_data, len_fmt, len_options))
        buf.write(xcffib.pack_list(data, "B"))
        buf.write(
            struct.pack(
                "=4x",
            )
        )
        buf.write(xcffib.pack_list(doc_format, "c"))
        buf.write(
            struct.pack(
                "=4x",
            )
        )
        buf.write(xcffib.pack_list(options, "c"))
        return self.send_request(11, buf, is_checked=is_checked)

    def PrintGetDocumentData(self, context, max_bytes, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xII", context, max_bytes))
        return self.send_request(
            12, buf, PrintGetDocumentDataCookie, is_checked=is_checked
        )

    def PrintStartPage(self, window, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xI", window))
        return self.send_request(13, buf, is_checked=is_checked)

    def PrintEndPage(self, cancel, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xB3x", cancel))
        return self.send_request(14, buf, is_checked=is_checked)

    def PrintSelectInput(self, context, event_mask, is_checked=False):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xII", context, event_mask))
        return self.send_request(15, buf, is_checked=is_checked)

    def PrintInputSelected(self, context, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xI", context))
        return self.send_request(
            16, buf, PrintInputSelectedCookie, is_checked=is_checked
        )

    def PrintGetAttributes(self, context, pool, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xIB3x", context, pool))
        return self.send_request(
            17, buf, PrintGetAttributesCookie, is_checked=is_checked
        )

    def PrintGetOneAttributes(self, context, nameLen, pool, name, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xIIB3x", context, nameLen, pool))
        buf.write(xcffib.pack_list(name, "c"))
        return self.send_request(
            19, buf, PrintGetOneAttributesCookie, is_checked=is_checked
        )

    def PrintSetAttributes(
        self,
        context,
        stringLen,
        pool,
        rule,
        attributes_len,
        attributes,
        is_checked=False,
    ):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xIIBB2x", context, stringLen, pool, rule))
        buf.write(xcffib.pack_list(attributes, "c"))
        return self.send_request(18, buf, is_checked=is_checked)

    def PrintGetPageDimensions(self, context, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xI", context))
        return self.send_request(
            21, buf, PrintGetPageDimensionsCookie, is_checked=is_checked
        )

    def PrintQueryScreens(self, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2x"))
        return self.send_request(
            22, buf, PrintQueryScreensCookie, is_checked=is_checked
        )

    def PrintSetImageResolution(self, context, image_resolution, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xIH", context, image_resolution))
        return self.send_request(
            23, buf, PrintSetImageResolutionCookie, is_checked=is_checked
        )

    def PrintGetImageResolution(self, context, is_checked=True):
        buf = io.BytesIO()
        buf.write(struct.pack("=xx2xI", context))
        return self.send_request(
            24, buf, PrintGetImageResolutionCookie, is_checked=is_checked
        )


xcffib._add_ext(key, xprintExtension, _events, _errors)
