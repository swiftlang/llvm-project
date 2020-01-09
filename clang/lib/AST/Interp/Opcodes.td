//===--- Opcodes.td - Opcode defitions for the constexpr VM -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Helper file used to generate opcodes, the interpreter and the disassembler.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Types evaluated by the interpreter.
//===----------------------------------------------------------------------===//

class Type;
def Bool : Type;
def Sint8 : Type;
def Uint8 : Type;
def Sint16 : Type;
def Uint16 : Type;
def Sint32 : Type;
def Uint32 : Type;
def Sint64 : Type;
def Uint64 : Type;
def SintFP : Type;
def UintFP : Type;
def RealFP : Type;
def Ptr : Type;
def FnPtr : Type;
def MemPtr : Type;
def VoidPtr : Type;
def ObjCBlockPtr : Type;

//===----------------------------------------------------------------------===//
// Types transferred to the interpreter.
//===----------------------------------------------------------------------===//

class ArgType { string Name = ?; }
def ArgSint8 : ArgType { let Name = "int8_t"; }
def ArgUint8 : ArgType { let Name = "uint8_t"; }
def ArgSint16 : ArgType { let Name = "int16_t"; }
def ArgUint16 : ArgType { let Name = "uint16_t"; }
def ArgSint32 : ArgType { let Name = "int32_t"; }
def ArgUint32 : ArgType { let Name = "uint32_t"; }
def ArgSint64 : ArgType { let Name = "int64_t"; }
def ArgUint64 : ArgType { let Name = "uint64_t"; }
def ArgBool : ArgType { let Name = "bool"; }
def ArgGlobal : ArgType { let Name = "GlobalLocation"; }

def ArgFunction : ArgType { let Name = "Function *"; }
def ArgRecord : ArgType { let Name = "Record *"; }
def ArgDescriptor : ArgType { let Name = "Descriptor *"; }

def ArgSema : ArgType { let Name = "const llvm::fltSemantics *"; }

def ArgExpr : ArgType { let Name = "const Expr *"; }
def ArgFloatingLiteral : ArgType { let Name = "const FloatingLiteral *"; }
def ArgCXXMethodDecl : ArgType { let Name = "const CXXMethodDecl *"; }
def ArgFunctionDecl : ArgType { let Name = "const FunctionDecl *"; }
def ArgRecordDecl : ArgType { let Name = "const RecordDecl *"; }
def ArgCXXRecordDecl : ArgType { let Name = "const CXXRecordDecl *"; }
def ArgVarDecl : ArgType { let Name = "const VarDecl *"; }
def ArgValueDecl : ArgType { let Name = "const ValueDecl *"; }
def ArgRecordField : ArgType { let Name = "const Record::Field *"; }
def ArgBlockExpr : ArgType { let Name = "const BlockExpr *"; }
def ArgAddrLabelExpr : ArgType { let Name = "const AddrLabelExpr *"; }
def ArgMaterializeTemporaryExpr : ArgType { let Name = "const MaterializeTemporaryExpr *"; }
def ArgTypePtr : ArgType { let Name = "const Type *"; }
def ArgCXXTypeidExpr : ArgType { let Name = "const CXXTypeidExpr *"; }

//===----------------------------------------------------------------------===//
// Classes of types intructions operate on.
//===----------------------------------------------------------------------===//

class TypeClass {
  list<Type> Types;
}

def IntTypeClass : TypeClass {
  let Types = [Sint8, Uint8, Sint16, Uint16, Sint32,
               Uint32, Sint64, Uint64, Bool];
}

def PtrTypeClass : TypeClass {
  let Types = [Ptr, FnPtr, MemPtr, VoidPtr, ObjCBlockPtr];
}

def FPTypeClass : TypeClass {
  let Types = [SintFP, UintFP];
}

def RealFPTypeClass : TypeClass {
  let Types = [RealFP];
}

def IntFPTypeClass : TypeClass {
  let Types = !listconcat(IntTypeClass.Types, FPTypeClass.Types);
}

def AluFPRealFPTypeClass : TypeClass {
  let Types = !listconcat(IntTypeClass.Types, FPTypeClass.Types,
                          RealFPTypeClass.Types);
}

