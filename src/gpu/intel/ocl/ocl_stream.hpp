/*******************************************************************************
* Copyright 2019-2024 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef GPU_INTEL_OCL_OCL_STREAM_HPP
#define GPU_INTEL_OCL_OCL_STREAM_HPP

#include <memory>

#include "common/c_types_map.hpp"
#include "common/thread_local_storage.hpp"

#include "xpu/ocl/stream_impl.hpp"
#include "xpu/stream_profiler.hpp"

#include "gpu/intel/compute/compute_stream.hpp"
#include "gpu/intel/ocl/mdapi_utils.hpp"
#include "gpu/intel/ocl/ocl_context.hpp"
#include "gpu/intel/ocl/ocl_utils.hpp"

namespace dnnl {
namespace impl {
namespace gpu {
namespace intel {
namespace ocl {

struct ocl_stream_t : public compute::compute_stream_t {
    static status_t create_stream(
            impl::stream_t **stream, engine_t *engine, unsigned flags) {

        std::unique_ptr<ocl_stream_t> ocl_stream(
                new ocl_stream_t(engine, flags));
        if (!ocl_stream) return status::out_of_memory;

        status_t status = ocl_stream->init();
        if (status != status::success) return status;

        *stream = ocl_stream.release();
        return status::success;
    }

    static status_t create_stream(
            impl::stream_t **stream, engine_t *engine, cl_command_queue queue) {
        unsigned flags;
        CHECK(xpu::ocl::stream_impl_t::init_flags(&flags, queue));

        std::unique_ptr<ocl_stream_t> ocl_stream(
                new ocl_stream_t(engine, flags, queue));
        if (!ocl_stream) return status::out_of_memory;

        CHECK(ocl_stream->init());

        *stream = ocl_stream.release();
        return status::success;
    }

    status_t wait() override {
        OCL_CHECK(clFinish(queue_));
        return status::success;
    }

    void before_exec_hook() override;
    void after_exec_hook() override;

    status_t reset_profiling() override {
        if (!is_profiling_enabled()) return status::invalid_arguments;
        profiler_->reset();
        return status::success;
    }

    status_t get_profiling_data(profiling_data_kind_t data_kind,
            int *num_entries, uint64_t *data) const override {
        if (!is_profiling_enabled()) return status::invalid_arguments;
        return profiler_->get_info(data_kind, num_entries, data);
    }

    cl_command_queue queue() const { return queue_; }

    const mdapi_helper_t &mdapi_helper() const { return *mdapi_helper_; }

    status_t copy(const memory_storage_t &src, const memory_storage_t &dst,
            size_t size, const xpu::event_t &deps,
            xpu::event_t &out_dep) override;

    status_t fill(const memory_storage_t &dst, uint8_t pattern, size_t size,
            const xpu::event_t &deps, xpu::event_t &out_dep) override;

    ~ocl_stream_t() override {
        if (queue_) { clReleaseCommandQueue(queue_); }
    }

    const ocl_context_t &ocl_ctx() const {
        static ocl_context_t empty_ctx {};
        return ctx_.get(empty_ctx);
    }
    ocl_context_t &ocl_ctx() {
        const ocl_context_t &ctx
                = const_cast<const ocl_stream_t *>(this)->ocl_ctx();
        return *const_cast<ocl_context_t *>(&ctx);
    }
    xpu::context_t &ctx() override { return ocl_ctx(); }
    const xpu::context_t &ctx() const override { return ocl_ctx(); }

    const xpu::ocl::wrapper_t<cl_event> &get_output_event() const {
        auto &deps = ocl_event_t::from(ctx().get_deps());
        assert(deps.size() == 1);
        return deps[0];
    }

private:
    ocl_stream_t(engine_t *engine, unsigned flags)
        : compute_stream_t(engine, new xpu::ocl::stream_impl_t(flags)) {}
    ocl_stream_t(engine_t *engine, unsigned flags, cl_command_queue queue)
        : compute_stream_t(engine, new xpu::ocl::stream_impl_t(queue, flags)) {}
    status_t init();

    cl_command_queue create_queue(
            cl_context ctx, cl_device_id dev, cl_int *err) const;

    cl_command_queue queue_;
    std::unique_ptr<mdapi_helper_t> mdapi_helper_;
    mutable utils::thread_local_storage_t<ocl_context_t> ctx_;
};

} // namespace ocl
} // namespace intel
} // namespace gpu
} // namespace impl
} // namespace dnnl

#endif
