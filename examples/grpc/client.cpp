/****************************************************************************
**
** Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of Qbs.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif  // __GNUC__

#include <ping-pong-grpc.grpc.pb.h>

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>

#ifdef __GNUC__
  #pragma GCC diagnostic pop
#endif  // __GNUC__

#include <iostream>
#include <memory>
#include <string>

class Client
{
public:
    Client(const std::shared_ptr<grpc::Channel> &channel)
        : m_stub(PP::MyApi::NewStub(channel)) {}

    int ping(int count) {
        PP::Ping request;
        request.set_count(count);
        PP::Pong reply;
        grpc::ClientContext context;

        const auto status = m_stub->pingPong(&context, request, &reply);
        if (status.ok()) {
            return reply.count();
        } else {
            throw std::runtime_error("invalid status");
        }
    }

private:
    std::unique_ptr<PP::MyApi::Stub> m_stub;
};

int main(int, char**)
{
    Client client(
            grpc::CreateCustomChannel(
                    "localhost:50051",
                    grpc::InsecureChannelCredentials(),
                    grpc::ChannelArguments()));

    for (int i = 0; i < 1000; ++i) {
        std::cout << "Sending ping " << i << "... ";
        int result = client.ping(i);
        if (result != i) {
            std::cerr << "Invalid pong " << result << " for ping" << i << std::endl;
            continue;
        }
        std::cout << "got pong " << result << std::endl;
    }

    return 0;
}

