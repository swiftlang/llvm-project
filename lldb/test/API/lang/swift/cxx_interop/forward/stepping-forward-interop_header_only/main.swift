import CxxFunctions

func testFunction() {
  cxxFunction() // Break here for function
}

func testMethod() {
  var cxxClass = CxxClass()
  cxxClass.cxxMethod() // Break here for method
}

func testContructor() {
  var classWithConstructor = ClassWithConstructor(4, true, 5.7); // Break here for constructor
}

protocol P {
  mutating func definedInExtension() -> Int32
}

extension ClassWithExtension: P {}

func testClassWithExtension() {
  var classWithExtension: P = ClassWithExtension()
  classWithExtension.definedInExtension() // Break here for extension
}


func testCallOperator() {
  var classWithCallOperator = ClassWithCallOperator()
  classWithCallOperator() // Break here for call operator
}


testFunction()
testMethod()
testContructor()
testClassWithExtension()
testCallOperator()
