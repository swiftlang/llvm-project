import Foundation

@inline(never)
func makeNil() -> Unmanaged<CFError>? { return nil }

func f() {
  let unmanaged: Unmanaged<CFError> = Unmanaged.passUnretained(
    CFErrorCreate(nil, kCFErrorDomainPOSIX, 0, nil))
  let optUnmanaged: Unmanaged<CFError>? = Unmanaged.passUnretained(
    CFErrorCreate(nil, kCFErrorDomainPOSIX, 0, nil))
  let nilUnmanaged: Unmanaged<CFError>? = makeNil()
  print("break here \(unmanaged)")
}

f()
