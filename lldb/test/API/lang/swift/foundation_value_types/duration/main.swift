import Foundation
 
func main() {
  let zero = Duration.zero
  let simple = Duration.seconds(3) + .milliseconds(33)
  let negative = Duration.seconds(-90)
  let fractionOnly = Duration.microseconds(15)
  let negativeFraction = Duration.seconds(-1) + .milliseconds(-500)
  
  return //%self.expect("type category list", substrs=['swift'])
         //%self.expect("type summary list -l swift", substrs=['Swift.Duration summary provider'])
         //%self.expect("frame var -d run -- zero", substrs=['0.0 seconds'])
         //%self.expect("expr -d run -- zero", substrs=['0.0 seconds'])
         //%self.expect("frame var -d run -- simple", substrs=['3.033 seconds'])
         //%self.expect("expr -d run -- simple", substrs=['3.033 seconds'])
         //%self.expect("frame var -d run -- negative", substrs=['-90.0 seconds'])
         //%self.expect("expr -d run -- negative", substrs=['-90.0 seconds'])
         //%self.expect("frame var -d run -- fractionOnly", substrs=['0.000015 seconds'])
         //%self.expect("expr -d run -- fractionOnly", substrs=['0.000015 seconds'])
         //%self.expect("frame var -d run -- negativeFraction", substrs=['-1.5 seconds'])
         //%self.expect("expr -d run -- negativeFraction", substrs=['-1.5 seconds'])
}

main()
