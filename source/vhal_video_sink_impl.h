#ifndef VHAL_VIDEO_SINK_IMPL_H
#define VHAL_VIDEO_SINK_IMPL_H
/**
 * @file vhal_video_sink_impl.h
 * @author Shakthi Prashanth M (shakthi.prashanth.m@intel.com)
 * @brief
 * @version 1.0
 * @date 2021-04-27
 *
 * Copyright (c) 2021 Intel Corporation
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "istream_socket_client.h"
#include "vhal_video_sink.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
extern "C"
{
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>
}
#include <thread>
#include <tuple>

using namespace std;
using namespace chrono_literals;

namespace vhal {
namespace client {

class VHalVideoSink::Impl
{
public:
    Impl(unique_ptr<IStreamSocketClient> socket_client)
      : socket_client_{ move(socket_client) }
    {
        vhal_talker_thread_ = thread([this]() {
            while (should_continue_) {
                if (not socket_client_->Connected()) {
                    if (auto [connected, error_msg] = socket_client_->Connect();
                        !connected) {
                        cout << "Failed to connect to VHal: " << error_msg
                             << ". Retry after 3ms...\n";
                        this_thread::sleep_for(3ms);
                    }
                }
                // connected ...
                cout << "Connected to Camera VHal!\n";

                struct pollfd fds[1];
                const int     timeout_ms = 1 * 1000; // 1 sec timeout
                int           ret;

                // watch socket for input
                fds[0].fd     = socket_client_->GetNativeSocketFd();
                fds[0].events = POLLIN;

                do {
                    ret = poll(fds, 1, timeout_ms);
                    if (ret == -1) {
                        throw system_error(errno, system_category());
                    }
                    if (!ret) {
                        continue;
                    }
                    if (fds[0].revents & POLLIN) {
                        cout << "Camera VHal has some message for us!\n";

                        VHalVideoSink::CtrlMessage ctrl_msg;

                        if (auto [received, recv_err_msg] =
                              socket_client_->Recv(
                                reinterpret_cast<uint8_t*>(&ctrl_msg),
                                sizeof(ctrl_msg));
                            received != sizeof(VHalVideoSink::CtrlMessage)) {
                            cout
                              << "Failed to read message from VHalVideoSink: "
                              << recv_err_msg
                              << ", going to disconnect and reconnect.\n";
                            socket_client_->Close();
                            continue;
                            // FIXME: What to do ?? Exit ?
                        }
                        callback_(cref(ctrl_msg));
                    }
                } while (true);
            }
        });
    }

    ~Impl()
    {
        should_continue_ = false;
        vhal_talker_thread_.join();
    }

    bool RegisterCallback(CameraCallback callback)
    {
        callback_ = move(callback);
        return true;
    }

    // atomic<bool> ShouldContinue() const { return should_continue_; }

    VHalVideoSink::IOResult WritePacket(const uint8_t* packet, size_t size)
    {
        if (auto [sent, error_msg] = socket_client_->Send(packet, size);
            sent == -1)
            return { sent, error_msg };
        // success
        return { size, "" };
    }

private:
    CameraCallback                  callback_ = nullptr;
    unique_ptr<IStreamSocketClient> socket_client_;
    thread                          vhal_talker_thread_;
    atomic<bool>                    should_continue_ = true;
};

} // namespace client
} // namespace vhal

#endif /* VHAL_VIDEO_SINK_IMPL_H */