def AllTypeClass : TypeClass {
  let Types = !listconcat(AluFPRealFPTypeClass.Types, PtrTypeClass.Types);
}

def ArithTypeClass : TypeClass {
  let Types = !listconcat(AluFPRealFPTypeClass.Types, [Ptr]);
}

class SingletonTypeClass<Type Ty> : TypeClass {
  let Types = [Ty];
}

//===----------------------------------------------------------------------===//
// Record describing all opcodes.
//===----------------------------------------------------------------------===//

class Opcode {
  list<TypeClass> Types = [];
  list<ArgType> Args = [];
  string Name = "";
  bit CanReturn = 0;
  bit ChangesPC = 0;
  bit HasCustomLink = 0;
  bit HasCustomEval = 0;
  bit HasGroup = 0;
}

class AluOpcode : Opcode {
  let Types = [IntFPTypeClass];
  let HasGroup = 1;
}

class AluRealOpcode : Opcode {
  let Types = [AluFPRealFPTypeClass];
  let HasGroup = 1;
}

//===----------------------------------------------------------------------===//
// Jump opcodes
//===----------------------------------------------------------------------===//

class JumpOpcode : Opcode {
  let Args = [ArgSint32];
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [] -> []
def Jmp : JumpOpcode;
// [Bool] -> [], jumps if true.
def Jt : JumpOpcode;
// [Bool] -> [], jumps if false.
def Jf : JumpOpcode;

//===----------------------------------------------------------------------===//
// Call opcodes
//
// The default calling convention, which does not support varargs, pops the
// arguments on exit. Vararg functions require arguments to be caller-popped.
//
//===----------------------------------------------------------------------===//

// [Args...] -> []
def Call : Opcode {
  let Args = [ArgFunction];
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [Args..., FnPointer] -> []
def IndirectCall : Opcode {
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [Args..., Pointer] -> []
def Invoke : Opcode {
  let Args = [ArgFunction];
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [Args..., Pointer, MemberPtr] -> []
def IndirectInvoke : Opcode {
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [Args..., Pointer] -> []
def VirtualInvoke : Opcode {
  let Args = [ArgCXXMethodDecl];
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [Args...] -> []
def Builtin : Opcode {
  let Args = [ArgUint32];
}

// [] -> []
def CheckFunction : Opcode {
  let Args = [ArgFunctionDecl];
}

// [] -> []
def CheckMethod : Opcode {
  let Args = [ArgCXXMethodDecl];
}

//===----------------------------------------------------------------------===//
// Invalid relocation trap
//===----------------------------------------------------------------------===//

// [] -> EXIT
def NoCall : Opcode {
  let Args = [ArgFunctionDecl];
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

// [] -> EXIT
def NoInvoke : Opcode {
  let Args = [ArgCXXMethodDecl];
  let ChangesPC = 1;
  let HasCustomEval = 1;
}

//===----------------------------------------------------------------------===//
// Returns
//===----------------------------------------------------------------------===//

// [Value] -> []
def Ret : Opcode {
  let Types = [AllTypeClass];
  let ChangesPC = 1;
  let CanReturn = 1;
  let HasGroup = 1;
  let HasCustomEval = 1;
}
// [] -> []
def RetVoid : Opcode {
  let CanReturn = 1;
  let ChangesPC = 1;
  let HasCustomEval = 1;
}
// [Value] -> []
def RetValue : Opcode {
  let CanReturn = 1;
  let ChangesPC = 1;
  let HasCustomEval = 1;
}
// [] -> []
def RetCons : Opcode {
  let CanReturn = 1;
  let ChangesPC = 1;
  let HasCustomEval = 1;
}
// [] -> EXIT
def NoRet : Opcode {}

//===----------------------------------------------------------------------===//
// Frame management
//===----------------------------------------------------------------------===//

// [] -> []
def Destroy : Opcode {
  let Args = [ArgUint32];
  let HasCustomEval = 1;
}

//===----------------------------------------------------------------------===//
// Constants
//===----------------------------------------------------------------------===//

class ConstOpcode<Type Ty, ArgType ArgTy> : Opcode {
  let Types = [SingletonTypeClass<Ty>];
  let Args = [ArgTy];
  let Name = "Const";
}

// [] -> [Integer]
def ConstSint8 : ConstOpcode<Sint8, ArgSint8>;
def ConstUint8 : ConstOpcode<Uint8, ArgUint8>;
def ConstSint16 : ConstOpcode<Sint16, ArgSint16>;
def ConstUint16 : ConstOpcode<Uint16, ArgUint16>;
def ConstSint32 : ConstOpcode<Sint32, ArgSint32>;
def ConstUint32 : ConstOpcode<Uint32, ArgUint32>;
def ConstSint64 : ConstOpcode<Sint64, ArgSint64>;
def ConstUint64 : ConstOpcode<Uint64, ArgUint64>;
def ConstBool : ConstOpcode<Bool, ArgBool>;

// [] -> [Integer]
def ConstFP : Opcode {
  let Types = [FPTypeClass];
  let Args = [ArgUint32, ArgUint64];
  let Name = "ConstFP";
}

// [] -> [Real]
def ConstRealFP : Opcode {
  let Args = [ArgFloatingLiteral];
}

// [] -> [FnPointer]
def ConstFn : Opcode {
  let Args = [ArgFunction];
}

// [] -> [FnPointer]
def ConstNoFn : Opcode {
  let Args = [ArgFunctionDecl];
}

// [] -> [MemberPointer]
def ConstMem : Opcode {
  let Args = [ArgValueDecl];
}

// [] -> [ObJCBlockPointer]
def ConstObjCBlock : Opcode {
  let Args = [ArgBlockExpr];
}

// [] -> [VoidPointer]
def ConstAddrLabel : Opcode {
  let Args = [ArgAddrLabelExpr];
}

// [] -> [Integer]
def Zero : Opcode {
  let Types = [IntTypeClass];
}

// [] -> [Integer]
def ZeroFP : Opcode {
  let Types = [FPTypeClass];
  let Args = [ArgUint32];
}

// [] -> [Real]
def ZeroRealFP : Opcode {
  let Args = [ArgSema];
}

def Null : Opcode {
  let Types = [PtrTypeClass];
}

def NullPtrT : Opcode {
  let Args = [ArgDescriptor];
}

//===----------------------------------------------------------------------===//
// Pointer generation
//===----------------------------------------------------------------------===//

// [] -> [Pointer]
def GetPtrLocal : Opcode {
  // Offset of local.
  let Args = [ArgUint32];
  bit HasCustomEval = 1;
}
// [] -> [Pointer]
def GetPtrParam : Opcode {
  // Offset of parameter.
  let Args = [ArgUint32];
}
// [] -> [Pointer]
def GetPtrGlobal : Opcode {
  // Index of global.
  let Args = [ArgGlobal];
}
// [] -> [Pointer]
def GetDummyPtr : Opcode {
  let Args = [ArgValueDecl, ArgDescriptor];
}
// [] -> [Pointer]
def GetDummyVoidPtr : Opcode {
  let Args = [ArgValueDecl, ArgDescriptor];
}
// [] -> [FnPointer]
def GetDummyFnPtr : Opcode {
  let Args = [ArgValueDecl];
}
// [] -> [ObjCBlockPointer]
def GetDummyObjCBlockPtr : Opcode {
  let Args = [ArgValueDecl];
}
// [] -> [MemberPointer]
def GetDummyMemberPtr : Opcode {
  let Args = [ArgValueDecl];
}
// [Pointer] -> [Pointer]
def GetPtrField : Opcode {
  // Offset of field.
  let Args = [ArgUint32];
}
// [Pointer] -> [Pointer]
def GetPtrActiveField : Opcode {
  // Offset of field.
  let Args = [ArgUint32];
}
// [] -> [Pointer]
def GetPtrActiveThisField : Opcode {
  // Offset of field.
  let Args = [ArgUint32];
}
// [] -> [Pointer]
def GetPtrThisField : Opcode {
  // Offset of field.
  let Args = [ArgUint32];
}
// [Pointer] -> [Pointer]
def GetPtrBase : Opcode {
  // Offset of field, which is a base.
  let Args = [ArgUint32];
}
// [Pointer] -> [Pointer]
def GetPtrVirtBase : Opcode {
  // RecordDecl of base class.
  let Args = [ArgRecordDecl];
}
// [] -> [Pointer]
def GetPtrThisBase : Opcode {
  // Offset of field, which is a base.
  let Args = [ArgUint32];
}
// [] -> [Pointer]
def GetPtrThisVirtBase : Opcode {
  // RecordDecl of base class.
  let Args = [ArgRecordDecl];
}
// [] -> [Pointer]
def This : Opcode;

// [Pointer, MemberPointer] -> [Pointer]
def GetPtrFieldIndirect : Opcode;

// [Pointer] -> [Pointer]
def DecayPtr : Opcode;

//===----------------------------------------------------------------------===//
// Direct field accessors
//===----------------------------------------------------------------------===//

class AccessOpcode : Opcode {
  let Types = [AllTypeClass];
  let Args = [ArgUint32];
  let HasGroup = 1;
}

class GlobalOpcode : Opcode {
  let Types = [AllTypeClass];
  let Args = [ArgGlobal];
  let HasGroup = 1;
}

class BitFieldOpcode : Opcode {
  let Types = [IntFPTypeClass];
  let Args = [ArgRecordField];
  let HasGroup = 1;
}

// [] -> [Pointer]
def GetLocal : AccessOpcode { let HasCustomEval = 1; }
// [] -> [Pointer]
def SetLocal : AccessOpcode { let HasCustomEval = 1; }

// [] -> [Value]
def GetGlobal : GlobalOpcode;
// [Value] -> []
def InitGlobal : GlobalOpcode;
// [Value] -> []
def SetGlobal : GlobalOpcode;
// [Value] -> [Value]
def InitTemp : Opcode {
  let Types = [AllTypeClass];
  let Args = [ArgMaterializeTemporaryExpr, ArgGlobal];
  let HasGroup = 1;
}
// [Pointer] -> [Pointer]
def InitTempValue : Opcode {
  let Types = [];
  let Args = [ArgMaterializeTemporaryExpr, ArgGlobal];
}

// [] -> [Value]
def GetParam : AccessOpcode;
// [Value] -> []
def SetParam : AccessOpcode;

// [Pointer] -> [Pointer, Value]
def GetField : AccessOpcode;
// [Pointer] -> [Value]
def GetFieldPop : AccessOpcode;
// [] -> [Value]
def GetThisField : AccessOpcode;

// [Pointer] -> [Value]
def GetElem : AccessOpcode;

// [Pointer, Value] -> [Pointer]
def SetField : AccessOpcode;
// [Pointer, Value] -> []
def SetFieldPop : AccessOpcode;
// [Value] -> []
def SetThisField : AccessOpcode;

// [Value] -> []
def InitThisField : AccessOpcode;
// [Value] -> []
def InitThisFieldActive : AccessOpcode;
// [Value] -> []
def InitThisBitField : BitFieldOpcode;
// [Pointer, Value] -> [Pointer, Value]
def InitFieldPeek : AccessOpcode;
// [Pointer, Value] -> [Pointer]
def InitField : AccessOpcode;
// [Pointer, Value] -> []
def InitFieldPop : AccessOpcode;
// [Pointer, Value] -> []
def InitBitField : BitFieldOpcode;
// [Pointer, Value] -> []
def InitFieldActive : AccessOpcode;

//===----------------------------------------------------------------------===//
// Pointer access
//===----------------------------------------------------------------------===//

class LoadOpcode : Opcode {
  let Types = [AllTypeClass];
  let HasGroup = 1;
}

// [Pointer] -> [Pointer, Value]
def Load : LoadOpcode {}
// [Pointer] -> [Value]
def LoadPop : LoadOpcode {}

class StoreOpcode : Opcode {
  let Types = [AllTypeClass];
  let HasGroup = 1;
}

class StoreBitFieldOpcode : Opcode {
  let Types = [IntFPTypeClass];
  let HasGroup = 1;
}

// [Pointer, Value] -> [Pointer]
def Store : StoreOpcode {}
// [Pointer, Value] -> []
def StorePop : StoreOpcode {}

// [Pointer, Value] -> [Pointer]
def StoreBitField : StoreBitFieldOpcode {}
// [Pointer, Value] -> []
def StoreBitFieldPop : StoreBitFieldOpcode {}

// [Pointer, Value] -> []
def InitPop : StoreOpcode {}
// [Pointer, Value] -> [Pointer]
def InitElem : Opcode {
  let Types = [AllTypeClass];
  let Args = [ArgUint32];
  let HasGroup = 1;
}
// [Pointer, Value] -> []
def InitElemPop : Opcode {
  let Types = [AllTypeClass];
  let Args = [ArgUint32];
  let HasGroup = 1;
}

// [Pointer] -> []
def Initialise : Opcode;
def Activate : Opcode;

//===----------------------------------------------------------------------===//
// Pointer arithmetic.
//===----------------------------------------------------------------------===//

// [Pointer, Integral] -> [Pointer]
def AddOffset : AluOpcode;
// [Pointer, Integral] -> [Pointer]
def SubOffset : AluOpcode;

// [Pointer, Pointer] -> [Integral]
def PtrDiff : Opcode {
  let Types = [IntTypeClass];
  let HasGroup = 1;
}
// [Pointer, Pointer] -> [Integral]
def PtrDiffFP : Opcode {
  let Types = [FPTypeClass];
  let Args = [ArgUint32];
  let HasGroup = 1;
}

def RealElem : Opcode;
def ImagElem : Opcode;

//===----------------------------------------------------------------------===//
// Binary operators.
//===----------------------------------------------------------------------===//

// [Real, Real] -> [Real]
def Sub : AluRealOpcode;
def Add : AluRealOpcode;
def Mul : AluRealOpcode;
def Div : AluRealOpcode;
// [Integral, Integral] -> [Integral]
def Rem : AluOpcode;
def And : AluOpcode;
def Or : AluOpcode;
def Xor : AluOpcode;

//===----------------------------------------------------------------------===//
// Bit shift operators.
//===----------------------------------------------------------------------===//

class ShiftOpcode : Opcode {
  let Types = [IntFPTypeClass, IntFPTypeClass];
  let HasGroup = 1;
}

// [Integral, Integral] -> [Integral]
def Shr : ShiftOpcode;
def Shl : ShiftOpcode;
def ShrTrunc : ShiftOpcode;
def ShlTrunc : ShiftOpcode;

//===----------------------------------------------------------------------===//
// Unary operators.
//===----------------------------------------------------------------------===//

// [Real] -> [Real]
def Minus : AluRealOpcode;
// [Integral] -> [Integral]
def Not : AluOpcode;

//===----------------------------------------------------------------------===//
// Logical negation.
//===----------------------------------------------------------------------===//

// [Value] -> [Bool]
def LogicalNot : Opcode {
  let Types = [AllTypeClass, IntTypeClass];
  let HasGroup = 1;
}
// [Integral] -> [Integral]
def Test : Opcode {
  let Types = [AllTypeClass, IntTypeClass];
  let HasGroup = 1;
}

//===----------------------------------------------------------------------===//
// Increment/decrement.
//===----------------------------------------------------------------------===//

class IncDecOpcode : Opcode {
  let Types = [ArithTypeClass];
  let HasGroup = 1;
}

// [Pointer] -> [Arith]
def PostInc : IncDecOpcode;
// [Pointer] -> [Arith]
def PostDec : IncDecOpcode;

// [Pointer] -> [Pointer]
def PreInc : IncDecOpcode;
// [Pointer] -> [Pointer]
def PreDec : IncDecOpcode;

//===----------------------------------------------------------------------===//
// Comparison opcodes.
//===----------------------------------------------------------------------===//

class ComparisonOpcode : Opcode {
  let Types = [AllTypeClass, IntTypeClass];
  let HasGroup = 1;
}

def EQ : ComparisonOpcode;
def NE : ComparisonOpcode;
def LT : ComparisonOpcode;
def LE : ComparisonOpcode;
def GT : ComparisonOpcode;
def GE : ComparisonOpcode;

// [Pointer, Value, Value] -> []
def Spaceship : Opcode {
  let Types = [AllTypeClass];
  let HasGroup = 1;
}

// [Real, Real, Real] -> [Bool]
def InRange : Opcode {
  let Types = [IntFPTypeClass];
  let HasGroup = 1;
}

//===----------------------------------------------------------------------===//
// Stack management.
//===----------------------------------------------------------------------===//

// [Value] -> []
def Pop : Opcode {
  let Types = [AllTypeClass];
  let HasGroup = 1;
}

// [Value] -> [Value, Value]
def Dup : Opcode {
  let Types = [AllTypeClass];
  let HasGroup = 1;
}

//===----------------------------------------------------------------------===//
// Cast operators
//===----------------------------------------------------------------------===//

// [Value] -> [Value]
def Cast : Opcode {
  let Types = [IntFPTypeClass, IntTypeClass];
  let HasGroup = 1;
}

// [Real] -> [Alu]
def CastRealFPToAlu : Opcode {
  let Types = [RealFPTypeClass, IntTypeClass];
  let HasGroup = 1;
}

// [Real] -> [AluFP]
def CastRealFPToAluFP : Opcode {
  let Types = [RealFPTypeClass, IntFPTypeClass];
  let Args = [ArgUint32];
  let HasGroup = 1;
}

// [Value] -> [Value]
def CastFP : Opcode {
  let Types = [IntFPTypeClass, FPTypeClass];
  let Args = [ArgUint32];
  let HasGroup = 1;
}

// [Value] -> [Real]
def CastRealFP : Opcode {
  let Types = [AluFPRealFPTypeClass];
  let Args = [ArgSema];
  let HasGroup = 1;
}

// [MemberPtr] -> [MemberPtr]
def CastMemberToDerived : Opcode {
  let Args = [ArgCXXRecordDecl];
}

// [MemberPtr] -> [MemberPtr]
def CastMemberToBase : Opcode {
  let Args = [ArgCXXRecordDecl];
}

// [Pointer] -> [Pointer]
def CastToDerived : Opcode {
  let Args = [ArgExpr];
}

// [Pointer] -> [Pointer]
def BitCast : Opcode {
  let Args = [];
  let Types = [AllTypeClass, AllTypeClass];
  let HasGroup = 1;
}

//===----------------------------------------------------------------------===//
// TypeInfo
//===----------------------------------------------------------------------===//

def TypeInfo : Opcode {
  let Args = [ArgTypePtr, ArgTypePtr];
}

def TypeInfoPoly : Opcode {
  let Args = [ArgTypePtr];
}

//===----------------------------------------------------------------------===//
// Union copy constructor
//===----------------------------------------------------------------------===//

def InitUnion : Opcode;

//===----------------------------------------------------------------------===//
// Pointer generation
//===----------------------------------------------------------------------===//

def CreateGlobal : Opcode { let Args = [ArgVarDecl]; }
def GetGlobalValue : Opcode {
  let Args = [ArgVarDecl];
  let Types = [IntTypeClass];
  let HasGroup = 1;
}

//===----------------------------------------------------------------------===//
// Trap instructions.
//===----------------------------------------------------------------------===//

def Trap : Opcode;
def TrapReadNonConstInt : Opcode { let Args = [ArgValueDecl]; }
def TrapReadNonConstexpr : Opcode { let Args = [ArgValueDecl]; }
def TrapModifyGlobal : Opcode;
def TrapNonLiteral : Opcode;
def Warn : Opcode;
def WarnReadNonConstInt : Opcode { let Args = [ArgValueDecl]; }
def WarnReadNonConstexpr : Opcode { let Args = [ArgValueDecl]; }
def WarnNonConstexpr : Opcode { let Args = [ArgValueDecl]; }
def WarnPolyTypeid : Opcode {let Args = [ArgCXXTypeidExpr]; }
def WarnReinterpretCast : Opcode;
def WarnDynamicCast : Opcode;
def Note : Opcode;
