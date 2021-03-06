#pragma once

#include <cstdlib>

#include <phantasm-hardware-interface/commands.hh>

namespace inc
{
struct growing_writer
{
    growing_writer(size_t initial_size) { _writer.initialize(static_cast<std::byte*>(std::malloc(initial_size)), initial_size); }

    growing_writer() : growing_writer(1024) {}

    ~growing_writer() { std::free(_writer.buffer()); }

    void reset() { _writer.reset(); }

    template <class CmdT>
    void add_command(CmdT const& cmd)
    {
        accomodate_t<CmdT>();
        _writer.add_command(cmd);
    }

    size_t size() const { return _writer.size(); }
    std::byte* buffer() const { return _writer.buffer(); }
    size_t max_size() const { return _writer.max_size(); }

    template <class CmdT>
    void accomodate_t()
    {
        accomodate(sizeof(CmdT));
    }

    void accomodate(size_t num_bytes)
    {
        if (!_writer.can_accomodate(num_bytes))
        {
            size_t const new_size = (_writer.max_size() + num_bytes) << 1;
            std::byte* const new_buffer = static_cast<std::byte*>(std::malloc(new_size));

            std::memcpy(new_buffer, _writer.buffer(), _writer.size());

            std::free(_writer.buffer());
            _writer.exchange_buffer(new_buffer, new_size);
        }
    }

    phi::command_stream_writer& raw_writer() { return _writer; }

private:
    phi::command_stream_writer _writer;
};

}
