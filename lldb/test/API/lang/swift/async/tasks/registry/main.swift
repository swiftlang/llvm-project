actor Park {
  static let shared = Park()

  /// Parks the calling task forever, then resumes `creator`. Both are isolated
  /// to this actor, so `creator` cannot return from its `withUnsafeContinuation`
  /// until this task releases the actor, which happens only once it suspends.
  func forever(thenResume creator: UnsafeContinuation<Void, Never>) async {
    await withUnsafeContinuation { (_: UnsafeContinuation<Void, Never>) in
      creator.resume()
    }
  }

  func run() async {
    // Unstructured: no ChildFragment, so no parent edge, and nothing awaits its
    // future, so no waiter edge. The parent/child/waiter walk cannot reach it.
    await withUnsafeContinuation { (creator: UnsafeContinuation<Void, Never>) in
      Task(name: "unstructured") { await self.forever(thenResume: creator) }
    }

    // Detached: same, plus no task-local parent.
    await withUnsafeContinuation { (creator: UnsafeContinuation<Void, Never>) in
      Task.detached(name: "detached") {
        await Park.shared.forever(thenResume: creator)
      }
    }

    print("break here")
  }
}

@main struct Main {
  static func main() async {
    await Park.shared.run()
  }
}
