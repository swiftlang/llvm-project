import OpaqueLib

func use() {
    let variable = Container.make(42)
    print("break here") // break here
    _ = variable
}

use()
