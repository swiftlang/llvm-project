private class V
{ 
    func layoutSubviews() {
        print("break here")
    }
}

private var my_v = V()
my_v.layoutSubviews()
