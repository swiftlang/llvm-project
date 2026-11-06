#pragma once

namespace WTF {

enum class DestructionThread : unsigned char { Any, Main, MainRunLoop };

} // namespace WTF

using WTF::DestructionThread;
