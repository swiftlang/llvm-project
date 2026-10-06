func takeCallback(body: (_ line: String, _ lineNum: Int, _ stop: inout Bool) -> Void) -> Void {
  var stop: Bool = false
  body("Hello", 3, &stop) // break here
}
let b = { (line: String, lineNum: Int, stop: inout Bool) -> Void in }

takeCallback(body: b)
