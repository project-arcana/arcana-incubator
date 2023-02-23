#pragma once

#include <clean-core/allocator.hh>

#include <phantasm-hardware-interface/commands.hh>

namespace inc
{
struct growing_writer
{
    growing_writer(size_t initial_size, cc::allocator* pAlloc)
    {
        _alloc = pAlloc;
        _writer.initialize(static_cast<std::byte*>(_alloc->alloc(initial_size)), initial_size);
    }

    growing_writer() : growing_writer(1024, cc::system_allocator) {}

    ~growing_writer() { _alloc->free(_writer.buffer()); }

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
            std::byte* const new_buffer = static_cast<std::byte*>(_alloc->realloc(_writer.buffer(), _writer.max_size(), new_size));
            _writer.exchange_buffer(new_buffer, new_size);
        }
    }

    phi::command_stream_writer& raw_writer() { return _writer; }

private:
    phi::command_stream_writer _writer;
    cc::allocator* _alloc;
};

} // namespace inc
