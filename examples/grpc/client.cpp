/****************************************************************************
**
** Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

