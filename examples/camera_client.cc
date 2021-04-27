/**
 * @file camera_client.cc
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

#include "unix_stream_socket_client.h"
#include "vhal_video_sink.h"
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace vhal::client;
using namespace std;

static void
usage(string program_name)
{
    cout << "\tUsage:   " << program_name
         << " <filename> <vhal-sock-path>\n"
            "\tExample: "
         << program_name
         << " test.h265 /ipc/camdec-sock-0"
            "frames\n";
    return;
}

int
main(int argc, char** argv)
{
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }

    string       socket_path(argv[2]);
    string       filename = argv[1];
    atomic<bool> stop     = false;
    thread       file_src_thread;

    auto unix_sock_client =
      make_unique<UnixStreamSocketClient>(move(socket_path));

    VHalVideoSink vhal_video_sink(move(unix_sock_client));

    cout << "Waiting Camera Open callback..\n";
    vhal_video_sink.RegisterCallback(
      [&](const VHalVideoSink::CtrlMessage& ctrl_msg) {
          switch (ctrl_msg.cmd) {
              case VHalVideoSink::Command::kOpen:
                  cout << "Received Open command from Camera VHal\n";
                  file_src_thread = thread([&stop,
                                            &vhal_video_sink,
                                            &filename]() {
                      // open file for reading
                      fstream istrm(filename, istrm.binary | istrm.in);
                      if (!istrm.is_open()) {
                          cout << "Failed to open " << filename << '\n';
                          exit(1);
                      }
                      cout << "Will start reading from file: " << filename
                           << '\n';
                      const size_t av_input_buffer_padding_size = 64;
                      const size_t inbuf_size                   = 4 * 1024;
                      array<uint8_t, inbuf_size + av_input_buffer_padding_size>
                        inbuf;
                      while (!stop) {
                          istrm.read(reinterpret_cast<char*>(inbuf.data()),
                                     inbuf_size); // binary input
                          if (!istrm.gcount() or istrm.eof()) {
                              //   istrm.clear();
                              //   istrm.seekg(0);
                              istrm.close();
                              istrm.open(filename, istrm.binary | istrm.in);
                              if (!istrm.is_open()) {
                                  cout << "Failed to open " << filename << '\n';
                                  exit(1);
                              }
                              cout << "Closed and re-opened file: " << filename
                                   << "\n";
                              continue;
                          }

                          // Write payload size
                          if (auto [sent, error_msg] =
                                vhal_video_sink.WritePacket(
                                  (uint8_t*)&inbuf_size,
                                  sizeof(inbuf_size));
                              sent < 0) {
                              cout << "Error in writing payload size to "
                                      "Camera VHal: "
                                   << error_msg << "\n";
                              exit(1);
                          }

                          // Write payload
                          if (auto [sent, error_msg] =
                                vhal_video_sink.WritePacket(inbuf.data(),
                                                            inbuf_size);
                              sent < 0) {
                              cout
                                << "Error in writing payload to Camera VHal: "
                                << error_msg << "\n";
                              exit(1);
                          }
                          cout << "[rate=30fps] Sent " << istrm.gcount()
                               << " bytes to Camera VHal.\n";
                          // sleep for 33ms to maintain 30 fps
                          this_thread::sleep_for(33ms);
                      }
                  });
                  break;
              case VHalVideoSink::Command::kClose:
                  cout << "Received Close command from Camera VHal\n";
                  stop = false;
                  file_src_thread.join();
                  exit(0);
              default:
                  cout << "Unknown Command received, exiting with failure\n";
                  exit(1);
          }
      });

    // we need to be alive :)
    while (true) {
        this_thread::sleep_for(5ms);
    }

    return 0;
}