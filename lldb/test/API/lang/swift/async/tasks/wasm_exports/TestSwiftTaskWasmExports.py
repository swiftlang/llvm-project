import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.gdbclientutils import *
from lldbsuite.test.lldbgdbclient import GDBRemoteTestBase
from lldbsuite.test import lldbutil

MODULE_ID = 4
MODULE_ADDRESS = (1 << 62) | (MODULE_ID << 32)

# The values of the globals concurrency.yaml exports.
VERSION_ADDRESS = 0x10
STORAGE_KIND_ADDRESS = 0x14
NAME_OFFSET_ADDRESS = 0x18
CURRENT_TASK_ADDRESS = 0x20
TLS_ARRAY_ADDRESS = 0x40
SUPPORTS_ESCALATION_ADDRESS = 0x300
WASM_GLOBALS = {
    0: VERSION_ADDRESS,
    1: CURRENT_TASK_ADDRESS,
    2: STORAGE_KIND_ADDRESS,
    3: TLS_ARRAY_ADDRESS,
    4: NAME_OFFSET_ADDRESS,
    5: SUPPORTS_ESCALATION_ADDRESS,
}

LAYOUT_VERSION = 3
STORAGE_KIND_GLOBAL = 2
STORAGE_KIND_GLOBAL_TLS_ARRAY = 5
STORAGE_KIND_DEFERRED_FLAG = 0x80
CONCURRENCY_TASK_TLS_KEY = 3

TASK_ADDRESS = 0x100
TASK_ID = 42
TASK_THREAD_ID_MASK = 0xF00000000
JOB_FLAGS_OFFSET = 4 * 4
TASK_ID_OFFSET = JOB_FLAGS_OFFSET + 4
JOB_FLAG_HAS_INITIAL_TASK_NAME = 1 << 30
NAME_FRAGMENT_OFFSET = 0x40
NAME_ADDRESS = 0x200
TASK_NAME = b"worker"
# Remote Mirror takes the high half of the task id from task + 80 when the
# runtime supports priority escalation, and from task + 72 when it does not.
TASK_ID_HIGH_OFFSET_WITHOUT_ESCALATION = 72

CALL_STACK = [MODULE_ADDRESS | 0x2, MODULE_ADDRESS | 0x10]


def little_endian(value, size):
    return value.to_bytes(size, "little")


class Responder(MockGDBServerResponder):
    def __init__(self, module_path, words):
        MockGDBServerResponder.__init__(self)
        with open(module_path, "rb") as module:
            self._module = module.read()
        self._memory = bytearray(0x10000)
        words[TASK_ADDRESS + JOB_FLAGS_OFFSET] = JOB_FLAG_HAS_INITIAL_TASK_NAME
        words[TASK_ADDRESS + TASK_ID_OFFSET] = TASK_ID
        words[NAME_OFFSET_ADDRESS] = NAME_FRAGMENT_OFFSET
        words[TASK_ADDRESS + NAME_FRAGMENT_OFFSET] = NAME_ADDRESS
        words[TASK_ADDRESS + NAME_FRAGMENT_OFFSET + 4] = len(TASK_NAME)
        words[SUPPORTS_ESCALATION_ADDRESS] = 1
        words[TASK_ADDRESS + TASK_ID_HIGH_OFFSET_WITHOUT_ESCALATION] = 7
        for address, value in words.items():
            self._memory[address : address + 4] = little_endian(value, 4)
        self._memory[NAME_ADDRESS : NAME_ADDRESS + len(TASK_NAME)] = TASK_NAME

    def respond(self, packet):
        if packet.startswith("qRegisterInfo"):
            if packet == "qRegisterInfo0":
                return "name:pc;alt-name:pc;bitsize:64;offset:0;encoding:uint;format:hex;set:General Purpose Registers;gcc:16;dwarf:16;generic:pc;"
            return "E45"
        if packet.startswith("qWasmCallStack"):
            return "".join(little_endian(pc, 8).hex() for pc in CALL_STACK)
        if packet.startswith("qWasmGlobal"):
            index = int(packet.split(";")[1])
            if index not in WASM_GLOBALS:
                return "E03"
            return little_endian(WASM_GLOBALS[index], 4).hex()
        return MockGDBServerResponder.respond(self, packet)

    def qSupported(self, client_supported):
        return "qXfer:libraries:read+;PacketSize=1000;vContSupported-"

    def qHostInfo(self):
        return ""

    def QEnableErrorStrings(self):
        return ""

    def qProcessInfo(self):
        return "pid:1;ppid:1;uid:1;gid:1;euid:1;egid:1;name:%s;triple:%s;ptrsize:4" % (
            hex_encode_bytes("lldb"),
            hex_encode_bytes("wasm32-unknown-unknown-wasm"),
        )

    def haltReason(self):
        return "T02thread:1;"

    def qfThreadInfo(self):
        return "m1"

    def readRegister(self, register):
        return little_endian(MODULE_ADDRESS | 0x2, 8).hex()

    def qXferRead(self, obj, annex, offset, length):
        if obj == "libraries":
            return (
                '<library-list><library name="concurrency"><section address="%d"/></library></library-list>'
                % MODULE_ADDRESS,
                False,
            )
        return None, False

    def readMemory(self, addr, length):
        offset = addr & 0xFFFFFFFF
        if addr >> 32 == MODULE_ADDRESS >> 32:
            data = self._module[offset : offset + length]
        elif addr >> 62 == 0:
            # A pointer read from linear memory carries no module id.
            data = self._memory[offset : offset + length]
        else:
            data = b""
        return data.hex() if data else "E03"


