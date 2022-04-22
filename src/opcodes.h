#pragma once

namespace m65dap {

enum class Mnemonic { Illegal, BSR, JSR };

enum class AddressingMode { Absolute, AbsoluteIndirect, AbsoluteIndirectX, RelativeWord };

struct Opcode {
  std::byte code;
  Mnemonic mnemonic{Mnemonic::Illegal};
  AddressingMode mode{AddressingMode::Absolute};
};

constexpr std::array<Opcode, 5> opcodes{{{},
                                         {std::byte{0x20}, Mnemonic::JSR, AddressingMode::Absolute},
                                         {std::byte{0x22}, Mnemonic::JSR, AddressingMode::AbsoluteIndirect},
                                         {std::byte{0x23}, Mnemonic::JSR, AddressingMode::AbsoluteIndirectX},
                                         {std::byte{0x63}, Mnemonic::BSR, AddressingMode::RelativeWord}}};

constexpr auto get_num_opcodes(Mnemonic m) -> int
{
  return std::count_if(opcodes.begin(), opcodes.end(), [&](const Opcode& o) { return o.mnemonic == m; });
}

constexpr auto get_num_opcodes(std::initializer_list<Mnemonic> m) -> int
{
  int count{0};
  return std::accumulate(m.begin(), m.end(), 0, [](int acc, Mnemonic m) { return acc + get_num_opcodes(m); });
}

constexpr auto get_opcodes(Mnemonic m) -> const std::span<const Opcode>
{
  auto it1 = std::find_if(opcodes.cbegin(), opcodes.cend(), [&](const Opcode& o) -> bool { return o.mnemonic == m; });
  auto it2 = std::find_if(it1, opcodes.end(), [&](const Opcode& o) -> bool { return o.mnemonic != m; });
  return {it1, it2};
}

constexpr auto get_opcode(std::byte code) -> const Opcode&
{
  const auto it = std::find_if(opcodes.begin(), opcodes.end(), [&](const Opcode& o) { return o.code == code; });
  if (it == opcodes.end()) {
    return *(opcodes.begin());
  }
  return *it;
};

}  // namespace m65dap
