import LLDB

public extension lldb.SBTarget {

    var modules: GenericSBList<lldb.SBModule> {
        get {
            GenericSBList(count: Int(self.GetNumModules())) {
                var me = self; return me.GetModuleAtIndex(UInt32($0))
            }
        }
    }
}