class TestSwiftTaskWasmExports(GDBRemoteTestBase):
    def connect_to_module(self, name, words):
        module_path = self.getBuildArtifact(name + ".wasm")
        self.yaml2obj(name + ".yaml", module_path)
        self.server.responder = Responder(module_path, words)

        target = self.dbg.CreateTarget("")
        process = self.connect(target, "wasm")
        lldbutil.expect_state_changes(
            self, self.dbg.GetListener(), process, [lldb.eStateStopped]
        )
        return process

    def check_current_task(self, words):
        process = self.connect_to_module("concurrency", words)

        thread = process.GetThreadAtIndex(0)
        self.assertEqual(thread.GetThreadID(), TASK_THREAD_ID_MASK | TASK_ID)
        self.assertEqual(thread.GetName(), "worker (Task 42)")
        self.assertEqual(
            [thread.GetFrameAtIndex(i).GetPC() for i in range(thread.GetNumFrames())],
            CALL_STACK,
        )
        caller = thread.GetFrameAtIndex(1)
        self.assertEqual(caller.FindRegister("pc").GetValueAsUnsigned(), CALL_STACK[1])
        self.expect(
            "language swift task list",
            substrs=["Task 42 ('worker'), addr = 0x100"],
        )

    def enable_log(self):
        log = self.getBuildArtifact(self.testMethodName + ".log")
        self.runCmd('log enable -f "%s" lldb os types' % log)
        self.addTearDownHook(lambda: self.runCmd("log disable lldb os types"))
        return log

    @swiftTest
    @skipIfXmlSupportMissing
    def test_global(self):
        self.check_current_task(
            {
                VERSION_ADDRESS: (STORAGE_KIND_GLOBAL << 24) | LAYOUT_VERSION,
                CURRENT_TASK_ADDRESS: TASK_ADDRESS,
            }
        )

    @swiftTest
    @skipIfXmlSupportMissing
    def test_deferred_global_tls_array(self):
        self.check_current_task(
            {
                VERSION_ADDRESS: (STORAGE_KIND_DEFERRED_FLAG << 24) | LAYOUT_VERSION,
                STORAGE_KIND_ADDRESS: STORAGE_KIND_GLOBAL_TLS_ARRAY,
                TLS_ARRAY_ADDRESS + CONCURRENCY_TASK_TLS_KEY * 4: TASK_ADDRESS,
            }
        )

    @swiftTest
    @skipIfXmlSupportMissing
    def test_current_task_outside_data(self):
        log = self.enable_log()
        process = self.connect_to_module(
            "invalid_globals",
            {VERSION_ADDRESS: (STORAGE_KIND_GLOBAL << 24) | LAYOUT_VERSION},
        )

        self.assertEqual(process.GetThreadAtIndex(0).GetThreadID(), 1)
        self.filecheck_log(log, __file__, "--check-prefix=OUTSIDE")
        # OUTSIDE: GlobalVarTaskFinder: _swift_concurrency_currentTask holds 0x0, outside every data section

    @swiftTest
    @skipIfXmlSupportMissing
    def test_name_offset_without_constant_value(self):
        log = self.enable_log()
        process = self.connect_to_module(
            "invalid_globals",
            {
                VERSION_ADDRESS: (STORAGE_KIND_DEFERRED_FLAG << 24) | LAYOUT_VERSION,
                STORAGE_KIND_ADDRESS: STORAGE_KIND_GLOBAL_TLS_ARRAY,
                TLS_ARRAY_ADDRESS + CONCURRENCY_TASK_TLS_KEY * 4: TASK_ADDRESS,
            },
        )

        thread = process.GetThreadAtIndex(0)
        self.assertEqual(thread.GetThreadID(), TASK_THREAD_ID_MASK | TASK_ID)
        self.assertEqual(thread.GetName(), "Task 42")
        self.runCmd("language swift task list")
        self.filecheck_log(log, __file__, "--check-prefix=NOVALUE")
        # NOVALUE: failed while looking for name of task 0x100: _swift_concurrency_debug_asyncTaskNameOffset has no constant address
        # NOVALUE: [MemoryReader] _swift_concurrency_debug_supportsPriorityEscalation has no constant address
