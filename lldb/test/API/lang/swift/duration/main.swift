func main() {
  let zero = Duration.zero
  let simple = Duration.seconds(3) + .milliseconds(33)
  let negative = Duration.seconds(-90)
  let fractionOnly = Duration.microseconds(15)
  let negativeFraction = Duration.seconds(-1) + .milliseconds(-500)
  let threshold = Duration.milliseconds(1)
  let belowThreshold = Duration.microseconds(999)

  print("break here")
}

main()
