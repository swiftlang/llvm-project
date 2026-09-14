func main() {
  let continuous = ContinuousClock.now
  let suspending = SuspendingClock.now

  print("break here")
}

main()