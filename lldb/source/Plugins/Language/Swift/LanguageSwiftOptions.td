include "../../../../source/Commands/OptionsBase.td"

let Command = "expression swift" in {
  def bind_generic_types : Option<"bind-generic-types", "\\x31">,
                           EnumArg<"BindGenTypeParamValue">,
                           Desc<"Controls whether any "
                                "generic types in the current context should "
                                "be bound to their dynamic "
                                "type before evaluating. Defaults to auto.">;
}
