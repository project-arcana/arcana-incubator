#include "mesh_util.hh"

#include <cstdlib>

#include <clean-core/defer.hh>

#include <phantasm-hardware-interface/Backend.hh>
#include <phantasm-hardware-interface/commands.hh>

#include <arcana-incubator/asset-loading/mesh_loader.hh>

inc::phi_mesh inc::load_mesh(phi::Backend& backend, const char* path, bool binary)
{
    using namespace phi;

    handle::resource upload_buffer;

    auto const buffer_size = 1024ull * 2;
    auto* const buffer = static_cast<std::byte*>(std::malloc(buffer_size));
    CC_DEFER { std::free(buffer); };
    command_stream_writer writer(buffer, buffer_size);

    inc::phi_mesh res;

    {
        auto const mesh_data = binary ? inc::assets::load_binary_mesh(path) : inc::assets::load_obj_mesh(path);

        res.num_indices = unsigned(mesh_data.indices.size());

        auto const vert_size = mesh_data.get_vertex_size_bytes();
        auto const ind_size = mesh_data.get_index_size_bytes();

        res.vertex_buffer = backend.createBuffer(vert_size, sizeof(inc::assets::simple_vertex));
        res.index_buffer = backend.createBuffer(ind_size, sizeof(uint32_t));

        {
            cmd::transition_resources tcmd;
            tcmd.add(res.vertex_buffer, resource_state::copy_dest);
            tcmd.add(res.index_buffer, resource_state::copy_dest);
            writer.add_command(tcmd);
        }

        upload_buffer = backend.createUploadBuffer(vert_size + ind_size);
        std::byte* const upload_mapped = backend.mapBuffer(upload_buffer);

        std::memcpy(upload_mapped, mesh_data.vertices.data(), vert_size);
        std::memcpy(upload_mapped + vert_size, mesh_data.indices.data(), ind_size);

        writer.add_command(cmd::copy_buffer{res.vertex_buffer, 0, upload_buffer, 0, vert_size});
        writer.add_command(cmd::copy_buffer{res.index_buffer, 0, upload_buffer, vert_size, ind_size});

        {
            cmd::transition_resources tcmd;
            tcmd.add(res.vertex_buffer, resource_state::vertex_buffer);
            tcmd.add(res.index_buffer, resource_state::index_buffer);
            writer.add_command(tcmd);
        }
    }

    auto const setup_cmd_list = backend.recordCommandList(writer.buffer(), writer.size());

    backend.unmapBuffer(upload_buffer);
    backend.submit(cc::span{setup_cmd_list});
    backend.flushGPU();
    backend.free(upload_buffer);

    return res;
}
