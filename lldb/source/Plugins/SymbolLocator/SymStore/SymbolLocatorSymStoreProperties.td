include "../../../../include/lldb/Core/PropertiesBase.td"

let Definition = "symbollocatorsymstore" in {
  def SymStoreURLs : Property<"urls", "Array">,
    ElementType<"String">,
    Desc<"List of local symstore directories to query for symbols">;
}
