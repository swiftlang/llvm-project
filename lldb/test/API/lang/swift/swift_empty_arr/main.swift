protocol P {}

func go() {
    let x: [any P] = []
    print("break here")
}

go()
