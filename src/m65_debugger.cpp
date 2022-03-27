#include "m65_debugger.h"

#include "serial_connection.h"

M65Debugger::M65Debugger(std::string_view serial_port_device) :
  conn_(std::make_unique<SerialConnection>(serial_port_device))
{

}

void M65Debugger::set_target(const std::filesystem::path& prg_path)
{
  auto file_size = std::filesystem::file_size(prg_path);
  if (file_size > 65536)
  {
    throw std::runtime_error(fmt::format("File size >64KB not supported"));
  }
  if (file_size < 3)
  {
    throw std::runtime_error("PRG file is too small");
  }

  std::vector<char> prg_data;
  prg_data.reserve(file_size);

  std::ifstream prg_file(prg_path, std::ios::binary);
  prg_data.assign(std::istreambuf_iterator<char>(prg_file),
                  std::istreambuf_iterator<char>());

  int load_address = static_cast<int>(prg_data[0]) +
                     static_cast<int>(prg_data[1]) * 256;

  std::span<char> payload(prg_data.data() + 2, prg_data.size() - 2);

  auto end_address_plus_one = load_address + payload.size();

  auto cmd = fmt::format("l{0:x} {1:x}\n", load_address, end_address_plus_one);
  conn_->write(cmd);
  conn_->write(payload);
}
